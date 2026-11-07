// Renders the Heltec MP3 OLED screens with the real U8g2 library + fonts,
// mirroring the drawing code in HeltecMP3.ino, to PBM files.
#include <stdio.h>
#include <string.h>
#include "u8g2.h"

static u8g2_t u8g2;
static u8g2_t *g = &u8g2;

static uint8_t nocom(u8x8_t *u, uint8_t m, uint8_t a, void *p) { return 1; }
static void save(const char *name) {
  uint8_t *buf = u8g2_GetBufferPtr(g);
  char p[128]; snprintf(p, sizeof p, "out/%s.pbm", name);
  FILE *f = fopen(p, "w"); fprintf(f, "P1\n128 64\n");
  for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fprintf(f, "%d ", (buf[(y / 8) * 128 + x] >> (y % 8) & 1) ? 1 : 0); fprintf(f, "\n"); }
  fclose(f);
}

static void header(const char *title, int vol, const char *mode, int bt) {
  u8g2_SetFont(g, u8g2_font_6x10_tf);
  u8g2_DrawStr(g, 0, 8, title);
  int x = 127; char v[8]; snprintf(v, 8, "%d", vol);
  x -= u8g2_GetStrWidth(g, v); u8g2_DrawStr(g, x, 8, v);
  x -= 8; u8g2_DrawBox(g, x, 3, 2, 4); u8g2_DrawTriangle(g, x + 2, 5, x + 5, 1, x + 5, 9);
  if (mode && *mode) { x -= u8g2_GetStrWidth(g, mode) + 3; u8g2_DrawStr(g, x, 8, mode); }
  if (bt) { x -= 8; u8g2_SetFont(g, u8g2_font_open_iconic_embedded_1x_t); u8g2_DrawGlyph(g, x, 9, 74); u8g2_SetFont(g, u8g2_font_6x10_tf); }
  u8g2_DrawHLine(g, 0, 10, 128);
}

// items: label strings; dirs: 1 = folder icon
static void list(const char **items, const int *dirs, int count, int sel, int top) {
  const int rows = 5, rowH = 10, y0 = 13;
  u8g2_SetFont(g, u8g2_font_6x10_tf);
  for (int r = 0; r < rows && top + r < count; r++) {
    int i = top + r, y = y0 + r * rowH;
    if (i == sel) { u8g2_DrawBox(g, 0, y, 122, rowH); u8g2_SetDrawColor(g, 0); }
    int tx = 2;
    if (dirs && dirs[i]) { u8g2_DrawFrame(g, 2, y + 3, 7, 5); u8g2_DrawHLine(g, 2, y + 2, 3); tx = 12; }
    u8g2_SetClipWindow(g, tx, y, 121, y + rowH);
    u8g2_DrawStr(g, tx, y + 8, items[i]);
    u8g2_SetMaxClipWindow(g);
    u8g2_SetDrawColor(g, 1);
  }
  if (count > rows) {
    int h = 50 * rows / count; if (h < 4) h = 4;
    int yy = 13 + (50 - h) * sel / (count - 1);
    u8g2_DrawVLine(g, 125, 13, 50); u8g2_DrawBox(g, 124, yy, 3, h);
  }
}

static void toast(const char *m) {
  u8g2_SetFont(g, u8g2_font_6x10_tf);
  int w = u8g2_GetStrWidth(g, m) + 8, x = (128 - w) / 2;
  u8g2_SetDrawColor(g, 0); u8g2_DrawBox(g, x - 1, 24, w + 2, 17); u8g2_SetDrawColor(g, 1);
  u8g2_DrawRFrame(g, x, 25, w, 15, 3);
  u8g2_DrawStr(g, x + 4, 36, m);
}

static void nowPlaying(const char *state, const char *title, const char *artist, float prog, const char *t, const char *pos, int playing, int bt) {
  header(state, 12, "SH", bt);
  u8g2_SetFont(g, u8g2_font_7x13B_tf);
  u8g2_SetClipWindow(g, 0, 12, 127, 27); u8g2_DrawStr(g, 0, 25, title); u8g2_SetMaxClipWindow(g);
  u8g2_SetFont(g, u8g2_font_6x10_tf);
  u8g2_DrawStr(g, 0, 38, artist);
  u8g2_DrawFrame(g, 0, 43, 128, 6); u8g2_DrawBox(g, 1, 44, (int)(126 * prog), 4);
  u8g2_DrawStr(g, 0, 62, t);
  u8g2_DrawStr(g, 128 - u8g2_GetStrWidth(g, pos), 62, pos);
  if (playing) u8g2_DrawTriangle(g, 60, 53, 60, 63, 68, 58);
  else { u8g2_DrawBox(g, 60, 53, 3, 10); u8g2_DrawBox(g, 65, 53, 3, 10); }
}

int main(void) {
  u8g2_Setup_ssd1306_128x64_noname_f(g, U8G2_R0, nocom, nocom);

  // boot splash
  u8g2_ClearBuffer(g);
  u8g2_SetFont(g, u8g2_font_7x13B_tf); u8g2_DrawStr(g, 22, 26, "Heltec MP3");
  u8g2_SetFont(g, u8g2_font_5x7_tf); u8g2_DrawStr(g, 40, 40, "v1.1.0"); u8g2_DrawStr(g, 16, 56, "Loading SD card...");
  save("01_boot");

  const char *home[] = { "Browse music", "Now playing", "Resume last", "Shuffle folder", "Bluetooth", "Wi-Fi", "Settings", "About" };
  u8g2_ClearBuffer(g); header("Heltec MP3", 12, "SH", 1); list(home, NULL, 8, 0, 0); save("02_home");

  const char *br[] = { "..", "Live at the Roxy", "01 Midnight Drive", "02 Neon Rain", "03 Afterglow", "04 Chrome Hearts" };
  int brd[] = { 1, 1, 0, 0, 0, 0 };
  u8g2_ClearBuffer(g); header("Synthwave", 12, "SH", 1); list(br, brd, 6, 2, 0); save("03_browse");

  u8g2_ClearBuffer(g); nowPlaying("Playing", "Midnight Drive", "The Night Cars", 0.42f, "1:23", "3/14", 1, 1); save("04_now_playing");

  u8g2_ClearBuffer(g); nowPlaying("Paused", "Neon Rain", "Lumen Avenue", 0.71f, "2:58", "4/14", 0, 1); save("05_paused");

  const char *ctl[] = { "Pause", "Next track", "Previous track", "Volume +  (12)", "Volume -  (12)", "Mode: Shuffle", "Back to Now playing", "Browse music" };
  u8g2_ClearBuffer(g); header("Controls", 12, "SH", 1); list(ctl, NULL, 8, 3, 0); save("06_controls");

  const char *bt[] = { "Headset: none", "Scanning... (3)", "* JBL TUNE510BT", "+ Sony WH-1000XM4", "+ Soundcore Q20", "Reconnect", "Forget saved", "Disconnect", "Back" };
  u8g2_ClearBuffer(g); header("Bluetooth", 12, "SH", 0); list(bt, NULL, 9, 3, 0); save("07_bluetooth_scan");

  u8g2_ClearBuffer(g); header("Bluetooth", 12, "SH", 0); list(bt, NULL, 9, 3, 0); toast("Connecting..."); save("08_bluetooth_connecting");

  const char *bt2[] = { "Headset: connected", "Scan for headsets", "* JBL TUNE510BT", "+ Sony WH-1000XM4", "+ Soundcore Q20", "Reconnect", "Forget saved", "Disconnect", "Back" };
  u8g2_ClearBuffer(g); header("Bluetooth", 12, "SH", 1); list(bt2, NULL, 9, 0, 0); save("09_bluetooth_connected");

  const char *wf[] = { "Wi-Fi: On", "IP: 192.168.1.57", "Net: HomeNet", "heltec-mp3.local", "Forget home Wi-Fi", "Back" };
  u8g2_ClearBuffer(g); header("Wi-Fi", 12, "SH", 1); list(wf, NULL, 6, 1, 0); save("10_wifi");

  const char *wf2[] = { "Wi-Fi: On", "IP: 192.168.4.1", "Hotspot: Heltec-MP3-3A7C", "Pass: heltecmp3", "Forget home Wi-Fi", "Back" };
  u8g2_ClearBuffer(g); header("Wi-Fi", 12, "", 0); list(wf2, NULL, 6, 2, 0); save("11_wifi_hotspot");

  const char *st[] = { "Volume +  (12)", "Volume -  (12)", "Mode: Shuffle", "Screen off: 30 sec", "Brightness: High", "Resume: On", "Re-scan SD card", "Back" };
  u8g2_ClearBuffer(g); header("Settings", 12, "SH", 1); list(st, NULL, 8, 3, 0); save("12_settings");

  // PRG hold meter on the home screen (~1 s into a hold)
  u8g2_ClearBuffer(g); header("Heltec MP3", 12, "SH", 1); list(home, NULL, 8, 4, 0);
  u8g2_SetDrawColor(g, 0); u8g2_DrawBox(g, 0, 61, 128, 3); u8g2_SetDrawColor(g, 1);
  u8g2_DrawBox(g, 0, 62, 85, 2); u8g2_DrawPixel(g, 51, 61);
  save("13_prg_hold_meter");

  u8g2_ClearBuffer(g);
  u8g2_SetFont(g, u8g2_font_7x13B_tf); u8g2_DrawStr(g, 18, 14, "No SD card");
  u8g2_SetFont(g, u8g2_font_5x7_tf);
  u8g2_DrawStr(g, 0, 26, "SCK40 MOSI41 MISO42 CS39"); u8g2_DrawStr(g, 0, 35, "3.3V + GND, FAT32 card"); u8g2_DrawStr(g, 0, 44, "Put .mp3 files on it");
  u8g2_SetFont(g, u8g2_font_6x10_tf); u8g2_DrawStr(g, 0, 60, "OK / hold PRG: retry");
  save("14_no_sd");

  u8g2_ClearBuffer(g); header("About", 12, "SH", 1);
  u8g2_SetFont(g, u8g2_font_6x10_tf);
  u8g2_DrawStr(g, 0, 23, "Heltec MP3  v1.1.0"); u8g2_DrawStr(g, 0, 34, "IP 192.168.1.57");
  u8g2_DrawStr(g, 0, 45, "PRG: tap=down 2tap=up"); u8g2_DrawStr(g, 0, 56, "hold=OK  long=back");
  save("15_about");
  return 0;
}
