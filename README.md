<h1 align="center">X6100 custom firmware</h1>

<p align="center"><i>by Giovanni Ricciardi IU1WXY</i></p>

<p align="center"><b>A complete SD-card firmware for the Xiegu X6100: every digital mode on the radio,
DX cluster on the waterfall, themes, and full remote control from your phone or a browser.</b></p>

<p align="center"><img src="docs/screenshots/main_screen.png" alt="Main screen" width="720"></p>

> **First release: everything is beta, and this is only the beginning.**
> The GUI has been deeply rewritten: a new theme system draws every screen,
> button and font, so the look can change completely without touching the code.
> It runs daily on the author's radio, but expect rough edges. The
> **companion app is alpha**: transmitting from it works, test it on a dummy
> load first. Problems and ideas: open an issue.

## Remote control

**From your phone: X6100 Companion** (Android, [android/](android/)).
Turn on the phone's hotspot, let the radio join it, open the app: it finds the
radio by itself.

- The whole radio: frequency, band, mode, filters, S-meter, spectrum and
  waterfall, RX audio on the phone.
- **Every digital mode, receive and transmit**: FT8/FT4, JS8, WSPR, SSTV
  (pictures from the gallery or the camera), WeFax, RTTY, CW with macros,
  NavTex (receive). The radio decodes and transmits, the phone is the screen.
- Safe by default: the VFO is locked while an app runs (**BASE** brings it
  back), and the phone transmits only after you switch on **Remote TX** on the
  radio (Settings → Wireless → WEB REMOTE).
- Portrait and landscape, English or Italian.

**From any browser: the built-in web server** (`http://<radio IP>/`, nothing
to install). The **Remote** page tunes the radio with live spectrum and
waterfall and plays the RX audio; the other pages set bands, memories, cluster,
backup, and a **gallery** with folders, multi-select, ZIP download and picture
upload for SSTV/WeFax TX.

**From a PC**: wfview (the radio appears as an IC-705 on the network) and CAT
over USB or Bluetooth.

<table>
  <tr>
    <td align="center"><img src="docs/screenshots/android/ft8_portrait.png" alt="Companion app, FT8" width="220"><br><sub>Companion app: FT8</sub></td>
    <td align="center"><img src="docs/screenshots/android/cw_portrait.png" alt="Companion app, CW" width="220"><br><sub>CW with macros</sub></td>
    <td align="center"><img src="docs/screenshots/android/wefax_portrait.png" alt="Companion app, WeFax" width="220"><br><sub>WeFax</sub></td>
  </tr>
</table>

## Features on the radio

### Digital modes, all inside the radio

No PC, no sound card cable: APP → **Modes** opens them, the radio decodes and
transmits by itself.

- **FT8 / FT4**: decode, CQ, reply, auto sequence, QSO log in ADIF.
- **JS8**: JS8Call compatible, all four speeds decoded at once, inbox,
  relays, heartbeats, ADIF log with POTA/SOTA, **APRS** (position, spots,
  SMS, email).
- **WSPR**: decode every cycle, random beacon (TX %), upload to WSPRnet.
- **SSTV**: receive with automatic VIS, **transmit** CQ, replies and pictures
  with text templates (Martin, Scottie, Robot).
- **WeFax** and **NavTex**: station lists, faxes saved as pictures, big or
  full-screen view. WeFax and RTTY also transmit from the phone.
- **CW decoder**: a new receiver that copies down to about 0 dB SNR, shows the
  WPM and does not print noise.
- **CW Keyboard**: type on a USB or Bluetooth keyboard and the radio keys it,
  with a send queue.

### DX Cluster

- Spots drawn **on the waterfall**, in a **column** beside it, and in a full
  **Cluster app** that follows the VFO; tap or press to tune.
- Filters by mode, band and POTA/SOTA; **alerts** on callsigns and prefixes
  with a popup.
- **Spot snap**: turning the knob slowly stops the VFO on a spot.
- Ten public servers built in, add your own from the web page.

### Field Ops: SOTA and POTA

- A **full-screen interface for activations**: finds the summit or park where
  you are (radio GPS, the phone's position or the locator), starts the
  activation, logs the QSOs in ADIF with the reference, spots you on POTA.
- **Bands** with the Reverse Beacon Network: activity per band, who hears you
  and a CW **test beacon**, also as an app of its own outside Field Ops.
- Leaving it with GEN, or a restart, loses nothing: it all lives on the SD card.

### Radio and screen

- **IARU band plan** strip over the waterfall, **S-meter**.
- **Long press** on every front key and on the mic F1/F2 keys: screenshot,
  recorder, mute, open any app or tool.
- Editable bands, memories and digital frequencies; transverters.
- RX recorder, screen recording, screenshots, voice messages.
- CW trainer, SWR scan, GPS, QTH grid.
- Settings rewritten as cards, all reachable with the MFK.

### Wireless

- **Bluetooth CAT** for up to 3 clients at once, together with USB CAT.
- **Bluetooth headphones** (RX audio, volume, speaker mute) and **keyboards**;
  pairings survive a new image.
- **wfview**: the radio appears as an IC-705 on the network.
- **Web server** on the radio, HTTP and HTTPS (see [Remote control](#remote-control)).

<table>
  <tr>
    <td align="center"><img src="docs/screenshots/theme_rs_sstv.png" alt="SSTV TX"><br><sub><b>SSTV TX</b>: picture and text template</sub></td>
    <td align="center"><img src="docs/screenshots/cluster_app.png" alt="DX Cluster"><br><sub><b>DX Cluster</b> app</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screenshots/js8_app.png" alt="JS8"><br><sub><b>JS8</b></sub></td>
    <td align="center"><img src="docs/screenshots/wspr_app.png" alt="WSPR"><br><sub><b>WSPR</b></sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screenshots/cw_keyboard.png" alt="CW Keyboard"><br><sub><b>CW Keyboard</b></sub></td>
    <td align="center"><img src="docs/screenshots/digital_modes_menu.png" alt="Modes menu"><br><sub><b>Modes</b> menu</sub></td>
  </tr>
</table>

## Themes

The GUI has a new **theme system**: colours, buttons, corners, gradients,
fonts and the waterfall palette all come from the theme, and a theme can draw
every surface with PNG images. Three are built in, choose them in Settings →
Display → **Theme**; your own go on the SD card
([how to make one](docs/THEMES.md)). The control the encoder is adjusting
lights its red LED.

<table>
  <tr>
    <td align="center"><img src="docs/screenshots/theme_iu1wxy.png" alt="IU1WXY theme"><br><sub><b>IU1WXY</b> (default): dark blue-grey</sub></td>
    <td align="center"><img src="docs/screenshots/theme_classic.png" alt="Classic theme"><br><sub><b>Classic</b>: black, grey keys</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screenshots/theme_rs.png" alt="RS theme"><br><sub><b>RS</b>: glossy buttons with LEDs, black LCD</sub></td>
    <td align="center"><img src="docs/screenshots/theme_rs_settings.png" alt="RS settings"><br><sub><b>RS</b>: settings</sub></td>
  </tr>
</table>

## Install

1. Download `sdcard.v0.9.img.zip` from the **[latest release](https://github.com/IU1WXY/X6100-IU1WXY/releases/latest)** and unzip it.
2. Write it to a microSD card with [balenaEtcher](https://etcher.balena.io/) or Rufus.
3. Put the card in the radio and switch on. Settings and logs live on the card's
   DATA partition (`/mnt`).

Writing a new image erases DATA too: copy it to your PC first and back after
the first start (the [manual](docs/MANUAL.md) lists the files). A patched
baseband ([x6100_base_patch](https://github.com/gdyuldin/x6100_base_patch)) r12
or newer is recommended for the TX audio options.

## Documentation

- **[Manual](docs/MANUAL.md)**: every app and setting, the web pages, CAT commands, files, building
- [Remote API](docs/REMOTE_API.md): what the companion app talks to
- [Themes](docs/THEMES.md): make your own

## Credits

**Giovanni Ricciardi IU1WXY** designed and wrote this firmware:

- the deep **rewrite of the GUI** and the **theme system**, with the IU1WXY,
  Classic and RS themes and the new settings;
- the **X6100 Companion** app and the **remote control** of the radio (remote
  API, web Remote page, gallery, backup);
- the **DX Cluster** (waterfall spots, column, app, filters, alerts, spot snap);
- **WSPR**, **SSTV TX**, **RTTY** and **WeFax TX**, the **CW Keyboard**, the
  integration of JS8, SSTV, WeFax and NavTex into the radio;
- **Bluetooth** CAT, headphones and keyboards, **wfview** server (Icom LAN
  protocol), IARU band plan strip, configurable long-press actions, CW trainer
  stats, and the SD image itself.

Standing on the work of: the **R1CBU** GUI by Oleg Belousov R1CBU and Georgy
Dyuldin R2RFE ([gdyuldin/x6100_gui](https://github.com/gdyuldin/x6100_gui)),
**1KO125** by Hany El Imam / TheMurusTeam (WeFax, NavTex, SSTV decoders),
**JS8** by randal007 on the JS8Call engine (Jordan Sherer KN4CRD), **wsprd**
from WSJT-X (K1JT, K9AN). Libraries: LVGL, cJSON, FFTW, liquid-dsp, libpng,
bottle, PulseAudio, BlueZ. Fonts: Roboto, DejaVu, Arial, Sony Sketch.
Hardware: Xiegu.

## License

The GUI is LGPL-2.1-or-later ([LICENSE](LICENSE)); the vendored js8core and
wsprd are GPLv3, so the firmware image is GPLv3. The web server has its own
license ([webserver/LICENSE](webserver/LICENSE)).
