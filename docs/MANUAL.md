# X6100 custom firmware: manual

How to use every part of the firmware, and the reference tables. The short
overview is the [README](../README.md).

Each option has a "Where:" line with its place on the radio, for example
"Where: Settings > Display > MAIN SCREEN". Settings opens from APP (page 2) >
Settings. Button and option names are written as the radio shows them.

## Contents

- [Getting started](#getting-started): [first steps](#first-steps), [buttons and knobs](#buttons-and-knobs)
- [Main screen](#main-screen): [status bar](#status-bar), [S-meter](#s-meter), [waterfall](#waterfall), [screen recording](#screen-recording), [Walkie Talkie](#walkie-talkie-mode)
- [Settings](#settings): [General](#general-tab), [Display](#display-tab), [Audio](#audio-tab), [Wireless](#wireless-tab), [Keys](#keys-tab), [Cluster](#cluster-tab), [TX Cal](#tx-cal-tab), [Info](#info-tab)
- [Transmitting](#transmitting): [digital-mode TX level](#digital-mode-tx-level), [recording voice messages](#recording-voice-messages)
- [Digital modes](#digital-modes): [opening a mode](#opening-a-mode), [FT8 and FT4](#ft8-and-ft4), [JS8](#js8), [WSPR](#wspr), [SSTV, WeFax and NavTex](#sstv-wefax-and-navtex), [PSK31, PSK63 and Olivia](#psk31-psk63-and-olivia), [RTTY and WeFax TX](#rtty-and-wefax-tx)
- [CW](#cw): [CW decoder](#cw-decoder), [CW from the keyboard](#cw-from-the-keyboard), [CW messages](#cw-messages), [CW Trainer](#cw-trainer)
- [Apps](#apps): [DX Cluster](#dx-cluster), [Field Ops (SOTA / POTA)](#field-ops-sota--pota), [Bands](#bands)
- [Bluetooth](#bluetooth): [headphones](#bluetooth-headphones), [CAT](#bluetooth-cat)
- [Themes](#themes)
- [Web configurator](#web-configurator)
- [X6100 Link app](#x6100-link-app)
- [wfview server](#wfview-server)
- [Troubleshooting and FAQ](#troubleshooting-and-faq)
- [Reference](#reference): [Bluetooth CAT connection](#bluetooth-cat-connection), [CI-V commands](#supported-ci-v-commands), [btcat-ctl](#btcat-ctl), [btaudio-ctl](#btaudio-ctl), [files on /mnt](#files-on-mnt)
- [Build](#build)

## Getting started

### First steps

1. Set your callsign and QTH locator.
   Where: Settings > General > STATION.
   JS8, FT8, WSPR (TX and upload) and the DX Cluster need the callsign.
2. Connect to a WiFi network, for example your phone's hotspot.
   Where: Settings > Wireless > WIFI.
   WiFi sets the clock, and the DX Cluster, uploads and the X6100 Link app
   need it.
3. Check the clock. FT8, JS8 and WSPR need it right to a second or two.
   Where: Settings > General > DATE / TIME.
4. Open the web configurator from a browser on the same network:
   `http://<radio IP>/` (see [Web configurator](#web-configurator)).

### Buttons and knobs

The APP button opens three pages of apps and tools:

| Page | Buttons |
|---|---|
| APP 1:3 | Modes, Cluster, SWR Scan, RX Recorder |
| APP 2:3 | Settings, CW Trainer, Field Ops |
| APP 3:3 | Bands, GPS, Screen rec |

- Each app has its own button pages, named like `(JS8 1:7)`. The first
  button goes to the next page.
- The MFK walks the items of an app or a list; a press chooses.
- ESC closes popups and apps.

Long press. Each button group (GEN, APP, KEY, MSG, DFN, DFL) has a
long-press action.
Where: Settings > Keys > FRONT KEYS, LONG PRESS.

| Kind | Actions |
|---|---|
| Quick | none, screenshot, RX recorder on/off, screen rec on/off, mute, WF mode, ATT on/off, PRE on/off, tuner on/off, tuner tune (transmits a carrier) |
| Digital modes | RTTY, FT8, WSPR, JS8, SSTV, WeFax, NavTex |
| Tools | Cluster, CW Trainer, Field Ops, Bands, RX recorder, SWR Scan, GPS, QTH grid, Settings |

The hand mic F1 and F2 keys (short and long press) are set in the same tab.
Where: Settings > Keys > HAND MIC KEYS.
The hand mic MODE key steps LSB, USB, CW, CW-R, AM, NFM; a long press turns
DIG on or off in SSB, like a long press of the front SSB key.
The hand mic FIL key works as the front DFL key: the filter pages, and the
DFL long press action.

## Main screen

### Status bar

The box at the top left of the main screen: the radio state on the first row,
the connections on the second.

| Item | Meaning |
|---|---|
| VFO-A / VFO-B | the VFO in use; SPL-A / SPL-B when split is on |
| USB-D, LSB, CW... | the mode |
| AUTO, FAST, SLOW, OFF | the AGC |
| PRE, ATT, ATU | highlighted when the preamp, the attenuator or the tuner is on |

Each icon of the second row is dark when off, grey when on but not connected
yet, in colour when working:

| Icon | Colour means |
|---|---|
| WiFi | green connected; grey on, no network; dark off |
| Bluetooth | blue a device connected; grey on, nothing connected; dark off |
| Note | blue Bluetooth headphones connected (A2DP) |
| Keyboard | amber a Bluetooth CAT client connected; grey CAT bridge ready |
| Arrow | green GPS position (fix in the last 10 s); grey GPS on, no fix |
| DX | green DX cluster connected; grey connecting or retrying; dark stopped |

In the middle is the [S-meter](#s-meter). On the right: the local time, the
battery (icon and voltage) and the external supply voltage. The plug turns
into a lightning bolt while charging. On battery only, the supply voltage is
replaced by the battery percentage.

### S-meter

The S-meter shows the signal and its level in dB. Three options set its look.

Where: Settings > Display > MAIN SCREEN (also on the web configurator's
Display page).

| Option | Values | What it does |
|---|---|---|
| S-meter type | Bars, Needle | Bars: thin bars along the scale (default). Needle: a wide, low dial with a needle, in the theme's colours; the dB value is under the dial. |
| S-meter colours | Theme, Scale, Level | Theme: as the theme draws it. Scale: green, yellow and red along the scale. Level: the lit bars (on the needle, the arc up to the signal) in the colour of the signal level. |
| Needle lamp | on, off | An amber dial lit from behind, with dark marks and a red zone over S9. Needle only; it replaces the colours. |

The VISION theme always uses bars.

S-meter calibration. The reading can be shifted in dB. The default, -22 dB,
matches an IC-7300. Change it only while reading the same signal on a
calibrated reference (a radio or a generator); the radio shows this warning
when you start editing it.
Where: Settings > General > CONTROLS > S-meter cal.

### Waterfall

Where: Settings > Display > WATERFALL.

| Option | What it does |
|---|---|
| Mode | Centered keeps the VFO in the middle; Fixed keeps the span |
| Auto level, offset | the levels follow the noise floor; the offset makes it darker or brighter |
| Min / max | the manual levels, used when auto level is off |
| Line, zoom | a line on the VFO; zoom with the waterfall instead of the span |
| Smooth scroll | scrolls by pixels instead of lines |
| Speed | 3 = 15 lines/s (default), 4 = 20, 5 = 30; 2 = 7.5 and 1 = 5 average the frames into a line: less noise and less CPU |

Over the waterfall. Where: Settings > Display > MAIN SCREEN.

| Option | What it does |
|---|---|
| Band plan | the band segments in a strip at the top |
| Cluster spots | the DX cluster calls at their frequencies (see [DX Cluster](#dx-cluster)) |
| Cluster column | the spot list in a box on the right |

Spectrum. Where: Settings > Display > SPECTRUM (off by default; the screen
restarts to lay it out). Fill: the area under the trace filled. Peaks: the
peaks held for a moment. The band plan and the cluster spots stay at the top
of the waterfall.

Colours, on the web configurator's Display page:

| Option | Values |
|---|---|
| Waterfall colours | Theme (default), Classic, Gauss, Grey, Fire, Ice, Rainbow |
| Trace colour | Theme, or your own |
| Fill colour | As trace, or your own (the trace is then drawn over it) |

### Screen recording

Records the screen and the audio to an AVI file (MJPEG video, 800x480,
8 frames/s, mono audio).

- Start and stop: APP 3:3 > Screen rec (the button reads Stop rec while
  recording), or a long-press action (screen rec on/off).
- Files: `/mnt/screenrec/screen-YYYYMMDD-HHMMSS.avi`. The web configurator's
  Gallery lists them and can convert them to MP4.
- While recording, the monitor level is raised (to at least 30) so your
  transmitted voice is recorded too. Your monitor setting comes back when the
  recording stops.
- It needs the SD card and 50 MB free ("Screen rec: no SD card", "Disk full").

### Walkie Talkie mode

The radio held upright, the knobs on top, with a plain screen: VFO or memory,
RX/TX, frequency, mode, S-meter, and the VOL, SQL and PWR bars. It follows the
theme.

Where: Settings > General > CONTROLS > Walkie Talkie. The screen turns to
portrait at once, no restart.

> **How to leave Walkie Talkie mode (EN)**
> 1. Press **APP**: the walkie buttons appear on the left edge, the first one reads "(WLK 1:4)".
> 2. **Hold** that first button "(WLK 1:4)" (or press it three times): it goes to "(WLK 4:4)".
> 3. **Hold** the button **"Exit hold"** for a second. The radio goes back to the normal landscape screen.
>
> **Come uscire dal modo Walkie Talkie (IT)**
> 1. Premi **APP**: sul bordo sinistro compaiono i tasti del walkie, il primo dice "(WLK 1:4)".
> 2. **Tieni premuto** quel primo tasto "(WLK 1:4)" (oppure premilo tre volte): passa a "(WLK 4:4)".
> 3. **Tieni premuto** per un secondo il tasto **"Exit hold"**. La radio torna alla schermata normale orizzontale.

- PTT: the PTT key on the side of the radio. It transmits with the built-in
  microphone; the border turns red and the S-meter shows the power and SWR.
- VOL knob: always the volume. The buttons that set a value use the MFK.
- F1-F5 run down the left edge, with the buttons next to them. APP opens the
  walkie pages:

| Page | Buttons |
|---|---|
| WLK 1 | MEM/VFO, Band +, Band -, TX power |
| WLK 2 | AM/FM, SSB (held: DIG), ATU (held: tune, it transmits), PRE (held: ATT) |
| WLK 3 | AGC (held: split), A/B (held: copy), Step (held: back), Lock |
| WLK 4 | Vol Sql RF Pwr (the levels page), Exit (held) |

- GEN, KEY, DFN, DFL and VM open their pages as on the main screen.
- MEM: the main knob steps through the memories; the number and name are on
  top.
- Exit: APP, then WLK 4, hold "Exit hold" (steps in the box above). The screen
  goes back to landscape, no restart.
- Not in walkie mode: the apps, Settings and the hand mic keys. Leave walkie
  mode to use them.

## Settings

APP (page 2) > Settings. The tabs are on two button pages: General, Display,
Audio, Wireless (page 1); Keys, Cluster, TX Cal, Info (page 2).

The MFK walks the controls and stops at the first and last one. There the
page shows its top or its end, so status rows below the last control are
always in view. Some cards depend on the base firmware revision (TX AUDIO,
band output gain, TX filter, CESSB, FM emphasis): they are hidden when the
base is too old.

### General tab

| Card | Options |
|---|---|
| STATION | callsign, QTH locator |
| DATE / TIME | date, time, time zone |
| CONTROLS | Freq acceleration, Accel. onset, Charger (Off, On, Shadow), S-meter cal, Walkie Talkie (see [Walkie Talkie mode](#walkie-talkie-mode)) |
| TRANSVERTERS | two transverters: from, to, shift |
| WFVIEW SERVER | on/off, user, password, server port and status, connected client |

Main knob acceleration. Where: Settings > General > CONTROLS > Freq
acceleration.

- The radio times every detent of the main knob, and the step grows smoothly
  with the speed.
- Turning slowly the step is the tuning step. Turning faster it grows up to
  6 times (Lite) or 15 times (Strong, which also starts sooner). None keeps
  the step.
- Accel. onset (1-5, default 3) sets how soon it starts: 1 only when turned
  fast, 5 already when turned gently.

S-meter cal: see [S-meter](#s-meter). WFVIEW SERVER: see
[wfview server](#wfview-server).

### Display tab

| Card | Options |
|---|---|
| BACKLIGHT | timeout and brightness, buttons brightness (Always Off, Always On, Temporarily On) |
| SPECTRUM | Spectrum, Fill, Peaks (see [Waterfall](#waterfall)) |
| WATERFALL | see [Waterfall](#waterfall) |
| MAIN SCREEN | Band plan, Cluster spots, Cluster column, S-meter type, S-meter colours, Needle lamp |
| INTERFACE | Popups Freq/Info/ALC, Knob info, Theme, Font, FT8/FT4 info |

- Popups Freq/Info/ALC: a big frequency while tuning, a big value of the
  control being changed, a big ALC reading while transmitting.
- Knob info: the value shown while a knob turns.
- Theme: see [Themes](#themes). A new theme applies at once, without a restart.
- Font: Sony Sketch, Roboto or DejaVu (default). A new font applies at once.
  Themes with their own font (RS, Black Metal, Total Black) keep it.

All these options are also on the web configurator's Display page.

### Audio tab

| Card | Options |
|---|---|
| RX AUDIO | Speaker mode, Line-in / Line-out, Play / Rec gain |
| TX AUDIO | compressor (threshold, makeup), TX filter, FM pre/de-emphasis |
| MICROPHONE | Input (Built-in, Hand, Auto), Hand mic gain, Built-in gain |
| VOX | On, Gain, Anti-VOX, Delay |
| AGC | Hang, Knee, Slope |
| BT HEADPHONES | RX to headphones, Volume, Mute speaker (see [Bluetooth headphones](#bluetooth-headphones)) |

### Wireless tab

| Card | Options |
|---|---|
| WIFI | power, status, IP, networks |
| BLUETOOTH | Bluetooth (power), Pairing (3 min), device list, Scan headphones (also finds keyboards) |

### Keys tab

| Card | Options |
|---|---|
| FRONT KEYS, LONG PRESS | the long-press action of GEN, APP, KEY, MSG, DFN, DFL (see [Buttons and knobs](#buttons-and-knobs)) |
| HAND MIC KEYS | F1, F2, F1 long, F2 long |

### Cluster tab

| Card | Options |
|---|---|
| CLUSTER FILTERS | Modes (CW, SSB, DIG, AM/FM), Bands (160-6 m), Activity (All spots, POTA, SOTA, POTA + SOTA) |
| CLUSTER VIEW | Newest first, All bands, Spot snap hold, Spot snap speed |
| PSK REPORTER | Report decodes (on by default) |

What they do: see [DX Cluster](#dx-cluster).

### TX Cal tab

| Card | Options |
|---|---|
| TX CALIBRATION | IQ offsets, band output gain, codec gain |
| CESSB | on, level |
| TX PROTECTION | SWR protection |

SWR protection. Where: Settings > TX Cal > TX PROTECTION > SWR protection.

- Off, or the highest SWR allowed, 1.5 to 5.0 in steps of 0.5. Default 2.0.
- When the SWR stays over it for 0.3 s, the transmission stops and the radio
  says "TX stopped: SWR 3.4". Short spikes at key-up don't count.
- Not while the ATU tunes: the tune carrier sees a high SWR on purpose.
- Nothing stays locked: the next PTT or transmission goes out again.
- It stops what the radio keys itself: CAT PTT, the PTT button, CW messages
  and the CW keyboard, voice messages, the digital apps and the keyboard
  paddle (it keys again after both paddles are let go). A PTT held on the
  hand mic, VOX and the radio's own CW keyer may key again at once: let go
  and check the antenna.

### Info tab

The FIRMWARE card: the firmware version, the author, the baseband version and:

- Update alert: a message at start when a newer release is out.
- Diagnostics, Send...: sends a diagnostics text to IU1WXY to help with a
  problem.

Diagnostics. Where: Settings > Info > FIRMWARE > Diagnostics.

1. Ask IU1WXY first. The text is accepted only while IU1WXY has opened
   reception for it.
2. Press Send... The radio collects the text and shows it on screen first.
   The MFK scrolls it; a press or Send sends it, ESC or Cancel closes it.
3. After sending, the row shows a code. Tell IU1WXY that code.

The text has the versions, uptime and memory, the Bluetooth controller and
devices, the last pairing log, the input devices and the kernel messages
about Bluetooth, HID and USB. It has no WiFi networks, no position and no
logbook. The first time, a short note explains what it is for: it is read
only by IU1WXY and deleted within 90 days.

| Message | Meaning |
|---|---|
| sent, code XXXXXX | received; tell IU1WXY the code |
| not accepted now: ask IU1WXY first | reception is closed |
| too many today, try tomorrow | the daily limit is reached |
| not sent: no internet? | the radio could not reach the site |

## Transmitting

### Digital-mode TX level

The apps that transmit from the radio (FT8/FT4, JS8, WSPR, SSTV, RTTY,
WeFax) set their audio level by themselves.

- The level settles with the ALC working a little (about 1), with the power
  at the setting.
- In the first second or so of a transmission nothing changes. Then, while
  the ALC reads under 1, the level goes up slowly and stops there; when the
  ALC goes over 3 or the power is well over the setting, it comes down.
- The level found is kept for the next transmission.

TX power while a digital app is open:

- The power is capped at 7 W. With a higher setting the app says so
  ("Power is limited to 7W"), and the setting comes back when you close it.
- The Power button in the apps sets the power: 0.5, 1, 2, 3, 5, 7 W.
- Digital apps reopen on their last frequency and mode.

### Recording voice messages

**Voice messages can be recorded only from the hand mic.** The built-in
microphone cannot record them. This is a limit of the radio's baseband, not
of this firmware: the baseband keeps the built-in microphone off while
receiving and turns it on only when transmitting.

- It applies to MSG > Voice (Rec) and to the Field Ops voice CQ (Rec CQ).
- With no hand mic, after 3 seconds the radio says "No audio: recording uses
  the hand mic".
- Transmitting with the built-in microphone works as usual (Settings > Audio
  > MICROPHONE), also in Walkie Talkie mode.
- The recording keeps 250-3000 Hz, what SSB sends, without the radio's own
  hum and whistles.

## Digital modes

### Opening a mode

APP (page 1) > Modes opens a menu: RTTY, PSK31, PSK63, FT8, FT4, FT2, WSPR, Olivia,
JS8, SSTV, WeFax, NavTex. Turn the MFK to choose, press to open; the last used mode is marked.
Pressing Modes again closes the menu.

- Callsign and QTH: Settings > General > STATION.
- The apps reopen on their last frequency and mode.
- Power is capped at 7 W while an app is open (see
  [Digital-mode TX level](#digital-mode-tx-level)).

### FT8 and FT4

APP > Modes > FT8 (or FT4, FT2). The radio tunes an FT8/FT4/FT2 frequency; the band
keys step through the others. 80 px of waterfall (about two slots) sit above
the list. The main knob moves your TX audio offset.

Buttons:

| Page | Buttons |
|---|---|
| 1 | Show: All / CQ, Mode: FT8 / FT4 / FT2, TX CQ, TX Call |
| 2 | Hold Freq, Auto, Force QSO save, Power |
| 3 | CQ Modifier, Time Sync, Log Map, Live Map |

- Show: CQ shows the CQs and the messages to you; All shows all traffic.
- TX CQ calls CQ. TX Call allows or stops transmitting; stopping cuts a
  transmission in progress.
- Hold Freq: your TX offset does not move to the station you answer.
- Auto: the next message of the QSO is sent by itself.
- Time Sync: rounds the clock to the nearest slot (15 s FT8, 7.5 s FT4, 3.75 s FT2).
- CQ Modifier: for example `CQ POTA` or `CQ DX`.

The list. Each row has the slot time, the message, and on the right the
country with its flag, the distance (when the sender sent a grid and your
QTH is set) and the SNR. The country comes from AD1C's cty.dat (SP2KMO's
upstream PR #282). Settings > Display > INTERFACE > FT8/FT4 info picks what
goes next to the SNR: km, mi, Country, Country + km (the default) or
Country + mi.

| Row | Colour |
|---|---|
| CQ, not worked | green |
| CQ, worked on another band or mode | dark green |
| CQ, worked on this band and mode | darker green, struck through |
| a message to you | red |
| your TX | blue |

Replying. Turn the MFK onto a row and press it, or touch it. The radio
answers that station, moves your TX offset there (unless Hold Freq is on) and
turns TX Call on. A press while transmitting stops TX.

The "For you" strip. Stations that call you stay pinned in a strip under the
list, so they do not scroll away.

- Up to 3 stations, one row each, newest at the bottom.
- A row stays 2 minutes after the station's last message. It leaves earlier
  when you send that station RR73 or 73. A band or mode change clears it.
- A touch or an MFK press on a row replies, as in the list.
- With the MFK: down past the last row of the list enters the strip; up from
  its first row goes back to the list.
- The strip has no title: a line in the "to you" colour sits on top of it.
  It is hidden when empty.

Log. A QSO is logged by itself when the other station sends R+report or
RR73 and both reports are known; "Force QSO save" logs by hand. The log is
`/mnt/ft_log.adi`; the radio says "Saved QSO de ...".

Log Map. Page 3 > Log Map shows the logged stations on a world map centred on you:
great-circle paths, a dot and the callsign for each station. Calls and dots
are larger since v0.119. ESC or Log Map closes it.

Live Map. Page 3 > Live Map puts on the same map every station decoded in
the last 15 minutes that sent a grid (a CQ or a reply with its 4-character
square). The dot's colour is the distance from your QTH square: green under
1000 km, blue under 3000, orange under 8000, pink beyond. Stations heard in the
last minute have a bigger, ringed dot and their call first; a call is left out
where it would cover another. A band change empties it. Your own position is
the centre of your 4-character square, the red dot. ESC or Live Map closes it;
the map is a snapshot, open it again to refresh.

Live Map (IT). Pagina 3 > Live Map mette sulla stessa mappa ogni stazione
decodificata negli ultimi 15 minuti che ha mandato un locatore (un CQ o una
risposta con il quadrato a 4 caratteri). Il colore del punto è la distanza dal
tuo quadrato: verde sotto 1000 km, blu sotto 3000, arancio sotto 8000, rosa
oltre. Chi è stato sentito nell'ultimo minuto ha il punto più grande, cerchiato,
e il nominativo per primo; un nominativo che coprirebbe un altro resta fuori.
Cambiando banda si svuota. La tua posizione è il centro del tuo quadrato a 4
caratteri, il punto rosso. ESC o Live Map la chiudono; è una fotografia, per
aggiornarla si riapre.

PSK Reporter. Settings > Cluster > PSK REPORTER > Report decodes (on by default)
sends what you decode in FT8, FT4, FT2 and JS8 to pskreporter.info, the way
WSJT-X does: UDP to report.pskreporter.info:4739, batched about every 5 minutes
(up to 30 s earlier or later, at random), each call once per band in 5 minutes.
Your callsign goes with your QTH square cut to 4 characters (JN35, never the
6-character locator) and "X6100 IU1WXY v0.xxx" as the software; each spot
carries the call, its grid when the message had one, dial + audio frequency,
SNR and mode. Your own transmissions are never reported. With no network the
batch is dropped quietly; nothing waits on it, the decoding goes on. Search
your call on pskreporter.info to see the spots after a few minutes.
In PSK31, PSK63 and Olivia the text is free, so a station is reported when the
decoder reads "DE <call>" (as in "CQ CQ DE IK1ABC"), with no grid and 0 dB SNR,
because these modes give no S/N figure.

PSK Reporter (IT). Settings > Cluster > PSK REPORTER > Report decodes (acceso
di fabbrica) manda a pskreporter.info quello che decodifichi in FT8, FT4, FT2 e
JS8, come fa WSJT-X: UDP a report.pskreporter.info:4739, a gruppi circa ogni 5
minuti (fino a 30 s prima o dopo, a caso), ogni nominativo una volta per
banda in 5 minuti. Con il tuo nominativo va il quadrato QTH tagliato a 4
caratteri (JN35, mai il locatore a 6) e "X6100 IU1WXY v0.xxx" come software;
ogni spot porta nominativo, locatore se il messaggio lo aveva, frequenza dial +
audio, SNR e modo. Le tue trasmissioni non vengono mai riportate. Senza rete il
gruppo si perde in silenzio; niente resta in attesa, la decodifica continua.
Cerca il tuo nominativo su pskreporter.info per vedere gli spot dopo qualche
minuto. In PSK31, PSK63 e Olivia il testo è libero: una stazione viene riportata
quando il decoder legge "DE <nominativo>" (come in "CQ CQ DE IK1ABC"), senza
locatore e con SNR 0 dB, perché questi modi non danno una misura di S/N.

FT2. FT2 (IU8LMC, 2026) is FT4 at twice the speed: 41.67 Bd, 103 symbols in
2.47 s, 167 Hz wide, 3.75 s slots. Mode cycles FT8 > FT4 > FT2; the band keys
step through the FT2 frequencies (1.843, 3.568, 5.360, 7.062, 10.144, 14.084,
18.108, 21.144, 24.923, 28.184, 50.316 MHz). A transmission starts only in the
first 0.8 s of its slot, so that it ends inside it; the "RX" line comes every
4th slot (15 s). QSOs are logged as ADIF MFSK / FT2. From SP2KMO's upstream
port. The knob moves the TX offset 5 or 10 Hz a click when turned fast again
(4 or more clicks in 30 ms), from randal007's upstream fix.

FT2 (IT). FT2 (IU8LMC, 2026) è FT4 a velocità doppia: 41,67 Bd, 103 simboli in
2,47 s, 167 Hz di banda, slot di 3,75 s. Mode passa FT8 > FT4 > FT2; i tasti
BAND scorrono le frequenze FT2 (1,843, 3,568, 5,360, 7,062, 10,144, 14,084,
18,108, 21,144, 24,923, 28,184, 50,316 MHz). Una trasmissione parte solo nei
primi 0,8 s dello slot, così finisce dentro; la riga "RX" arriva ogni 4 slot
(15 s). I QSO vanno nel log come ADIF MFSK / FT2. Dal port upstream di SP2KMO.
La manopola sposta di nuovo l'offset TX di 5 o 10 Hz a scatto quando gira
veloce (4 o più scatti in 30 ms), dalla correzione upstream di randal007.

WSJT-X UDP output. While the app is open the radio sends WSJT-X compatible
UDP messages (heartbeat, status, decodes, logged QSOs), so GridTracker or
JTAlert can follow it. From SP2KMO's upstream PR #275. It is set in
`/mnt/ft8_udp.conf` (`enabled=`, `host=`, `port=`, `id=`); the defaults are
port 2237 and the network gateway (your phone on a hotspot) plus the WSJT-X
multicast group.

### JS8

Beta 3 of randal007's [X6100 JS8](https://github.com/randal007/x6100-js8):
a JS8Call-compatible decoder and keyboard in the radio, with all four speeds
(Normal, Fast, Turbo, Slow) decoded at once.

First steps:

1. Set callsign and QTH in Settings > General > STATION. JS8 will not send
   without a callsign.
2. The radio clock must be within about 2 s: WiFi sets it, or use Time Sync
   (page 3) after some stations have decoded. It shifts only JS8's own
   timing (a drift shown on the top line); hold the button to reset it.
3. APP > Modes > JS8. The radio tunes the nearest JS8 frequency; the band
   keys step through JS8Call's (or GhostNet's) frequencies, or type your own
   (Freq, page 6). While JS8 is open the RX filter is 200-3000 Hz, noise
   reduction, noise blanker and notch are off, and power is capped at 7 W;
   all go back when you leave.
4. Turn the MFK onto a station's row, then HW CPY? (page 1), Reply (page 2)
   or Query > (page 1).
5. ESC or the top knob stops sending; the next ESC leaves the app.

Nothing sends by itself: AUTO (automatic replies), HB (heartbeats) and HB ACK
are off every time the app opens.

Buttons (the first one of each row turns the page, hold to go back):

| Page | Buttons |
|---|---|
| 1 | CQ (hold: auto CQ), Heartbeat, Query >, HW CPY? |
| 2 | Show: No HB / Directed / All, Reply, Send..., Clear |
| 3 | Time Sync, Hold: Off / On, Show Stations / Messages, Inbox |
| 4 | AUTO, HB, HB ACK, Settings... (INFO, STATUS, Relay, Groups) |
| 5 | APRS >, Log QSO, Activ.: Off / POTA / SOTA, Log prompt |
| 6 | Alerts >, Speed, Decode: All speeds / My speed, Freq: JS8 / GhostNet / kHz |
| 7 | Power, Map |

What it does:

- One-press messages (SNR?, GRID?, INFO?, STATUS?, HEARING?, AGN?, RR, 73),
  an Inbox for `MSG` messages, relays and store-and-forward like desktop
  JS8Call.
- Log: the Log popup (page 5, or when a QSO ends with 73/SK) saves to
  `js8call_log.adi` in desktop JS8Call's ADIF format, with POTA/SOTA
  activation fields. FT8 keeps its own `ft_log.adi`.
- Map (page 7): the stations of the JS8 log on the same map as FT8.
- Alerts: beeps for messages, CQs and new stations, and purple alert words.
- APRS through JS8 gateways: grid and GPS position beacons (optionally with a
  message), POTA and SOTA spots, SMS, email and Winlink.
- Rows in the list: blue sent by you, red to you, green a CQ, dark blue to a
  group, purple an alert word.

Known limits: relays, store and forward, and most APRS items are not yet
proven on the air (only the grid spot and SMS are). The main screen's
waterfall stands still while JS8 is open.

Design notes: [docs/RESEARCH.md](RESEARCH.md),
[docs/TX_PLAN.md](TX_PLAN.md), [docs/T6_PLAN.md](T6_PLAN.md),
[docs/SPOTS_PLAN.md](SPOTS_PLAN.md).

### WSPR

APP > Modes > WSPR. The radio tunes the nearest WSPR dial frequency (USB-D,
WSJT-X's list); the band keys step through the others. Cycles are two minutes
from the even UTC minute, so the clock must be right to about a second (GPS
or NTP).

- RX: every cycle not used for TX is decoded at 1:54. The spots go to the
  top of the table (UTC, SNR, DT, audio Hz, drift, call, grid, dBm, km from
  your QTH).
- TX % (0, 10, 20, 33, 50): the chance that a cycle is a beacon, chosen at
  random as in WSJT-X. TX next sends one beacon in the next cycle; press
  again to stop it, also on the air.
- The message is the callsign, the first 4 characters of the locator and the
  power (at most 7 W, rounded to a WSPR level). Compound calls with a `/`
  can't be sent.
- The main knob moves the TX audio offset (1410-1590 Hz).
- Upload: the spots of each cycle go to WSPRnet (plain HTTP over WiFi). The
  status line counts what was sent and what failed.
- Power: on page 2.

### SSTV, WeFax and NavTex

These apps work on known frequencies, so they don't need the waterfall. They
fill the screen in the same look as WSPR, and Freq. opens the list to tune
from (the MFK chooses, a press tunes). The header shows the station or
frequency and the receiver state. Leaving the app gives the VFO back.

- NavTex: NAVTEX 518 and 490 kHz, NAVTEX HF 4209.5 kHz and the HF MSI
  frequencies. The MFK scrolls the text.
- DSC (in NavTex, Mode: NAVTEX / DSC): receive only, MF/HF distress and
  safety calls on 2187.5, 4207.5, 6312, 8414.5, 12577 and 16804.5 kHz (Freq.
  lists them). One row per call, the newest on top: UTC time, category
  (DISTRESS, URGENCY, SAFETY, ROUTINE, ACK), the caller's MMSI with its
  country, then the nature of distress, ALL SHIPS or the called MMSI and the
  proposed mode and frequency. Distress rows are red, with the position and
  the time it was valid. A `?` marks a call whose error check failed. Save
  (page 2) writes the calls to the Files page.
- WeFax: DWD, Northwood and Athens stations. View (page 2) chooses Big
  (default), Full (the whole fax) or Small, the old low panel that keeps the
  main waterfall visible above it.
- SSTV: calling frequencies, LSB on 80 and 40 m (3.735, 3.845, 7.165,
  7.171 MHz) and USB above (14.230, 14.233, 18.160, 21.340, 24.975, 28.680,
  50.510 MHz). The picture is on the left, the decoder state on the right,
  the decoding progress in the thin bar under the header. The decoder is
  TheMurusTeam's 1KO125 beta8 one, with a tolerant VIS detection.

SSTV buttons:

| Page | Buttons |
|---|---|
| 1 | Start Auto RX, Start Manual RX, RX Mode (Manual RX), TX |
| 2 | Freq., RX Pics Browser, Delete picture, Auto RX Restart (on: a new VIS during a picture starts a new one) |
| 3 | Test RX, Close (ESC also closes), Power |

SSTV TX. TX opens the TX menu (turn the MFK, press to choose):

- CALL CQ: a picture with CQ CQ SSTV and your callsign on it.
- REPLY TO / REPLAY TO the target callsign: the reply text on a picture;
  Replay adds a miniature of the last received picture.
- TX IMAGE ONLY: the picture without text. REPEAT LAST TX sends the last
  transmission again.
- SET TARGET CALLSIGN: type the other station's call (Reply and Replay need
  it).

Then choose the picture, the text (templates like `R575QSB`, `PSEQSL`, `73`)
and its colours; the next press transmits.

- TX Mode chooses Martin, Scottie or Robot. Cancel TX goes back. Halt TX
  stops on the air.
- TX goes through the same player as FT8 and WSPR: power at most 7 W, level
  set from the ALC and the power.
- Pictures are in `/mnt/sstv/tx_pics`, text templates in
  `/mnt/sstv/tx_text`; the default ones are copied there at the first start.
  Received pictures are saved in `/mnt/sstv`.
- Pictures can be uploaded from the web configurator's Gallery or sent from
  the phone.

### PSK31, PSK63 and Olivia

Keyboard-to-keyboard modes, ported from SP2KMO's OpenX6100 PSK31 and Olivia
work (randal007's merge). APP > Modes > PSK31, PSK63 or Olivia. The radio
goes to the nearest standard dial frequency in USB-D (PSK 14.070, 7.040,
3.580...; Olivia 14.071, 7.071, 3.5815...); the BAND keys step through the
others. Frequency and mode come back when the app closes.

- The waterfall covers the receive filter; the coloured band is where the
  decoder listens. The main knob moves it (5 Hz a click in PSK, 10 Hz in
  Olivia); in PSK it snaps
  onto the signal underneath when the knob stops. The MFK gives 1 Hz (PSK)
  or 5 Hz (Olivia) steps.
- Received text in the panel under the waterfall, sent text in the TX
  colour; the size follows Settings > Display > Decoder text, as RTTY and CW.
- Mode (page 1): PSK31 (31.25 Bd) or PSK63 (62.5 Bd), same Varicode; Olivia
  8/250 (default), 16/500 or 32/1000. The status button shows the audio
  frequency and the lock quality; press resyncs, hold clears the text.
- PSK squelch (page 2, SQL): text is shown only while the decoder's quality
  (how tightly the symbols sit on 0 and 180 degrees, the Q on the button)
  is at the threshold or above, and for 1.5 s after it drops. Noise alone
  reads about 50 (never above 64 in the tests), a readable PSK31 signal 70 and
  more. On at 68 by default, so noise prints nothing. Press, then the MFK sets
  it (0 = off); hold toggles off/on.
  Olivia needs no squelch: it prints only once it has locked (a few blocks,
  about 8 s at 8/250).
- Dial (page 2): the MFK moves the VFO in 1 kHz steps, up to 50 kHz either
  side; hold goes back to the standard frequency.
- DX call and RST (page 2): the DX call is taken from "DE <call>" in the
  received text, or typed in; hold clears it.
- TX: Free Text, or the macros (pages 3-5: press sends, hold edits; an empty
  text deletes). Tokens `<MYCALL>`, `<CALL>`, `<RST>`, `<RSTR>`. Stop TX ends
  with a short carrier tail (PSK) or after the current block (Olivia). Nothing is sent on
  opening. TX goes through the same path as FT8: power capped at 7 W, ALC
  levelling, SWR protection, the TX strip.
- Files: `/mnt/psk_macros.txt`, `/mnt/olivia_macros.txt`, `/mnt/psk.conf`
  (squelch).

**In italiano.** Modi tastiera-tastiera, dal lavoro di SP2KMO su OpenX6100
(merge di randal007). APP > Modes > PSK31, PSK63 o Olivia: la radio va sulla
frequenza standard più vicina in USB-D, i tasti BAND passano alle altre; alla
chiusura tornano frequenza e modo di prima. Il testo ricevuto sta nel riquadro
sotto il waterfall. La manopola principale sposta la
banda di ascolto sul waterfall (in PSK si aggancia al segnale quando si
ferma), l'MFK la regola fine. Mode: PSK31 o PSK63, Olivia 8/250 (predefinito),
16/500, 32/1000. Lo squelch PSK (pagina 2, SQL) guarda la qualità della
demodulazione (Q): parte acceso a 68, col solo rumore (Q intorno a 50) non
scrive nulla; si regola con l'MFK, 0 lo spegne. Olivia scrive solo
dopo l'aggancio. In TX: Free Text o le macro (pagine 3-5, premere invia,
tenere premuto modifica); la potenza resta a 7 W al massimo, con ALC e
protezione SWR come in FT8. All'apertura non trasmette mai.

### RTTY and WeFax TX

RTTY and WeFax transmit only from the [X6100 Link app](#x6100-link-app); the
radio has no local TX button for them.

- RTTY TX: ITA2 AFSK with the RTTY app's rate, shift, centre and reverse,
  through the same TX player.
- WeFax TX: a picture in grey, 1809 px wide (IOC 576), 120 LPM, with APT
  start and stop tones and phasing, at most 800 lines. Pictures go to
  `/mnt/wefax/tx_pics` (uploaded from the phone or the Gallery).
- NavTex is receive only.

## CW

### CW decoder

In CW mode the decoder buttons are on the CW pages (CW 1:2, 2:2): Decoder
on/off, Tuner (tuning indicator), Dec SNR, CW peak and CW peak Q. Text and
speed (WPM) appear in the panel over the waterfall.

- The receiver comes from gdyuldin x6100_gui 1.0.1: a coherent tone tracker
  that follows the station inside the CW filter, a detector with a noise
  floor, and a time classifier that learns the speed.
- On test signals it copies 98-100% from 20 dB down to 0 dB SNR (in 2.5 kHz)
  and still 97-100% at -3 dB; the old decoder stopped at about +5 dB. The WPM
  reading is within about 0.5 WPM.
- Dec SNR (5-30 dB, default 8) is how far over the noise floor a mark must
  be. Lower copies weaker signals but can print noise.
- No characters from noise: lone blips, clicks and steady carriers (birdies)
  on the tone are not decoded and do not change the speed estimate.
- A letter shows when the next one starts; the last one after a short pause.
  For 1 s after the decoder is reset (first use, CW peak or key tone change)
  nothing is decoded. A key-down longer than 8 dots (tune-up) is ignored.
- Upstream bugs fixed here: a strong station slightly off tune was never
  decoded, and a dash cut by a retune came out as a dot.

### CW from the keyboard

With a USB or Bluetooth keyboard you key CW straight from the main screen.
Pair a Bluetooth keyboard in Settings > Wireless > BLUETOOTH; it is found
again when it reconnects.

- It works in CW or CW-R, with the VFO inside a CW segment of the band plan
  (the CW part of the strip over the waterfall) and no app open.
- A letter or digit opens a thin line over the bottom of the waterfall with
  what you type. Backspace corrects, Esc clears it (and stops sending).
- Enter keys the line. Lines typed while sending wait in a queue. The line
  on the air is bright up to the key and highlighted after it, the queue is
  grey.
- On the left of the line, the yellow MFK tag with the key speed and an
  arrow to the corner: while the line is open the MFK turns the speed (the
  same as KEY > Speed), and goes back to its control when the line closes.
- When everything is sent the line goes away. Out of CW mode or out of the
  CW segment nothing new is sent and the line is cleared.
- F1 to F5 key the MSG > CW messages 1 to 5 (in list order) as if typed
  and sent with Enter; Esc stops them. An F key with no message does nothing.
- In a CW message `{CALL}` is your callsign (Settings > General > STATION),
  also with TX in MSG > CW; with no callsign set it stays as written.
- Until the line is open, the other keys (- = and so on) work as usual; in
  CW [ and ] are the paddle (below).

The X6100 Link app's CW tab uses the same keyer. Its F1-F5 buttons show
the start of the same five messages: a tap keys one with `{CALL}` filled in,
a long press edits it.

### TinyMIDI / Bluetooth paddle

The N6ARA TinyMIDI paddle works as a Bluetooth keyboard.

- Put its mode switch on the left (Keyboard Mode) and pair it in Settings >
  Wireless > BLUETOOTH like any keyboard: it is a Low Energy one, the scan
  finds it as *keyboard*.
- In CW or CW-R the dit contact (the `[` key) and the dah contact (`]`) key
  the radio. Out of CW they are the volume keys again.
- It follows the KEY settings: Speed, Iambic A or B, the dit:dah ratio and
  the key mode: Manual makes the dit contact a straight key, Auto-R swaps
  the contacts. Train keys only the sidetone, as with a wired paddle.
- A touch of the paddle stops a CW message or line on the air, as a break.
- The touch paddle goes up to 30 WPM, a paddle on its 3.5 mm jack up to 40.
  Any USB keyboard's [ and ] keys do the same.

### CW messages

The MSG > CW messages are sent with TX or as a beacon.

- Edit them on the radio (MSG > CW) or on the web configurator's CW messages
  page (Operating menu): add, edit, delete, 1-64 characters each. The radio
  shows the changes the next time MSG > CW opens.
- While a message or beacon is sent, the break-in delay is raised (one word
  space plus 150 ms, at most 1 s), so the transmitter stays keyed through
  the gaps instead of clicking the relay. It goes back to your setting between
  beacons and when the message ends.
- They also appear as macros in the X6100 Link app's CW tab.
### CW Trainer

APP (page 2) > CW Trainer keys with the radio's own keyer, so it follows the
KEY settings. While it is open the radio is in train mode: sidetone only, no TX.

- KEY button, page KEY 2:2:
  - Mode: Manual is a straight key, only one paddle contact works. Auto-L and
    Auto-R are the iambic keyer for a paddle; L and R swap which side gives
    dits and dahs.
  - Iambic: A or B.
  - QSK time and Ratio are on the same page.
- With a paddle, set Mode to Auto-L or Auto-R before opening the trainer. On
  Manual only one side of the paddle keys.


## Apps

### DX Cluster

Needs WiFi (Settings > Wireless) and your callsign (Settings > General >
STATION), used to log in.

- The client connects in the background as soon as both are there. It
  reconnects by itself (with a backoff) after an error or when WiFi comes
  back.
- Spots older than 60 minutes are dropped. After login it asks for the latest
  50 spots (`sh/dx`).

Cluster app (APP > Cluster; pages `(CLUSTER 1:2)` and `(CLUSTER 2:2)`):

- The spots of the current band, sorted by frequency, with a VFO marker row
  among them. The list stays centred on the VFO: tuning scrolls it and
  selects the nearest spot. Spots within 3 kHz of the VFO are highlighted; a
  green `*` marks spots less than 2 minutes old.
- Turning the MFK browses the list (this stops the VFO following). An MFK
  press, the Tune button or a tap on a row tunes to the spot. The mode comes
  from the comment (CW, FT8, SSB, ...) or else from the band plan: CW, USB-D
  on the FT8/FT4 frequencies, LSB below 10 MHz, USB above.
- Page 1: Band / All switches between the current band and all bands (always
  all when the VFO is outside the amateur bands); the button shows the view
  (`Band / 20m`, `Band / All`). Tune, Mode (All > CW > SSB > DIG, a quick
  view filter), Close.
- Page 2: Server opens the list of saved servers: MFK to choose, press (or
  tap) to use it, Server again or ESC to close. The choice is saved in
  `/mnt/cluster.conf`. Order: by frequency around the VFO (`Freq`) or newest
  first (`Time`).
- Order and band view are saved and shared with the cluster column.
- The status line shows the WiFi and cluster state ("Set your callsign",
  "WiFi not connected", "Connecting...", "cluster connected", or the error
  and "retrying").

Logging a spot. When you tune a spot with the MFK (in the app or the cluster
column), the X6100 Link app opens its logger with the call filled in (see
[X6100 Link app](#x6100-link-app)).

Filters and alerts. They are saved in `/mnt/cluster.conf`, shared by the
radio and the web configurator, and apply to the app, the column and the
waterfall.

- On the radio: modes (CW, SSB, DIG, AM/FM), bands (160-6 m, all on = no
  filter) and activity (All spots, POTA, SOTA, POTA + SOTA). One mode and one
  band always stay on. A change applies at once.
  Where: Settings > Cluster > CLUSTER FILTERS.
- POTA / SOTA come from the spot comment: the word POTA or SOTA, a park
  reference (`US-1234`, `IT-0123`) or a summit reference (`I/LO-123`,
  `W7W/KG-001`). WWFF references (`IFF-0123`) are not taken as POTA.
- On the web configurator (Cluster page): the same modes, bands and activity,
  plus maximum age and spotters to skip (`*` wildcard).
- Alerts (web configurator): callsigns or prefixes (`3B8M`, `VP8*`, `*/MM`),
  optionally only on some bands or one mode. Matching spots are magenta with
  an `ALERT` tag and are never filtered out. A new one pops up on the main
  screen for 10 s (an MFK press tunes it).

On the main screen. Where: Settings > Display > MAIN SCREEN.

- Cluster spots: a short cyan line at each spot frequency at the top of the
  waterfall (across the band plan strip), with the callsign under it, in two
  staggered rows on a dark see-through background. Alert calls are pink. The
  waterfall does not move.
- Cluster column: a box at the right of the waterfall (80 % opaque) with the
  spots (kHz, callsign, mode, age) around the VFO marker row, coloured by
  age: green under 2 minutes, yellow under 10, amber under 30, grey older.
  The title shows the view (`Cluster 40m - Freq`). In time order the newest
  spot is on top and there is no VFO marker row.
- While the column is shown, the MFK has two more items, Cluster order
  (Freq / Time) and Cluster bands (Band / All), the same settings as in the
  app.

Cluster view. Where: Settings > Cluster > CLUSTER VIEW.

| Option | What it does |
|---|---|
| Newest first | the same as Cluster order |
| All bands | the same as Cluster bands |
| Spot snap hold (0-5, 0 = off) | with the cluster spots shown, turning the main knob slowly stops the VFO on a spot it passes for this many more detents |
| Spot snap speed (1-5) | how slow counts as slow: at most this many detents every 33 ms. Turning faster goes past. |

Without the cluster spots on the waterfall there is no snap.

Servers. If `/mnt/cluster.conf` has no `server=` lines the app lists eleven
public clusters (DXFUN, VE7CC, W3LPL, K1TTT, NC7J, W1NR, WA9PIE, K4ZR,
DB0SUE, GB7DJK, AE5E). The default is DXFUN, `dxfun.com` port 8000. Add and
remove servers from the [web configurator](#web-configurator) or in the file
([format](#files-on-mnt)).

### Field Ops (SOTA / POTA)

A full-screen interface for portable activations: APP (page 2) > Field Ops,
or a long-press action. GEN or Radio goes back to the normal screen.

- Everything (the reference, the activation running, its log, the screen you
  were on) is kept on the SD card in `/mnt/field`. Coming back, or a restart
  in the middle of an activation, finds it as it was.
- Start here (home screen) opens Field Ops at power on.
- The main knob still tunes; MFK and touch choose.

| Home | SOTA, before the activation |
|---|---|
| ![Field Ops home](screenshots/field_home.png) | ![SOTA mode](screenshots/field_sota.png) |
| **Activation** | **Bands** |
| ![Activation](screenshots/field_activation.png) | ![Band overview](screenshots/field_bands.png) |

Home: the SOTA, POTA and Bands tiles. MFK to choose, a press (or a tap)
opens.

SOTA / POTA, before the activation:

- On the left the reference: summit or park, name, altitude and points
  (SOTA), locator, coordinates and where the position comes from. On the
  right the nearest references with their distance. The nearest one is
  taken; the MFK and a press choose another; Ref... (page 2) types one.
- The position comes from the radio's GPS, else from the phone (the X6100
  Link app sends it), else from the QTH locator. The dot in the header is
  green for a real position (GPS, phone), yellow for the locator.
- Update list (page 2) downloads the parks (POTA) and summits (SOTA), about
  35 MB once, in about 3 minutes; then it works offline. Also from the web
  configurator's Field Ops page.
- Start begins the activation and its ADIF log.

SOTA / POTA, during the activation: the band, the frequency and the mode,
the S-meter, the CQ text with the reference, the QSO count, the time since
the start and the last call.

- CQ sends `CQ CQ SOTA DE <call> <call> K` in CW (POTA in a park). In SSB it
  sends your voice CQ: record it once with Rec CQ (page 2, before or during
  the activation): press, speak, Stop rec (60 s at most). It records from the
  hand mic only (see [recording voice messages](#recording-voice-messages)). It is `CQ SOTA` /
  `CQ POTA` in `/mnt/msg`, so MSG > Voice lists it too. CQ again while it is
  on air stops it.
- Log QSO: type `CALL`, or `CALL rst_rcvd rst_sent` (default 599 in CW, 59
  otherwise). Each QSO goes to the SD card at once, in
  `/mnt/field/logs/<date>-<time>_<ref>.adi` with `MY_SOTA_REF` or
  `MY_SIG POTA` / `MY_POTA_REF`. The logs can be downloaded from the web
  configurator's Field Ops page.
- The mode logged is FT8 or FT4 when the radio is on an FT8/FT4 frequency,
  also in USB.
- Spot: a POTA self-spot on pota.app with the frequency and mode of the radio
  (WiFi needed). The first press shows what will be posted and the button
  reads Spot?; press again within 4 s to post.
- In POTA the radio also spots by itself: when the activation starts, and
  after a QSY (1 kHz or more away, or another mode) once you have stayed
  there a minute. At most one automatic spot in 5 minutes; pota.app keeps
  only your latest spot, so the old frequency goes away. These QSY spots go
  to pota.app only: a DX cluster spot can't be withdrawn.
- SOTAwatch spots are not available yet (they need an API client approved by
  SOTA): post an alert on SOTAwatch and call CQ in CW, RBNHole spots you from
  the Reverse Beacon Network.
- End (page 2), pressed twice, ends the activation. In POTA, if the
  activation was spotted from the radio, End also posts a QRT spot on
  pota.app (WiFi needed).

X6100 Link app: while the radio is in Field Ops the phone shows its SOTA/POTA
page by itself: the reference, the nearby ones to choose, Start and End, the
QSO entry with the phone's keyboard (call, RST sent and received), the QSO
list, Spot. The phone's position goes to the radio when Send phone position
is on. With a QRZ.com login (SET > QRZ.com) the app looks up the call as you
type it and shows name, QTH, grid and country under the entry; LOG puts them
in the ADIF (NAME, QTH, GRIDSQUARE, COUNTRY). The full record needs a QRZ XML
subscription.

Bands screen: for each band the cluster spots of the last 30 minutes, the
Reverse Beacon Network skimmer spots of the last 15 and the skimmers that
heard you in the last 10 (with the best SNR); the activity as a bar and
HIGH / GOOD / FAIR / POOR, the best band marked.

- Test beacon sends `TEST DE <call>` in CW (the radio must be in CW), so you
  can see who hears you.
- Use band (or a press) tunes a typical activation frequency of the band.
- RBN is connected while the screen is open (the first minute fills the
  counts); it needs WiFi and the callsign.

No POTA or SOTA spot goes to the DX cluster: neither program reads them
there.

### Bands

The band overview of Field Ops as an app of its own, in the look of the other
apps: APP (page 3) > Bands, or a long-press action.

![Bands app](screenshots/bands_app.png)

- Each band shows its activity bar and word, the cluster / RBN spot counts
  and who heard you (skimmers, best SNR); the best band is framed.
- MFK to choose, a press (or Use band) tunes a typical frequency of the band
  (CW or phone, as the radio's mode).
- Test beacon sends `TEST DE <call>` in CW. ESC closes.
- The numbers are the same as in Field Ops.

## Bluetooth

### Bluetooth headphones

Radio RX audio to A2DP headphones. Pairing and connection are in Settings >
Wireless > BLUETOOTH; the audio switches are in Settings > Audio > BT
HEADPHONES.

1. Settings > Wireless: switch Bluetooth on, put the headphones in pairing
   mode and press Scan headphones (16 s).
2. An MFK press on a device marked *new* pairs, trusts and connects it; it
   also opens the pairing window (the Pairing switch counts down). What it is
   doing, or why it failed, shows next to Scan headphones. On a paired device
   the menu offers Connect / Disconnect, Connect at startup and Remove.
3. Settings > Audio > BT HEADPHONES, with the headphones connected:

| Option | What it does |
|---|---|
| RX to headphones | a PulseAudio loopback from the radio RX audio (`alsa_input.platform-sound.stereo-fallback`) to the headphones |
| Volume | the headphones sink volume |
| Mute speaker | AF volume 0 while the headphones are connected, back to the previous value when they disconnect. The AF volume does not change the audio sent to the headphones. |

- Only A2DP is used: HFP/HSP (SCO) gives no audio on this radio and drops the
  WiFi, so PulseAudio has no headset backend
  (`rootfs/etc/pulse/system.pa.d/btaudio.pa`). The same file stops PulseAudio
  from looping a phone or tablet that connects for CAT (it also appears as an
  A2DP source) into the radio TX audio.
- Bluetooth keyboards (for [CW from the keyboard](#cw-from-the-keyboard)) are paired the
  same way: Scan headphones also finds keyboards, marked *keyboard* in the
  list. After the MFK press, press the keyboard's pairing key when asked; if
  it asks for a code, the code to type (then Enter) shows next to Scan
  headphones. A keyboard that takes a new address at each pairing is found
  again by its name.
- Pairings are kept in a backup on `/mnt` and restored at boot
  (`S39btpairings`), so a new image does not forget them.

### Bluetooth CAT

`btcat` (`btcat/btcat.c`) opens the real CAT UART (`/dev/ttyS2`) and
bind-mounts a pty over it, so the GUI talks to btcat. USB CAT and up to 3
Bluetooth clients share the CAT port: one request at a time, replies go back
to whoever asked, transceive frames (dst `0x00`) go to everyone.

Where: Settings > Wireless > BLUETOOTH.

| Option | What it does |
|---|---|
| Bluetooth | adapter power |
| Pairing (3 min) | visible and pairable for 3 minutes, then off by itself; the label counts down |

CAT accepts paired devices only (RFCOMM security level medium): a device in
range that has not paired cannot open the port. Outside the pairing window
the radio is hidden, not pairable and its pairing agent refuses every
request; paired devices still connect. To pair a phone or PC once: switch Pairing on, then pair from
the phone or PC within 3 minutes (Just Works, no PIN). When the window closes
the new devices are marked trusted and the pairings are saved on `/mnt`.
Pairing a keyboard or headphones from the device list opens the window too.
On Linux: `rfcomm connect /dev/rfcomm0
<BT-MAC> 1`, then use `/dev/rfcomm0`. On Windows and Android the paired
device exposes an outgoing COM/SPP port. Log files: `/tmp/btcat.log`,
`/tmp/btcat-setup.log`. Connection details: [reference](#bluetooth-cat-connection).

## Themes

Every colour of the GUI comes from a role (background, button, selected row,
header text, OK, TX, ...). A theme gives each role a colour, plus corners,
borders, gradients, the font and the waterfall palette.

Where: Settings > Display > INTERFACE > Theme (also on the web
configurator's Display page). The radio app restarts at once with it (a few
seconds; the settings are saved first).

| Theme | Look |
|---|---|
| IU1WXY | default: dark blue-grey, cyan headers |
| Classic | black, grey keys, IC-7300 style |
| Outdoor | for the sun: light, black text, grey waterfall, S-meter in one grey gradient |
| Outdoor-Negative | Outdoor with the lightness inverted: dark, white text, the hues kept |
| VISION | for weak eyes: big type, white digits on dark grey, orange for what is on, aqua frames; the S-meter on a row of its own, always bars |
| Black Metal | designed by 2E0JFS: two rows of state on top (ATT PRE, NR NB DNF, AGC, SPL ATU, RIT XIT; mode, filter, BW, IF, STEP, scope span), S/SWR/ALC scales beside a big frequency with the 10 Hz digits smaller, the other VFO and an A/B badge, the span edges in the spectrum, one-line buttons; brushed grey panels, cyan for the tuned digit and the encoder's control, Roboto |
| Total Black | designed by 2E0JFS: Black Metal on pure black, the buttons marked by a thin grey line on top |
| RS | RS-BA1 look: glossy buttons with LEDs, black LCD, Arial; drawn with images |
| your files | `/mnt/themes/<name>.json`, or a folder `/mnt/themes/<name>/theme.json` with PNG images |

- A theme file sets only what it changes; the rest comes from its `base`
  theme.
- A folder theme can draw the background, the buttons (normal, active,
  disabled, page), the top bar, the frequency strip, the app windows and the
  encoder LED with PNG images, 9-sliced to the size of each surface.
- RS is such a folder: it is installed with the image and copied to
  `/mnt/themes/RS` at the first start (`themes/RS`, generated by
  `themes/make_rs.py`).
- Legacy is kept in the code but not offered in the menu.
- File format, images and the list of roles:
  [docs/THEMES.md](THEMES.md).

Encoder LED. The bottom buttons show which encoder their control is on. The
control the encoder adjusts now has a red LED, which follows button presses
and encoder changes. A theme can draw the LED with an image.

Fonts. Sony Sketch, Roboto and DejaVu (default) are in Settings > Display >
INTERFACE > Font. Arial is set by a theme's `font` (the RS theme uses it);
its LVGL sizes are generated with `tools/font_gen.py`.

## Web configurator

A small web server (vendored in [webserver/](../webserver/), Python/bottle) runs
on the radio on port 80. Open `http://<radio IP>/` from any browser on the
same network. The radio picks up changes within a few seconds.

The menu has General and four groups:

| Group | Page | Does |
|---|---|---|
| | General | index of the pages |
| Frequencies | Bands | band edges and default modes |
| | Digital modes | add, edit and delete the FT8/FT4 and JS8 frequencies |
| | Memories | MEM 1-9 slots and the named channel list |
| Operating | Remote | tune the radio from the browser: frequency, mode, band, filter, RF gain, PRE/ATT/AGC/NB/NR, live spectrum and waterfall (click or drag to tune), RX audio (HTTPS page, one listener). Receive only (no PTT); transmitting remotely is done by the [X6100 Link app](#x6100-link-app) |
| | Cluster | DX cluster servers, filters and alerts (writes `/mnt/cluster.conf`) |
| | Field Ops | QRZ Logbook API key and upload, update the parks and summits list, download the activation logs |
| | CW messages | the MSG > CW messages: add, edit, delete (1-64 characters) |
| | Logs | every QSO on the radio, day by day: edit, delete, export as ADIF or CSV |
| Files | Files | browse and download `/mnt` |
| | Gallery | screen recordings, voice recordings, screenshots, SSTV and WeFax pictures (see below) |
| | Backup | download settings, memories, cluster, wfview, QSO log, voice messages (optionally CW trainer and WiFi) as one file, and restore the parts you choose |
| System | Display | every display option of the radio (see below) |
| | Time | local and server time, NTP sync, timezone |
| | wfview | enable the wfview server, set user and password |

Gallery: a folder per kind (screen recordings, voice recordings, screenshots,
SSTV and WeFax received and to send) with count and size.

- Multi-select (tap, checkbox, shift for a range, all/none); delete the
  selection with one confirmation or download it as a ZIP.
- Upload several pictures into the SSTV and WeFax TX folders (other formats
  are converted to PNG in the browser).
- Convert AVI to MP4 in the browser (HTTPS page only) or on the radio.

Display page (System > Display): the same options as Settings > Display,
applied on the radio as you change them.

| Card | Options |
|---|---|
| Theme | Theme (the radio's screen restarts with the new one) |
| S-meter | S-meter type (Bars, Needle), Colours (Theme, Scale, Level), Needle lamp; with a preview |
| Waterfall | Mode (Centered, Fixed), Speed (1-5), Auto level, Auto level offset (-15 to +15 dB), Center line, Zoom, Smooth scroll |
| On the waterfall | Band plan, Cluster spots, Cluster column |
| Backlight | Brightness, Brightness when idle, Idle after (5-120 s), Button lights (Always off, Always on, Temporarily on) |
| Text and info | Knob info, Magnify frequency, Magnify info, Magnify ALC |

HTTPS. The server also listens on port 443, with a self-signed certificate
made on first start (valid 10 years, stored in `/mnt/https/`). The browser
shows a certificate warning once; accept it. In-browser MP4 conversion in the
Gallery only works on the HTTPS page (WebCodecs needs a secure context);
"Convert on radio" works on both.

Server options: `--host`, `--port`, `--https-port` (0 = off), `--cert-dir`,
`--db`, `--cluster-conf`, `--wfview-conf`, `--channels-file`,
`--filebrowser-path`, `--debug`; see
[webserver/README.md](../webserver/README.md).

| Cluster | Digital modes |
|---|---|
| ![Cluster page](screenshots/web/cluster_desktop.png) | ![Digital modes page](screenshots/web/digital_modes_desktop.png) |

The same server carries the remote control used by the X6100 Link app.

## X6100 Link app

X6100 Link is a separate Android app (not part of this repository) for this
firmware: the radio decodes and transmits, the phone or tablet shows and
commands it over WiFi. An app older than the firmware is refused, and the app
asks for a newer firmware when the radio's is older.

Connect:

1. Turn on the phone's hotspot.
2. Connect the radio to it (Settings > Wireless).
3. Open the app. It tries the last radio, otherwise scans the phone's
   networks for `/api/info`. Tap the radio found, or type its IP.

What it has:

| Part | What it does |
|---|---|
| Radio | frequency, band, mode, tune steps, VFO knob, P.AMP/ATT/NB/NR/AGC, S-meter, spectrum and waterfall (tap or drag to tune), RX audio |
| Log (the default) | call, RST and LOG into the radio's log of the day (`YYYYMMDD.adi` with the Field Ops logs, downloadable from the web configurator). With a QRZ.com login the call's record shows under it (name, QTH, grid, country and flag, zones, distance, photo) and goes into the log. SOTA/POTA opens Field Ops. |
| Apps | all eight, with TX in all but NavTex: FT8/FT4 (CQ, reply, auto, hold, TX frequency), JS8, WSPR, SSTV (pictures from the phone's gallery or a camera photo, call and text), WeFax (received faxes full screen, zoom, save, share; send a picture), NavTex (receive only), RTTY and CW (text to send, macros with `{CALL}`, the characters already on the air in cyan) |
| Themes | Classic, Pro, Outdoor and Outdoor-Negative |
| Languages | English or Italian (the app's settings) |

- VFO lock: while an app is open on the radio, frequency, band, mode and
  tap-to-tune are locked (padlock to unlock). BASE goes back to the app's
  frequency and mode (*OFF BASE* blinks when the VFO is elsewhere). In
  FT8/FT4 a tap on the waterfall sets the TX audio frequency.
- Portrait (radio on top, app below) and landscape (side by side); rotation
  keeps the connection.
- FT8 and JS8 "For you": up to 3 recent messages to you stay pinned under the
  list for 2 minutes (in FT8 a caller also leaves after your 73 or RR73). A
  tap replies.

Logging a DX cluster spot. Tap a spot in the app's cluster column, or tune a
spot with the MFK on the radio: the app tunes it and opens the logger with
the call filled in, the RST at 59 (599 in CW) and, with a QRZ.com login, the
QRZ data. Nothing is logged until you tap LOG.

Transmitting from the phone: the app's TX commands (CQ, reply, send...) work
as soon as the app is open on the radio. Stop always works. When an app
opened from the phone changed the mode (CW, or USB for SSTV/WeFax/NavTex),
the mode comes back when the app closes.

Memories (MEMORIES, next to LOG/DIGITAL/CW). The radio's MEM slots M1-M9 and
its named channels, the same ones as on the radio and on the web
configurator's Memories page. Tap one to tune it (frequency, then mode).
Hold it down for its actions: SAVE VFO stores the radio's frequency, mode,
AGC, P.AMP and ATT in that slot, CLEAR empties it; a channel can be renamed,
moved up or down, or deleted (asked once more). Type a name and SAVE VFO to
add the VFO as a new channel. Needs firmware v0.125 or newer.

**In italiano.** Memorie (MEMORIE, accanto a LOG/DIGITALE/CW): gli slot MEM
M1-M9 e i canali con nome della radio, gli stessi della radio e della pagina
Memories del configuratore web. Un tocco sintonizza (frequenza, poi modo).
Tenendo premuto compaiono le azioni: SALVA VFO mette nello slot frequenza,
modo, AGC, P.AMP e ATT della radio, SVUOTA lo libera; un canale si rinomina,
si sposta su o giù, si elimina (con conferma). Scrivi un nome e SALVA VFO per
aggiungere il VFO come nuovo canale. Serve il firmware v0.125 o successivo.

<table>
  <tr>
    <td align="center"><img src="screenshots/android/ft8_portrait.png" alt="FT8 portrait" width="240"><br><sub><b>FT8</b>, portrait</sub></td>
    <td align="center"><img src="screenshots/android/cw_portrait.png" alt="CW" width="240"><br><sub><b>CW</b>: macros, text on the air</sub></td>
    <td align="center"><img src="screenshots/android/wefax_portrait.png" alt="WeFax" width="240"><br><sub><b>WeFax</b>: received faxes, picture TX</sub></td>
  </tr>
  <tr>
    <td colspan="3" align="center"><img src="screenshots/android/ft8_landscape.png" alt="Landscape" width="600"><br><sub><b>Landscape</b>: radio on the left, app on the right.
    App screenshots taken on an emulator against a mock radio (fake data).</sub></td>
  </tr>
</table>

## wfview server

The radio can act as an Icom IC-705 on the network, so
[wfview](https://wfview.org) on a PC controls it with CI-V, RX audio and the
waterfall.

- Off by default. Enable it in Settings > General > WFVIEW SERVER or on the
  web configurator's wfview page.
- Default login `x6100` / `x6100`: change it in the same places (stored in
  `/mnt/wfview.conf`).
- Ports: UDP 50001 (control), 50002 (CI-V), 50003 (audio).
- In wfview add a network radio (IC-705 profile): radio IP, control port
  50001, the user and password above, audio codec LPCM 1ch 16bit.
- The WFVIEW SERVER card shows the connected client.

## Troubleshooting and FAQ

#### FT8, JS8 or WSPR decode nothing, or nobody answers

Check the clock: FT8 and JS8 need it within about 2 s, WSPR within about a
second. WiFi (NTP) or GPS sets it. FT8 and JS8 also have Time Sync.

#### The ALC reads about 1 in FT8 or JS8. Is it too high?

No. The TX level settles with the ALC working a little, with the power at the
setting. It comes down by itself over 3. See [Digital-mode TX level](#digital-mode-tx-level).

#### I can't record a voice message with the built-in microphone

The radio's baseband records only from the hand mic: plug it in to record. See
[recording voice messages](#recording-voice-messages).

#### The power drops to 7 W when I open a digital app

That is the cap for the digital apps. Your setting comes back when you close
the app.

#### The DX Cluster shows no spots

Read the status line in the Cluster app. It needs WiFi and your callsign.
Check also the filters (Settings > Cluster): one mode and one band always
stay on, so a narrow choice can hide everything.

#### The MP4 conversion does not work in the Gallery

Open the web configurator on HTTPS (`https://<radio IP>/`) and accept the
certificate once, or use "Convert on radio".

#### A Bluetooth headset gives no audio for the microphone

Only A2DP (listening) is used: HFP/HSP gives no audio on this radio and drops
the WiFi.

#### The new font did not change anything

RS, Black Metal and Total Black have their own font and keep it: pick another theme
to see the font you chose.

#### The main screen's waterfall stands still

That is normal while JS8 is open.

#### The Diagnostics Send... says "not accepted now"

Ask IU1WXY first: the text is accepted only while reception is open. See
[Info tab](#info-tab).

#### Where are my logs?

FT8/FT4 in `/mnt/ft_log.adi`, JS8 in `/mnt/js8call_log.adi`, Field Ops in
`/mnt/field/logs/`, the app's logger in the day's log. The web configurator's
Logs page shows them all.

## Reference

### Bluetooth CAT connection

| | |
|---|---|
| Transport | Bluetooth SPP, RFCOMM channel 1 (or USB CAT) |
| SPP UUID | `00001101-0000-1000-8000-00805F9B34FB` |
| Pairing | required once, in the 3-minute pairing window; Just Works, no PIN; device name `X6100` (`BTCAT_ALIAS` in `/etc/default/btcat`) |
| Protocol | Icom CI-V, radio address `0xA4` |
| Frame | `FE FE A4 <ctrl> <cmd> [sub] [data] FD`; reply `FB` = OK, `FA` = NG |
| Baud (USB) | 19200 8N1 (ignored over BT) |
| Hamlib / flrig / wfview | rig model Xiegu X6100 |

### Supported CI-V commands

| Cmd | Sub | Function | Read | Write |
|---|---|---|:-:|:-:|
| `00` | - | Frequency (transceive, sent automatically on every change) | - | yes |
| `01` | - | Mode (transceive, sent automatically on every change; mode byte only, no filter) | - | - |
| `03` | - | Read frequency | yes | - |
| `04` | - | Read mode | yes | - |
| `05` | - | Set frequency (5 BCD bytes) | - | yes |
| `06` | - | Set mode (`00` LSB, `01` USB, `02` AM, `03` CW, `05` NFM, `07` CW-R) | - | yes |
| `07` | - / `00` / `01` / `A0` / `B0` | VFO: read current, VFO A, VFO B, A=B, swap A/B; `07 00` / `07 01` also sent automatically when the VFO changes (transceive) | yes | yes |
| `0F` | - | Split off/on | yes | yes |
| `10` | - | Tuning step (`00` 10 Hz, `01` 100 Hz, `02` 500 Hz, `03` 1 kHz, `04` 5 kHz) | yes | yes |
| `11` | - | Attenuator | yes | yes |
| `14` | `01` | AF volume (0-255) | yes | yes |
| `14` | `02` | RF gain (0-255) | yes | yes |
| `14` | `03` | Squelch (0-255) | yes | yes |
| `14` | `09` | CW pitch (0-255 = 300-900 Hz) | yes | yes |
| `14` | `0A` | TX power (0-255 = 0-10 W) | yes | yes |
| `14` | `15` | Monitor level | yes | - |
| `15` | `02` | S-meter (Icom scale, 120 = S9, for network clients) | yes | - |
| `15` | `11` | TX power meter | yes | - |
| `15` | `12` | SWR meter | yes | - |
| `15` | `13` | ALC meter | yes | - |
| `16` | `02` | Preamp | yes | yes |
| `16` | `22` | Noise blanker | yes | yes |
| `16` | `40` | Noise reduction | yes | yes |
| `16` | `44` / `46` | Compressor / VOX (always off, write ignored) | yes | - |
| `19` | `00` | Transceiver ID (returns `A4`) | yes | - |
| `1A` | `01` | Band (Icom codes `01` 160 m ... `09` 10 m, `10` 6 m, `15` none); write tunes the band's centre | yes | yes |
| `1A` | `03` | IF filter width (BCD index) | yes | - |
| `1A` | `06` | Data mode + filter; write only adds or drops the data mode (USB ↔ USB-D, LSB ↔ LSB-D) | yes | yes |
| `1C` | `00` | PTT (`00` RX, `01` TX) | yes | yes |
| `25` | `00` / `01` | Frequency of selected / unselected VFO | yes | yes |
| `26` | `00` / `01` | Mode + data mode of selected / unselected VFO | yes | yes |
| `27` | `10` / `11` | Scope on (always) / wave data output: waterfall data (about 30 frames/s) only to network clients (wfview); on serial and Bluetooth a stub | yes | yes |
| `27` | `14` / `15` | Scope centre mode (always centre) / span (follows the waterfall zoom, write sets the zoom) | yes | yes |
| `27` | `19` | Scope reference level (stub) | yes | - |

Anything else returns `FA` (NG).

Example, read frequency: `FE FE A4 E0 03 FD` →
`FE FE E0 A4 03 00 50 07 14 00 FD` (14.075.000 Hz).

### btcat-ctl

| Command | Effect |
|---|---|
| `btcat-ctl bt on\|off` | Bluetooth adapter power (= Settings > Wireless > Bluetooth) |
| `btcat-ctl discoverable on\|off` | Open / close the 3-minute pairing window: visible and pairable, the GUI's agent accepts pairing (= Pairing (3 min)) |
| `btcat-ctl status` | Print power and the time left in the pairing window |
| `btcat-ctl apply` | Re-apply power from `/mnt/btcat.conf`, close the pairing window |
| `/etc/init.d/S94btcat start\|stop\|restart` | Start/stop the bridge (set `BTCAT_ENABLE=no` in `/etc/default/btcat` to disable) |

### btaudio-ctl

Used by Settings > Wireless and Audio for the headphone audio. Scan, pair,
connect and remove are in the GUI itself, straight to BlueZ over D-Bus, with
its own pairing agent (log in `/tmp/bt_pair.log`).

| Command | Effect |
|---|---|
| `btaudio-ctl status` | Headphone audio state (tab separated) |
| `btaudio-ctl save` | Copy the pairings to `/mnt/backups/bluetooth.tgz` |
| `btaudio-ctl autoconnect MAC on\|off` | Connect at startup |
| `btaudio-ctl rx on\|off` | RX audio to the headphones (loopback) |
| `btaudio-ctl volume N` | Headphone volume, 0-100 % |
| `btaudio-ctl mute on\|off` | AF volume 0 while headphones are connected |
| `btaudio-ctl boot` / `monitor` | Started by `btcat-setup`: keeps the headphones on A2DP with the loopback up, connects the "Connect at startup" devices |

### Files on /mnt

The DATA partition, readable on a PC.

| File | What |
|---|---|
| `params.db`, `qso_log.db` | the radio's settings and QSO database |
| `cluster.conf` | DX cluster server in use, saved servers, filters and alerts |
| `cw_trainer.json` | CW trainer statistics and Calls best score |
| `wfview.conf` | wfview server on/off, user and password |
| `btcat.conf` | Bluetooth power, written by `btcat-ctl` |
| `btaudio.conf` | headphones settings (RX loopback, speaker mute, Connect at startup devices) |
| `ft_log.adi`, `js8call_log.adi` | FT8/FT4 and JS8 logs (ADIF) |
| `ft8_udp.conf` | FT8/FT4 WSJT-X UDP output: `enabled=`, `host=`, `port=`, `id=` |
| `app_freqs` | the last frequency and mode of each digital app |
| `psk_macros.txt`, `olivia_macros.txt`, `psk.conf` | PSK and Olivia macros, PSK squelch |
| `js8_inbox.txt`, `js8_held.txt`, `js8_texts.txt` | JS8 inbox, messages held for others, INFO/STATUS/alert words/groups |
| `incoming_log.adi` | put an ADIF log here to import it at the next start |
| `field/` | Field Ops state and activation logs (`field/logs/`) |
| `screenrec/` | screen recordings (AVI) |
| `themes/` | theme files and folders ([docs/THEMES.md](THEMES.md)); RS is copied here at the first start |
| `sstv/` | received SSTV pictures; `tx_pics/` pictures and `tx_text/` text templates to send |
| `wefax/` | received faxes; `tx_pics/` pictures to send |
| `https/` | the web configurator's HTTPS certificate |

`cluster.conf` example (the `host=`/`port=` lines are the server in use, each
`server=` line is a saved server, `name|host|port`):

```
host=dxfun.com
port=8000
server=DXFUN|dxfun.com|8000
server=NC7J|dxc.nc7j.com|7373
```

Defaults for the Bluetooth bridge are in `/etc/default/btcat`
(`BTCAT_ENABLE`, `BTCAT_BT`, `BTCAT_ALIAS`); the
runtime state in `/mnt/btcat.conf` overrides them.

## Build

GitHub Actions (`.github/workflows/main.yml`, *Build image*, about an hour)
builds the SD image with
[AetherX6100Buildroot](https://github.com/gdyuldin/AetherX6100Buildroot),
using this repository's GUI, the vendored `webserver/`, the boot logo
`boot/logo.png` (800x480) and the Bluetooth files in `rootfs/`.

- Run it manually (`workflow_dispatch`) on `main` for a test image, uploaded
  as the artifact `sdcard.<ref>.img.zip`.
- Push a tag: the same image is attached to a GitHub Release.

Locally: follow [docs/UPSTREAM_README.md](UPSTREAM_README.md): clone
AetherX6100Buildroot next to this repository, configure it with
`./br_config.sh` and `make` in its `build` directory, then run
`buildroot/build.sh` from this repository. The workflow's patch steps show
what to change in the buildroot tree to use this repository (GUI and
webserver from local sources, `boost` and `fftw-single` for JS8, RHVoice
dropped, boot logo).

After a full build, the GUI alone is rebuilt in WSL with `make
x6100-gui-rebuild` (then `make`) in `~/AetherX6100Buildroot/build`. Buildroot
copies the local GUI sources with `rsync -u`, so a newer file edited inside
`build/build/x6100-gui-*/` survives a rebuild: always edit this repository,
never the copy in the build tree.

Tests: `./run_tests.sh` builds and runs the host tests in `tests/` (FT8 QSO,
JS8 with the JSC word tables round trip, QTH, cluster, WSPR round trip down
to -27 dB, CW decoder, Field Ops, SSTV, WeFax, TX level, microphone level). Stand-alone checks, built
with the `gcc` line at the top of each file: `tests/test_rtty_tx.c` (RTTY TX
decoded back by the RTTY receiver, USB/LSB, normal/reverse, 45 and 75 Bd) and
`tools/wefax_loopback.c` (WeFax TX through the real decoder: APT start/stop
and the picture back by correlation).

UI harnesses: [tools/apps_ui_harness](../tools/apps_ui_harness) renders the
app dialogs (FT8, JS8, WSPR, SSTV with TX, WeFax, NavTex, Cluster, CW
Keyboard, Modes, Settings tabs, main screen) headless on a PC, with the real
button bar; `THEME=<name>` renders any theme. The screenshots in this README
come from it. [tools/js8_ui_harness](../tools/js8_ui_harness) runs the JS8 app
the same way under ASan/UBSan.

Design notes and UI mockups are in `docs/` (`docs/mockups/releases/`).
