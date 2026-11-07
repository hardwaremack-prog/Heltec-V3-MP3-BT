# Heltec MP3

A pocket MP3 player for the **Heltec WiFi LoRa 32 V3** (ESP32-S3), with:

- **OLED menus** on the built-in 0.96" screen: file browser, Now Playing (title, artist, progress, time), settings
- **Controls** with only the **PRG button**, or 4 optional add-on buttons
- **microSD** playback with ID3 tags, folders, shuffle and repeat, plus resume of the last track
- **Wired headphones** through a PCM5102A I2S DAC
- **Bluetooth headsets** through a KCX_BT_EMITTER module, with scan / connect / forget from the OLED or the web page
- **Wi-Fi control panel** at `http://heltec-mp3.local`: player, library with **MP3 upload**, Bluetooth, Wi-Fi setup and wireless firmware updates
- **One-file browser flasher**: open `flasher/index.html` in Chrome or Edge. The firmware is embedded, so it works offline.

> **Why a Bluetooth module?** The ESP32-S3 only has Bluetooth Low Energy. Headsets stream music over Classic Bluetooth (A2DP), which the S3 doesn't have. The KCX_BT_EMITTER (a few dollars) handles the headset, and the player controls it over serial "AT" commands.

## Flashing

1. Open **`flasher/index.html`** in Chrome or Edge on a computer. It also works from GitHub Pages.
2. Plug in the Heltec V3 with a USB-C data cable, then click **Connect & Flash** and pick the CP210x port.
3. Leave **Erase the whole flash first** ticked if the board had Meshtastic or another firmware on it.

You can also flash with esptool:

```
esptool.py --chip esp32s3 write_flash 0x0 releases/heltec-mp3-v1.1.0.bin
```

For wireless updates from the web page (System tab), use `releases/heltec-mp3-app-v1.1.0.bin`.

## Wiring

| microSD module (3.3 V SPI) | Heltec V3 |
|---|---|
| VCC / GND | 3V3 / GND |
| SCK | GPIO 40 |
| MOSI | GPIO 41 |
| MISO | GPIO 42 |
| CS | GPIO 39 |

| PCM5102A DAC | Heltec V3 |
|---|---|
| VIN / GND | 3V3 / GND |
| BCK | GPIO 48 |
| LCK | GPIO 47 |
| DIN | GPIO 33 |
| SCK | GND |

| KCX_BT_EMITTER (V1.7 recommended) | Connect to |
|---|---|
| +5V / PGND | Heltec 5V / GND |
| AUDIO_L / AUDIO_R / AGND | DAC LOUT / ROUT / AGND |
| RX | level shifter ← GPIO 2 |
| TX | level shifter → GPIO 3 |
| LINK | level shifter → GPIO 38 |

Use a 4-channel BSS138 level shifter (LV = 3V3, HV = 5V). The KCX module expects 5 V serial signals. The Heltec `5V` pin only has power on USB, so add a 5 V boost board for battery use.

Optional buttons (each wired GPIO → button → GND): **UP** GPIO 4, **DOWN** GPIO 5, **OK** GPIO 6, **BACK** GPIO 7.

## Controls

| PRG button | Menus & files | Now Playing |
|---|---|---|
| Tap | Down | Next track |
| Double-tap | Up | Play / Pause |
| Hold ~1 s, release | Select | Controls menu |
| Hold 1.5 s+ | Back | Home |

| Add-on button | Menus & files | Now Playing |
|---|---|---|
| UP / DOWN | Move (hold = fast scroll) | Volume (hold = next / previous) |
| OK | Select | Play / Pause (hold = Controls) |
| BACK | Parent folder | Back to files |

## Wi-Fi

- **First boot:** the player makes a hotspot named **Heltec-MP3-XXXX**, password `heltecmp3`. Join it and the page pops up, or go to `192.168.4.1`.
- **Home network:** in the Wi-Fi tab, pick your network and save. After that, use the IP shown on the OLED (Home → Wi-Fi) or `http://heltec-mp3.local`.
- **If home Wi-Fi is down:** after 15 s the hotspot comes back, and the player keeps retrying.

## Bluetooth headset

- **From the OLED:** Home → **Bluetooth** → *Scan for headsets*, then select one.
- **From the web page:** **Bluetooth** tab → Scan → Connect.
- **Before scanning,** put the headset in pairing mode.
- **Remembered headsets:** the module keeps up to 10 and auto-connects to the first one it sees. "Forget all" clears the list.

## Building from source

- **Arduino core:** ESP32 **3.3.x**. Board: *Heltec WiFi LoRa 32(V3)*, default 8 MB partition scheme (with OTA).
- **Libraries:** ESP8266Audio (earlephilhower) 2.4+, U8g2 (olikraus).
- **Sketch:** open `firmware/HeltecMP3/HeltecMP3.ino`.
- **Rebuild the flasher** with a new firmware:

```
python3 flasher/build_flasher.py <merged.bin> <version> <esptool-js bundle.js>
```

`bundle.js` comes from the npm package `esptool-js`.

## Credits

- KCX_BT_EMITTER AT-command details: [Mark-MDO47/BluetoothAudioTransmitter_KCX_BT_EMITTER](https://github.com/Mark-MDO47/BluetoothAudioTransmitter_KCX_BT_EMITTER)
- Flashing: [esptool-js](https://github.com/espressif/esptool-js)
