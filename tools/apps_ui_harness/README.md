# Apps UI harness

Renders the app dialogs (FT8, JS8, SSTV, WeFax, NavTex, Cluster) and the APP button
pages headless on a PC, with the real button bar and theme, to compare the
look and feel of apps that come from different forks.

Firmware symbols the harness does not need are auto-stubbed: `gen_stubs.py`
reads the undefined references from a failed link and writes
`build/stubs_auto.S` (functions return 0, data is zeroed). Values that matter
for layout (filter edges, dial) are in `stubs.c`.
`cfg_host.cpp` builds a real `cfg_sm` (default values, no database) so that
Settings can render; Settings → Wireless shows fake networks and Bluetooth
devices from `stubs.c`.

```sh
../js8_ui_harness/fetch_deps.sh      # deps are shared (symlink)
mkdir -p build && echo > build/stubs_auto.S && cd build && cmake .. && \
  (make apps_ui_harness 2>link.log || (python3 ../gen_stubs.py . && make apps_ui_harness))
mkdir -p out && cd out && ../apps_ui_harness [ft8|js8|sstv|wefax|navtex|cluster|msg|settings|wireless|bookmarks|topbar]
```

`topbar` renders the main screen top bar (info, S-meter, clock) in both
themes and the TX meter (`topbar_1..3.ppm`); the Bluetooth tools it polls are
faked by scripts written to `out/fakebin`.
