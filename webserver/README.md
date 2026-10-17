# X6100 web configurator

The radio's web pages (bands, memories, cluster, gallery, backup, remote...) and the API that
X6100 Link uses, served by the radio on port 80 (and 443 for the Remote page).

It started from [gdyuldin/x6100_webserver](https://github.com/gdyuldin/x6100_webserver) v0.0.4,
thanks to Georgy Dyuldin; the code has since been rewritten for this firmware.

## Run it on a PC

Needs Python 3.11 or newer with `bottle` and `bottle-sqlite`:

```bash
python3 -m venv .venv && . .venv/bin/activate
pip install -e .
x6100_webserver --db params.db --port 8080 --https-port 0 --filebrowser-path ./mnt
```

`params.db` is the radio's `/mnt/params.db`. Pages that talk to the radio screen (Remote, Display,
Ethernet) need the GUI's socket and show "Radio GUI not reachable" without it.
