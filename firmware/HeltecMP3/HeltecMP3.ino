/*
  Heltec MP3 — pocket MP3 player for the Heltec WiFi LoRa 32 V3 (ESP32-S3)
  ------------------------------------------------------------------------
  * Built-in 0.96" OLED for menus, file browser and Now Playing screen
  * Works with ONLY the PRG button, or add 4 cheap push buttons for direct control
  * Plays .mp3 from a microSD card (SPI module), shows ID3 title / artist
  * Audio out through a PCM5102A I2S DAC -> wired headphones,
    or -> KCX_BT_EMITTER module -> any Bluetooth headset
    (the ESP32-S3 has BLE only, no Classic Bluetooth A2DP, so a $3 BT
     transmitter module does the headset pairing)
  * Remembers volume, play mode, screen timeout and the last track (resume)
  * Wi-Fi web control panel: player, library + MP3 upload, Bluetooth headset
    setup (scan / connect / forget via the KCX module's serial commands),
    Wi-Fi setup and over-the-air firmware updates

  Libraries: ESP8266Audio (earlephilhower), U8g2 (olikraus)
  Board:     Heltec WiFi LoRa 32(V3) / ESP32S3 Dev Module
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Update.h>

#define FW_VERSION "1.1.0"

// ---------------- Pins (Heltec V3) ----------------
// Built in
#define PIN_OLED_SDA   17
#define PIN_OLED_SCL   18
#define PIN_OLED_RST   21
#define PIN_VEXT       36   // LOW = power to OLED / header Ve
#define PIN_LED        35
#define PIN_PRG         0   // the PRG / USER button

// microSD module (SPI, 3.3 V)
#define PIN_SD_SCK     40
#define PIN_SD_MOSI    41
#define PIN_SD_MISO    42
#define PIN_SD_CS      39

// PCM5102A I2S DAC
#define PIN_I2S_BCK    48
#define PIN_I2S_LRCK   47
#define PIN_I2S_DIN    33

// Optional external buttons (to GND, internal pull-ups)
#define PIN_BTN_UP      4
#define PIN_BTN_DOWN    5
#define PIN_BTN_OK      6
#define PIN_BTN_BACK    7

// KCX_BT_EMITTER serial control (through a 3.3V<->5V level shifter) + LINK pin
#define PIN_BT_TX       2   // ESP32 TX -> module RX
#define PIN_BT_RX       3   // module TX -> ESP32 RX
#define PIN_BT_STATUS  38   // module LINK (connection status)

// ---------------- Display ----------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, PIN_OLED_RST, PIN_OLED_SCL, PIN_OLED_SDA);

// ---------------- Audio ----------------
SPIClass sdSPI(HSPI);
AudioGeneratorMP3     *mp3   = nullptr;
AudioFileSourceSD     *fsrc  = nullptr;
AudioFileSourceID3    *id3   = nullptr;
AudioFileSourceBuffer *bsrc  = nullptr;
AudioOutputI2S        *out   = nullptr;
SemaphoreHandle_t audioMux;
Preferences prefs;

volatile bool trackEnded = false;
bool  isPlaying = false;     // a track is loaded
bool  isPaused  = false;
uint32_t playStartMs = 0, pausedAccum = 0, pauseStartMs = 0;

// ---------------- Settings ----------------
enum PlayMode { MODE_NORMAL, MODE_REPEAT_ALL, MODE_REPEAT_ONE, MODE_SHUFFLE, MODE_COUNT };
const char *modeNames[] = { "Normal", "Repeat all", "Repeat one", "Shuffle" };
const char *modeIcons[] = { "", "RA", "R1", "SH" };
int  volume      = 12;          // 0..20
int  playMode    = MODE_NORMAL;
int  screenIdx   = 1;           // index into screenTimes
const uint16_t screenTimes[] = { 0, 30, 60, 300 };   // 0 = always on (seconds)
const char *screenNames[]    = { "Always on", "30 sec", "1 min", "5 min" };
int  brightIdx   = 2;
const uint8_t brightVals[]   = { 10, 90, 255 };
const char *brightNames[]    = { "Low", "Medium", "High" };
bool resumeOn    = true;

// ---------------- File browser data ----------------
#define MAX_ENTRIES 256
struct Entry { String name; bool dir; };
Entry   entries[MAX_ENTRIES];
int     entryCount = 0;
String  curDir = "/";
int     listSel = 0, listTop = 0;

// playlist = mp3 files in the folder the track was started from
String  plDir = "/";
String  playlist[MAX_ENTRIES];
int     plCount = 0, plIndex = -1;
int     shuffleOrder[MAX_ENTRIES];
int     shufflePos = 0;

String  tagTitle, tagArtist;
char    nowFile[96] = "";

// ---------------- UI state ----------------
enum Screen { SCR_HOME, SCR_BROWSE, SCR_NOW, SCR_CONTROLS, SCR_SETTINGS, SCR_NOSD, SCR_INFO, SCR_BT, SCR_WIFI };
Screen screen = SCR_HOME;
int  menuSel = 0, menuTop = 0;
uint32_t lastInputMs = 0;
bool displayAsleep = false;
String toastMsg; uint32_t toastUntil = 0;
bool sdOk = false;
bool extButtons = false;

// ---------------- Input events ----------------
enum Ev { EV_NONE, EV_UP, EV_DOWN, EV_OK, EV_BACK, EV_UP_LONG, EV_DOWN_LONG, EV_OK_LONG };
struct InputEvent { Ev ev; bool prg; };   // prg = came from the single PRG button

struct Btn {
  uint8_t pin; bool repeat;
  bool down = false, longFired = false;
  uint32_t t0 = 0, lastRep = 0, lastChange = 0;
};
Btn bUp{PIN_BTN_UP, true}, bDown{PIN_BTN_DOWN, true}, bOk{PIN_BTN_OK, false}, bBack{PIN_BTN_BACK, false};

// Generic external button: click on release; long (600ms) on hold; UP/DOWN auto-repeat
Ev pollBtn(Btn &b, Ev shortEv, Ev longEv) {
  bool pressed = digitalRead(b.pin) == LOW;
  uint32_t now = millis();
  if (pressed != b.down && now - b.lastChange > 25) {
    b.lastChange = now; b.down = pressed;
    if (pressed) { b.t0 = now; b.longFired = false; b.lastRep = now; extButtons = true; }
    else if (!b.longFired) return shortEv;
    return EV_NONE;
  }
  if (b.down && !b.longFired && now - b.t0 > 600) {
    b.longFired = true;
    return longEv;
  }
  return EV_NONE;
}

// PRG button: click, double-click, long (0.6s) and extra-long (1.5s)
struct {
  bool down = false; uint32_t t0 = 0, lastChange = 0, releaseAt = 0;
  int clicks = 0; bool longSent = false, xlongSent = false;
} prg;
enum PrgEv { P_NONE, P_CLICK, P_DOUBLE, P_LONG, P_XLONG };
PrgEv pollPrg() {
  bool pressed = digitalRead(PIN_PRG) == LOW;
  uint32_t now = millis();
  if (pressed != prg.down && now - prg.lastChange > 25) {
    prg.lastChange = now; prg.down = pressed;
    if (pressed) { prg.t0 = now; prg.longSent = prg.xlongSent = false; }
    else {
      uint32_t held = now - prg.t0;
      if (prg.xlongSent) { prg.clicks = 0; return P_NONE; }
      if (held >= 600)   { prg.clicks = 0; return P_LONG; }   // hold & release = OK
      prg.clicks++; prg.releaseAt = now;
      if (prg.clicks >= 2) { prg.clicks = 0; return P_DOUBLE; }
    }
    return P_NONE;
  }
  if (prg.down && !prg.xlongSent && now - prg.t0 > 1500) { prg.xlongSent = true; prg.clicks = 0; return P_XLONG; }
  if (!prg.down && prg.clicks == 1 && now - prg.releaseAt > 280) { prg.clicks = 0; return P_CLICK; }
  return P_NONE;
}
// how far into a PRG hold we are (for the on-screen hold meter)
uint32_t prgHeldMs() { return (prg.down && !prg.xlongSent) ? millis() - prg.t0 : 0; }


uint32_t elapsedMs();

// ---------------- Helpers ----------------
void toast(const String &m, uint16_t ms = 1200) { toastMsg = m; toastUntil = millis() + ms; }
void lockA()   { xSemaphoreTake(audioMux, portMAX_DELAY); }
void unlockA() { xSemaphoreGive(audioMux); }

bool isMp3(const String &n) {
  String l = n; l.toLowerCase();
  return l.endsWith(".mp3");
}
String joinPath(const String &dir, const String &name) {
  return dir.endsWith("/") ? dir + name : dir + "/" + name;
}
String baseName(const String &p) {
  int i = p.lastIndexOf('/'); return i >= 0 ? p.substring(i + 1) : p;
}
String stripExt(const String &n) {
  int i = n.lastIndexOf('.'); return i > 0 ? n.substring(0, i) : n;
}
#include "bt.h"

void applyVolume() { if (out) out->SetGain((volume / 20.0f) * 1.2f); }

void saveSettings() {
  prefs.putInt("vol", volume);
  prefs.putInt("mode", playMode);
  prefs.putInt("scr", screenIdx);
  prefs.putInt("bri", brightIdx);
  prefs.putBool("res", resumeOn);
}
void loadSettings() {
  volume    = constrain(prefs.getInt("vol", 12), 0, 20);
  playMode  = constrain(prefs.getInt("mode", 0), 0, MODE_COUNT - 1);
  screenIdx = constrain(prefs.getInt("scr", 1), 0, 3);
  brightIdx = constrain(prefs.getInt("bri", 2), 0, 2);
  resumeOn  = prefs.getBool("res", true);
}

// ---------------- SD ----------------
bool mountSD() {
  lockA();
  SD.end();
  sdSPI.end();
  sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  bool ok = SD.begin(PIN_SD_CS, sdSPI, 20000000);
  if (!ok) ok = SD.begin(PIN_SD_CS, sdSPI, 4000000);   // slower retry for long wires
  unlockA();
  return ok && SD.cardType() != CARD_NONE;
}

void sortEntries() {
  // folders first, then alphabetical (simple insertion sort, n <= 256)
  for (int i = 1; i < entryCount; i++) {
    Entry k = entries[i]; int j = i - 1;
    auto less = [](const Entry &a, const Entry &b) {
      if (a.dir != b.dir) return a.dir;
      return a.name.compareTo(b.name) < 0 ? true : false;
    };
    while (j >= 0 && less(k, entries[j])) { entries[j + 1] = entries[j]; j--; }
    entries[j + 1] = k;
  }
}

void loadDir(const String &dir) {
  entryCount = 0;
  lockA();
  File d = SD.open(dir);
  if (d && d.isDirectory()) {
    File f;
    while ((f = d.openNextFile()) && entryCount < MAX_ENTRIES) {
      String n = baseName(String(f.name()));
      bool isDir = f.isDirectory();
      f.close();
      if (n.startsWith(".") || n == "System Volume Information") continue;
      if (isDir || isMp3(n)) { entries[entryCount].name = n; entries[entryCount].dir = isDir; entryCount++; }
    }
    d.close();
  }
  unlockA();
  sortEntries();
  curDir = dir; listSel = 0; listTop = 0;
}

void buildPlaylistFromCurrent() {
  plDir = curDir; plCount = 0;
  for (int i = 0; i < entryCount; i++) if (!entries[i].dir) playlist[plCount++] = entries[i].name;
}
void buildShuffle(int startIdx) {
  for (int i = 0; i < plCount; i++) shuffleOrder[i] = i;
  for (int i = plCount - 1; i > 0; i--) { int j = esp_random() % (i + 1); int t = shuffleOrder[i]; shuffleOrder[i] = shuffleOrder[j]; shuffleOrder[j] = t; }
  // put the chosen track first
  for (int i = 0; i < plCount; i++) if (shuffleOrder[i] == startIdx) { shuffleOrder[i] = shuffleOrder[0]; shuffleOrder[0] = startIdx; break; }
  shufflePos = 0;
}

// ---------------- Playback ----------------
void id3Callback(void *, const char *type, bool isUnicode, const char *str) {
  if (isUnicode || !str || !*str) return;
  if (!strcmp(type, "Title"))  tagTitle  = String(str);
  else if (!strcmp(type, "Artist") || !strcmp(type, "Band")) { if (tagArtist.isEmpty()) tagArtist = String(str); }
}

void stopTrackLocked() {
  if (mp3) { if (mp3->isRunning()) mp3->stop(); delete mp3; mp3 = nullptr; }
  if (bsrc) { delete bsrc; bsrc = nullptr; }
  if (id3)  { delete id3;  id3  = nullptr; }
  if (fsrc) { delete fsrc; fsrc = nullptr; }
}

void stopTrack() {
  lockA(); stopTrackLocked(); unlockA();
  isPlaying = false; isPaused = false;
}

bool playIndex(int idx) {
  if (idx < 0 || idx >= plCount) return false;
  String path = joinPath(plDir, playlist[idx]);
  lockA();
  stopTrackLocked();
  tagTitle = ""; tagArtist = "";
  fsrc = new AudioFileSourceSD(path.c_str());
  if (!fsrc->isOpen()) { stopTrackLocked(); unlockA(); toast("Can't open file"); isPlaying = false; return false; }
  id3  = new AudioFileSourceID3(fsrc);
  id3->RegisterMetadataCB(id3Callback, nullptr);
  bsrc = new AudioFileSourceBuffer(id3, 4096);
  mp3  = new AudioGeneratorMP3();
  applyVolume();
  bool ok = mp3->begin(bsrc, out);
  trackEnded = false;
  unlockA();
  if (!ok) { stopTrack(); toast("Bad MP3"); return false; }
  plIndex = idx; isPlaying = true; isPaused = false;
  playStartMs = millis(); pausedAccum = 0;
  strlcpy(nowFile, playlist[idx].c_str(), sizeof(nowFile));
  if (resumeOn) { prefs.putString("rdir", plDir); prefs.putString("rfile", playlist[idx]); }
  digitalWrite(PIN_LED, HIGH);
  return true;
}

void togglePause() {
  if (!isPlaying) return;
  isPaused = !isPaused;
  if (isPaused) pauseStartMs = millis(); else pausedAccum += millis() - pauseStartMs;
  digitalWrite(PIN_LED, isPaused ? LOW : HIGH);
}

int nextIndex(bool userPressed) {
  if (plCount == 0) return -1;
  if (playMode == MODE_REPEAT_ONE && !userPressed) return plIndex;
  if (playMode == MODE_SHUFFLE) {
    shufflePos++;
    if (shufflePos >= plCount) buildShuffle(-1);   // new random order each lap
    return shuffleOrder[shufflePos];
  }
  int n = plIndex + 1;
  if (n >= plCount) return (playMode == MODE_NORMAL && !userPressed) ? -1 : 0;
  return n;
}
int prevIndex() {
  if (plCount == 0) return -1;
  if (playMode == MODE_SHUFFLE) { if (shufflePos > 0) shufflePos--; return shuffleOrder[shufflePos]; }
  return plIndex > 0 ? plIndex - 1 : plCount - 1;
}
void nextTrack(bool user) {
  int n = nextIndex(user);
  if (n < 0) { stopTrack(); digitalWrite(PIN_LED, LOW); toast("End of folder"); return; }
  playIndex(n);
}
void prevTrack() {
  // restart current if >3s in, like every MP3 player ever
  if (isPlaying && elapsedMs() > 3000) { playIndex(plIndex); return; }
  int p = prevIndex(); if (p >= 0) playIndex(p);
}

uint32_t elapsedMs() {
  if (!isPlaying) return 0;
  uint32_t now = isPaused ? pauseStartMs : millis();
  return now - playStartMs - pausedAccum;
}

// Audio runs on core 0 so the display never causes stutter
void audioTask(void *) {
  for (;;) {
    bool worked = false;
    lockA();
    if (mp3 && mp3->isRunning() && !isPaused) {
      if (!mp3->loop()) { mp3->stop(); trackEnded = true; }
      worked = true;
    }
    unlockA();
    vTaskDelay(worked ? 1 : pdMS_TO_TICKS(10));   // always yield so Wi-Fi / UI get CPU
  }
}

// ---------------- Drawing ----------------
void drawHeader(const char *title) {
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 8, title);
  // right side status: BT, mode, volume
  int x = 127;
  char v[8]; snprintf(v, sizeof(v), "%d", volume);
  x -= oled.getStrWidth(v); oled.drawStr(x, 8, v);
  // little speaker
  x -= 8; oled.drawBox(x, 3, 2, 4); oled.drawTriangle(x + 2, 5, x + 5, 1, x + 5, 9);
  if (modeIcons[playMode][0]) { x -= oled.getStrWidth(modeIcons[playMode]) + 3; oled.drawStr(x, 8, modeIcons[playMode]); }
  if (btConnected()) { x -= 8; oled.setFont(u8g2_font_open_iconic_embedded_1x_t); oled.drawGlyph(x, 9, 74); oled.setFont(u8g2_font_6x10_tf); }
  oled.drawHLine(0, 10, 128);
}

// Draws a scrolling list. labels via callback.
typedef String (*LabelFn)(int i, bool &isDir);
void drawList(int count, int sel, int &top, LabelFn label) {
  const int rows = 5, rowH = 10, y0 = 13;
  if (sel < top) top = sel;
  if (sel >= top + rows) top = sel - rows + 1;
  oled.setFont(u8g2_font_6x10_tf);
  for (int r = 0; r < rows && top + r < count; r++) {
    int i = top + r; int y = y0 + r * rowH;
    bool isDir = false; String s = label(i, isDir);
    if (i == sel) { oled.drawBox(0, y, 122, rowH); oled.setDrawColor(0); }
    int tx = 2;
    if (isDir) { oled.drawFrame(2, y + 3, 7, 5); oled.drawHLine(2, y + 2, 3); tx = 12; }
    // marquee selected long names
    int w = oled.getStrWidth(s.c_str()); int maxW = 120 - tx;
    int off = 0;
    if (i == sel && w > maxW) { int span = w - maxW + 20; off = ((millis() / 40) % (span + 40)); off = off < 20 ? 0 : min(off - 20, w - maxW); }
    oled.setClipWindow(tx, y, 121, y + rowH);
    oled.drawStr(tx - off, y + 8, s.c_str());
    oled.setMaxClipWindow();
    oled.setDrawColor(1);
  }
  // scrollbar
  if (count > rows) {
    int h = max(4, 50 * rows / count); int yy = 13 + (50 - h) * sel / max(1, count - 1);
    oled.drawVLine(125, 13, 50); oled.drawBox(124, yy, 3, h);
  }
  if (count == 0) { oled.drawStr(20, 38, "(empty)"); }
}

void startFromBrowser(int fileIdxInEntries);
#include "web.h"

// ---- menus ----
const char *homeItems[] = { "Browse music", "Now playing", "Resume last", "Shuffle folder", "Bluetooth", "Wi-Fi", "Settings", "About" };
const int homeCount = 8;
String homeLabel(int i, bool &d) { d = false; return homeItems[i]; }

String browseLabel(int i, bool &d) {
  if (i == 0 && curDir != "/") { d = true; return ".."; }
  int k = curDir != "/" ? i - 1 : i;
  d = entries[k].dir;
  return d ? entries[k].name : stripExt(entries[k].name);
}
int browseCount() { return entryCount + (curDir != "/" ? 1 : 0); }

String ctrlLabel(int i, bool &d) {
  d = false;
  switch (i) {
    case 0: return isPaused || !isPlaying ? "Play" : "Pause";
    case 1: return "Next track";
    case 2: return "Previous track";
    case 3: return "Volume +  (" + String(volume) + ")";
    case 4: return "Volume -  (" + String(volume) + ")";
    case 5: return String("Mode: ") + modeNames[playMode];
    case 6: return "Back to Now playing";
    case 7: return "Browse music";
  }
  return "";
}
const int ctrlCount = 8;

String setLabel(int i, bool &d) {
  d = false;
  switch (i) {
    case 0: return "Volume +  (" + String(volume) + ")";
    case 1: return "Volume -  (" + String(volume) + ")";
    case 2: return String("Mode: ") + modeNames[playMode];
    case 3: return String("Screen off: ") + screenNames[screenIdx];
    case 4: return String("Brightness: ") + brightNames[brightIdx];
    case 5: return String("Resume: ") + (resumeOn ? "On" : "Off");
    case 6: return "Re-scan SD card";
    case 7: return "Back";
  }
  return "";
}
const int setCount = 8;

// Bluetooth screen: status, scan, found headsets, actions
int btItemCount() { return 2 + btFoundN + 4; }
String btLabel(int i, bool &d) {
  d = false;
  if (i == 0) return !btUart ? String("Module: not wired") : (btConnected() ? String("Headset: connected") : String("Headset: none"));
  if (i == 1) return btScanning ? String("Scanning... (") + btFoundN + ")" : String("Scan for headsets");
  int k = i - 2;
  if (k < btFoundN) {
    String n = btFound[k].name.length() ? btFound[k].name : btFound[k].mac;
    return (btIsSaved(btFound[k].mac) ? "* " : "+ ") + n;
  }
  k -= btFoundN;
  const char *t[] = { "Reconnect", "Forget saved", "Disconnect", "Back" };
  return t[k];
}

// Wi-Fi screen
const int wifiCount = 6;
String wifiLabel(int i, bool &d) {
  d = false;
  bool sta = WiFi.status() == WL_CONNECTED;
  switch (i) {
    case 0: return String("Wi-Fi: ") + (wifiOn ? "On" : "Off");
    case 1: { String ip = wifiIP(); return "IP: " + (wifiOn ? (ip.length() ? ip : String("connecting...")) : String("-")); }
    case 2: return sta ? "Net: " + WiFi.SSID() : (apActive ? "Hotspot: " + apName : (wifiSsid.length() ? "Joining: " + wifiSsid : String("Net: none")));
    case 3: return apActive && !sta ? String("Pass: heltecmp3") : String("heltec-mp3.local");
    case 4: return "Forget home Wi-Fi";
    case 5: return "Back";
  }
  return "";
}

void drawNowPlaying() {
  drawHeader(isPlaying ? (isPaused ? "Paused" : "Playing") : "Stopped");
  oled.setFont(u8g2_font_7x13B_tf);
  String title = tagTitle.length() ? tagTitle : (nowFile[0] ? stripExt(String(nowFile)) : String("Nothing playing"));
  int w = oled.getStrWidth(title.c_str());
  int off = 0;
  if (w > 128) { int span = w - 128; off = (millis() / 35) % (span + 80); off = off < 40 ? 0 : min(off - 40, span); }
  oled.setClipWindow(0, 12, 127, 27);
  oled.drawStr(-off, 25, title.c_str());
  oled.setMaxClipWindow();

  oled.setFont(u8g2_font_6x10_tf);
  String sub = tagArtist.length() ? tagArtist : (plCount ? String(plIndex + 1) + " of " + String(plCount) : String(""));
  oled.drawStr(0, 38, sub.c_str());

  // progress by bytes read
  float prog = 0;
  lockA();
  if (isPlaying && fsrc && fsrc->getSize() > 0) prog = (float)fsrc->getPos() / fsrc->getSize();
  unlockA();
  oled.drawFrame(0, 43, 128, 6);
  oled.drawBox(1, 44, (int)(126 * constrain(prog, 0.f, 1.f)), 4);

  uint32_t s = elapsedMs() / 1000; char t[16];
  snprintf(t, sizeof(t), "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
  oled.drawStr(0, 62, t);
  if (plCount && tagArtist.length()) { String n = String(plIndex + 1) + "/" + String(plCount); oled.drawStr(128 - oled.getStrWidth(n.c_str()), 62, n.c_str()); }
  // play / pause glyph center
  if (isPlaying && !isPaused) oled.drawTriangle(60, 53, 60, 63, 68, 58);
  else { oled.drawBox(60, 53, 3, 10); oled.drawBox(65, 53, 3, 10); }
}

void drawAbout() {
  drawHeader("About");
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 23, "Heltec MP3  v" FW_VERSION);
  oled.drawStr(0, 34, wifiIP().length() ? ("IP " + wifiIP()).c_str() : "Heltec V3 ESP32-S3");
  oled.drawStr(0, 45, "PRG: tap=down 2tap=up");
  oled.drawStr(0, 56, "hold=OK  hold 1.5s=back");
}

void drawNoSD() {
  oled.setFont(u8g2_font_7x13B_tf);
  oled.drawStr(18, 14, "No SD card");
  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(0, 26, "SCK40 MOSI41 MISO42 CS39");
  oled.drawStr(0, 35, "3.3V + GND, FAT32 card");
  oled.drawStr(0, 44, "Put .mp3 files on it");
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 60, "OK / hold PRG: retry");
}

void render() {
  if (displayAsleep || otaRunning) return;
  oled.clearBuffer();
  switch (screen) {
    case SCR_HOME:     drawHeader("Heltec MP3"); drawList(homeCount, menuSel, menuTop, homeLabel); break;
    case SCR_BROWSE: {
      String t = curDir == "/" ? String("Music") : baseName(curDir);
      if (t.length() > 11) t = t.substring(0, 10) + "~";
      drawHeader(t.c_str()); drawList(browseCount(), listSel, listTop, browseLabel); break; }
    case SCR_NOW:      drawNowPlaying(); break;
    case SCR_CONTROLS: drawHeader("Controls"); drawList(ctrlCount, menuSel, menuTop, ctrlLabel); break;
    case SCR_SETTINGS: drawHeader("Settings"); drawList(setCount, menuSel, menuTop, setLabel); break;
    case SCR_NOSD:     drawNoSD(); break;
    case SCR_INFO:     drawAbout(); break;
    case SCR_BT:       drawHeader("Bluetooth"); drawList(btItemCount(), menuSel, menuTop, btLabel); break;
    case SCR_WIFI:     drawHeader("Wi-Fi"); drawList(wifiCount, menuSel, menuTop, wifiLabel); break;
  }
  // PRG hold meter: fills to "OK" at 0.6s, then to "BACK" at 1.5s
  uint32_t h = prgHeldMs();
  if (h > 150) {
    int w = min(128, (int)(h * 128 / 1500));
    oled.setDrawColor(0); oled.drawBox(0, 61, 128, 3); oled.setDrawColor(1);
    oled.drawBox(0, 62, w, 2);
    oled.drawPixel(51, 61);   // 0.6s mark
  }
  if (millis() < toastUntil) {
    oled.setFont(u8g2_font_6x10_tf);
    int w = oled.getStrWidth(toastMsg.c_str()) + 8;
    int x = (128 - w) / 2;
    oled.setDrawColor(0); oled.drawBox(x - 1, 24, w + 2, 17); oled.setDrawColor(1);
    oled.drawRFrame(x, 25, w, 15, 3);
    oled.drawStr(x + 4, 36, toastMsg.c_str());
  }
  oled.sendBuffer();
}

// ---------------- Actions ----------------
void goScreen(Screen s) { screen = s; menuSel = 0; menuTop = 0; }

void openBrowser() {
  if (!sdOk) { goScreen(SCR_NOSD); return; }
  screen = SCR_BROWSE;
  if (entryCount == 0 && curDir == "/") loadDir("/");
}

void startFromBrowser(int fileIdxInEntries) {
  buildPlaylistFromCurrent();
  String name = entries[fileIdxInEntries].name;
  int idx = 0;
  for (int i = 0; i < plCount; i++) if (playlist[i] == name) { idx = i; break; }
  if (playMode == MODE_SHUFFLE) buildShuffle(idx);
  if (playIndex(idx)) screen = SCR_NOW;
}

void resumeLast() {
  String d = prefs.getString("rdir", ""), f = prefs.getString("rfile", "");
  if (!sdOk || d.isEmpty() || f.isEmpty()) { toast("Nothing to resume"); return; }
  loadDir(d);
  for (int i = 0; i < entryCount; i++) if (!entries[i].dir && entries[i].name == f) {
    startFromBrowser(i);
    listSel = i + (curDir != "/" ? 1 : 0);
    return;
  }
  toast("Track not found");
}

void shuffleFolder() {
  if (!sdOk) { goScreen(SCR_NOSD); return; }
  if (entryCount == 0) loadDir(curDir);
  buildPlaylistFromCurrent();
  if (plCount == 0) { toast("No MP3s here"); return; }
  playMode = MODE_SHUFFLE; saveSettings();
  buildShuffle(esp_random() % plCount);
  if (playIndex(shuffleOrder[0])) screen = SCR_NOW;
}

void changeVolume(int d) {
  volume = constrain(volume + d, 0, 20);
  lockA(); applyVolume(); unlockA();
  prefs.putInt("vol", volume);
}
void cycleMode() {
  playMode = (playMode + 1) % MODE_COUNT;
  if (playMode == MODE_SHUFFLE && plCount) buildShuffle(plIndex);
  saveSettings(); toast(modeNames[playMode]);
}

void moveSel(int &sel, int count, int d) {
  if (count == 0) return;
  sel += d;
  if (sel < 0) sel = count - 1;
  if (sel >= count) sel = 0;
}

void handle(InputEvent e) {
  Ev ev = e.ev;
  switch (screen) {
    case SCR_HOME:
      if (ev == EV_UP || ev == EV_UP_LONG) moveSel(menuSel, homeCount, -1);
      else if (ev == EV_DOWN || ev == EV_DOWN_LONG) moveSel(menuSel, homeCount, 1);
      else if (ev == EV_OK) {
        switch (menuSel) {
          case 0: openBrowser(); break;
          case 1: screen = SCR_NOW; break;
          case 2: resumeLast(); break;
          case 3: shuffleFolder(); break;
          case 4: goScreen(SCR_BT); if (btUart) btRefreshSaved(); break;
          case 5: goScreen(SCR_WIFI); break;
          case 6: goScreen(SCR_SETTINGS); break;
          case 7: screen = SCR_INFO; break;
        }
      }
      break;

    case SCR_BROWSE: {
      int cnt = browseCount();
      if (ev == EV_UP) moveSel(listSel, cnt, -1);
      else if (ev == EV_DOWN) moveSel(listSel, cnt, 1);
      else if (ev == EV_UP_LONG) listSel = max(0, listSel - 5);
      else if (ev == EV_DOWN_LONG) listSel = min(cnt - 1, listSel + 5);
      else if (ev == EV_OK && cnt) {
        bool up = (curDir != "/" && listSel == 0);
        if (up) { String p = curDir.substring(0, curDir.lastIndexOf('/')); loadDir(p.isEmpty() ? "/" : p); break; }
        int k = curDir != "/" ? listSel - 1 : listSel;
        if (entries[k].dir) loadDir(joinPath(curDir, entries[k].name));
        else startFromBrowser(k);
      } else if (ev == EV_BACK) {
        if (curDir == "/") screen = SCR_HOME;
        else { String p = curDir.substring(0, curDir.lastIndexOf('/')); loadDir(p.isEmpty() ? "/" : p); }
      } else if (ev == EV_OK_LONG) screen = isPlaying ? SCR_NOW : SCR_HOME;
      break; }

    case SCR_NOW:
      if (e.prg) {
        // single-button mapping on Now Playing
        if (ev == EV_DOWN) nextTrack(true);           // tap = next
        else if (ev == EV_UP) togglePause();          // double tap = play/pause
        else if (ev == EV_OK) goScreen(SCR_CONTROLS); // hold = controls menu
        else if (ev == EV_BACK) screen = SCR_HOME;    // hold 2s = home
      } else {
        if (ev == EV_UP) changeVolume(1);
        else if (ev == EV_DOWN) changeVolume(-1);
        else if (ev == EV_OK) { if (isPlaying) togglePause(); else if (plCount) playIndex(max(0, plIndex)); }
        else if (ev == EV_UP_LONG) nextTrack(true);
        else if (ev == EV_DOWN_LONG) prevTrack();
        else if (ev == EV_OK_LONG) goScreen(SCR_CONTROLS);
        else if (ev == EV_BACK) openBrowser();
      }
      break;

    case SCR_CONTROLS:
      if (ev == EV_UP || ev == EV_UP_LONG) moveSel(menuSel, ctrlCount, -1);
      else if (ev == EV_DOWN || ev == EV_DOWN_LONG) moveSel(menuSel, ctrlCount, 1);
      else if (ev == EV_BACK) screen = SCR_NOW;
      else if (ev == EV_OK) {
        switch (menuSel) {
          case 0: if (isPlaying) togglePause(); else if (plCount) playIndex(max(0, plIndex)); break;
          case 1: nextTrack(true); break;
          case 2: prevTrack(); break;
          case 3: changeVolume(1); break;
          case 4: changeVolume(-1); break;
          case 5: cycleMode(); break;
          case 6: screen = SCR_NOW; break;
          case 7: openBrowser(); break;
        }
      }
      break;

    case SCR_SETTINGS:
      if (ev == EV_UP || ev == EV_UP_LONG) moveSel(menuSel, setCount, -1);
      else if (ev == EV_DOWN || ev == EV_DOWN_LONG) moveSel(menuSel, setCount, 1);
      else if (ev == EV_BACK) goScreen(SCR_HOME);
      else if (ev == EV_OK) {
        switch (menuSel) {
          case 0: changeVolume(1); break;
          case 1: changeVolume(-1); break;
          case 2: cycleMode(); break;
          case 3: screenIdx = (screenIdx + 1) % 4; saveSettings(); break;
          case 4: brightIdx = (brightIdx + 1) % 3; oled.setContrast(brightVals[brightIdx]); saveSettings(); break;
          case 5: resumeOn = !resumeOn; saveSettings(); break;
          case 6: stopTrack(); sdOk = mountSD(); entryCount = 0; curDir = "/";
                  toast(sdOk ? "SD card OK" : "No SD card"); break;
          case 7: goScreen(SCR_HOME); menuSel = 6; break;
        }
      }
      break;

    case SCR_NOSD:
      if (ev == EV_OK) {
        toast("Mounting..."); render();
        sdOk = mountSD();
        if (sdOk) { loadDir("/"); screen = SCR_BROWSE; toast("SD card OK"); }
        else toast("Still no card");
      } else if (ev == EV_BACK) goScreen(SCR_HOME);
      break;

    case SCR_INFO:
      if (ev != EV_NONE) screen = SCR_HOME;
      break;

    case SCR_BT: {
      int cnt = btItemCount();
      if (ev == EV_UP || ev == EV_UP_LONG) moveSel(menuSel, cnt, -1);
      else if (ev == EV_DOWN || ev == EV_DOWN_LONG) moveSel(menuSel, cnt, 1);
      else if (ev == EV_BACK) { goScreen(SCR_HOME); menuSel = 4; }
      else if (ev == EV_OK) {
        int i = menuSel, k = i - 2 - btFoundN;
        if (i == cnt - 1) { goScreen(SCR_HOME); menuSel = 4; break; }
        if (!btUart) {
          if (i == 0) { btBegin(); toast(btUart ? "Module found" : "No reply - check wires", 1800); }
          else toast("Wire module TX/RX", 1800);
          break;
        }
        if (i == 0) { btCmd("AT+STATUS?"); toast(btVer.length() ? btVer : String("KCX module")); }
        else if (i == 1) { btScan(); toast("Scanning ~15 s", 1500); }
        else if (i - 2 < btFoundN) { btConnectTo(btFound[i - 2].mac); toast("Connecting...", 1500); }
        else if (k == 0) { btReconnect(); toast("Searching...", 1500); }
        else if (k == 1) { btForgetAll(); toast("Forgot all", 1500); }
        else if (k == 2) { btDisconnect(); toast("Disconnected", 1500); }
      }
      break; }

    case SCR_WIFI:
      if (ev == EV_UP || ev == EV_UP_LONG) moveSel(menuSel, wifiCount, -1);
      else if (ev == EV_DOWN || ev == EV_DOWN_LONG) moveSel(menuSel, wifiCount, 1);
      else if (ev == EV_BACK) { goScreen(SCR_HOME); menuSel = 5; }
      else if (ev == EV_OK) {
        if (menuSel == 0) { wifiOn = !wifiOn; prefs.putBool("wifi", wifiOn); wifiStart(); toast(wifiOn ? "Wi-Fi on" : "Wi-Fi off"); }
        else if (menuSel == 4) {
          wifiSsid = ""; wifiPass = ""; prefs.putString("wssid", ""); prefs.putString("wpass", "");
          if (wifiOn) { WiFi.disconnect(); wifiStart(); }
          toast("Hotspot mode", 1500);
        }
        else if (menuSel == 5) { goScreen(SCR_HOME); menuSel = 5; }
      }
      break;
  }
}

// Collect input from PRG + external buttons
InputEvent readInput() {
  switch (pollPrg()) {
    case P_CLICK:  return {EV_DOWN, true};
    case P_DOUBLE: return {EV_UP, true};
    case P_LONG:   return {EV_OK, true};
    case P_XLONG:  return {EV_BACK, true};
    default: break;
  }
  Ev e;
  if ((e = pollBtn(bUp,   EV_UP,   EV_UP_LONG))   != EV_NONE) return {e, false};
  if ((e = pollBtn(bDown, EV_DOWN, EV_DOWN_LONG)) != EV_NONE) return {e, false};
  if ((e = pollBtn(bOk,   EV_OK,   EV_OK_LONG))   != EV_NONE) return {e, false};
  if ((e = pollBtn(bBack, EV_BACK, EV_BACK))      != EV_NONE) return {e, false};
  // auto-repeat for held UP / DOWN in lists (after the long-press fires)
  uint32_t now = millis();
  if (screen != SCR_NOW) {
    if (bUp.down && bUp.longFired && now - bUp.lastRep > 120)     { bUp.lastRep = now;   return {EV_UP, false}; }
    if (bDown.down && bDown.longFired && now - bDown.lastRep > 120) { bDown.lastRep = now; return {EV_DOWN, false}; }
  }
  return {EV_NONE, false};
}

// ---------------- Setup / loop ----------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_VEXT, OUTPUT); digitalWrite(PIN_VEXT, LOW);   // power the OLED
  pinMode(PIN_LED, OUTPUT);  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_PRG, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP); pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP); pinMode(PIN_BTN_BACK, INPUT_PULLUP);
  delay(50);

  prefs.begin("heltecmp3", false);
  loadSettings();

  oled.begin();
  oled.setBusClock(800000);
  oled.setContrast(brightVals[brightIdx]);
  oled.clearBuffer();
  oled.setFont(u8g2_font_7x13B_tf);
  oled.drawStr(22, 26, "Heltec MP3");
  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(40, 40, "v" FW_VERSION);
  oled.drawStr(16, 56, "Loading SD card...");
  oled.sendBuffer();

  audioMux = xSemaphoreCreateMutex();
  out = new AudioOutputI2S();
  out->SetPinout(PIN_I2S_BCK, PIN_I2S_LRCK, PIN_I2S_DIN);
  applyVolume();
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 3, nullptr, 1);   // Wi-Fi lives on core 0

  btBegin();
  wifiOn   = prefs.getBool("wifi", true);
  wifiSsid = prefs.getString("wssid", "");
  wifiPass = prefs.getString("wpass", "");
  wifiStart();

  sdOk = mountSD();
  if (sdOk) { loadDir("/"); screen = SCR_HOME; }
  else screen = SCR_NOSD;
  lastInputMs = millis();
  Serial.printf("Heltec MP3 %s  SD:%s  BT module:%s\n", FW_VERSION, sdOk ? "ok" : "missing", btUart ? "ok" : "not found");
}

uint32_t lastDraw = 0;
void loop() {
  InputEvent e = readInput();
  if (e.ev != EV_NONE) {
    lastInputMs = millis();
    if (displayAsleep) { displayAsleep = false; oled.setPowerSave(0); }  // first press just wakes
    else handle(e);
    lastDraw = 0;
  }

  btPoll();
  wifiTick();
  if (libChanged) {      // files changed from the web page: refresh the on-screen list
    libChanged = false;
    if (sdOk) { int s = listSel; loadDir(curDir); listSel = min(s, max(0, browseCount() - 1)); }
  }
  if (trackEnded) { trackEnded = false; nextTrack(false); }

  // screen timeout
  uint16_t to = screenTimes[screenIdx];
  if (to && !displayAsleep && millis() - lastInputMs > to * 1000UL) { displayAsleep = true; oled.setPowerSave(1); }

  if (!displayAsleep && millis() - lastDraw > 50) { lastDraw = millis(); render(); }
  delay(2);
}
