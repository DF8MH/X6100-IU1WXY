from datetime import datetime, timezone, timedelta
from importlib import resources
import gzip
import json
import os
import pathlib
import re
import sqlite3
import subprocess

import bottle

from . import models
from . import settings

app = bottle.Bottle()

bottle.TEMPLATE_PATH += [
    resources.files('x6100_webserver').joinpath('views'),
]

STATIC_PATH = resources.files('x6100_webserver').joinpath('static')


def read_callsign():
    """Callsign from params.db for the page header, "" when missing."""
    try:
        con = sqlite3.connect(f"file:{settings.DB_PATH}?mode=ro", uri=True)
        try:
            row = con.execute("SELECT val FROM params WHERE name = 'callsign'").fetchone()
        finally:
            con.close()
    except sqlite3.Error:
        return ""
    return str(row[0]) if row and row[0] else ""


bottle.BaseTemplate.defaults.update(callsign=read_callsign, request=bottle.request)


# Bands API

@app.get('/api/bands')
def get_bands(dbcon):
    bands = models.read_bands(dbcon)
    bottle.response.content_type = 'application/json'
    return json.dumps([x.asdict() for x in bands])


@app.put('/api/bands')
def add_band(dbcon):
    data = bottle.request.json
    try:
        band_param = models.BandParams(**data)
        models.add_band(dbcon, band_param)
        bottle.response.status = 201
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


@app.post('/api/bands/<band_id:int>')
def update_band(band_id, dbcon):
    data = bottle.request.json
    try:
        band_param = models.BandParams(id=band_id, **data)
        models.update_band(dbcon, band_param)
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


@app.delete('/api/bands/<band_id:int>')
def delete_band(band_id, dbcon):
    try:
        models.delete_band(dbcon, band_id)
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


# Digital modes routes

@app.get('/api/digital_modes')
def get_digital_modes(dbcon):
    d_modes = models.read_digital_modes(dbcon)
    bottle.response.content_type = 'application/json'
    return json.dumps([x.asdict() for x in d_modes])


@app.put('/api/digital_modes')
def add_digital_mode(dbcon):
    data = bottle.request.json
    try:
        d_mode = models.DigitalMode(**data)
        models.add_digital_mode(dbcon, d_mode)
        bottle.response.status = 201
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


@app.post('/api/digital_modes/<mode_id:int>')
def update_digital_mode(mode_id, dbcon):
    data = bottle.request.json
    try:
        d_mode = models.DigitalMode(id=mode_id, **data)
        models.update_digital_mode(dbcon, d_mode)
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


@app.delete('/api/digital_modes/<mode_id:int>')
def delete_digital_mode(mode_id, dbcon):
    try:
        models.delete_digital_mode(dbcon, mode_id)
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}

# DX cluster routes
#
# Same file and format as the radio GUI (src/cluster/client.c): "host=..."
# and "port=..." lines are the server in use, "server=name|host|port" lines
# the saved ones. The GUI reloads the server when the file's mtime changes.

CLUSTER_DEFAULT_HOST = "dxc.ve7cc.net"
# Used while cluster.conf has no server= lines (keep in sync with src/cluster/client.c)
CLUSTER_DEFAULT_SERVERS = [
    ("VE7CC", "dxc.ve7cc.net", 23),
    ("W3LPL", "dxc.w3lpl.net", 7373),
    ("K1TTT", "k1ttt.net", 7373),
    ("NC7J", "dxc.nc7j.com", 7373),
    ("W1NR", "dxc.w1nr.net", 23),
    ("WA9PIE", "dxc.wa9pie.net", 8000),
    ("K4ZR", "k4zr.no-ip.org", 7300),
    ("DB0SUE", "db0sue.de", 8000),
    ("GB7DJK", "gb7djk.dxcluster.net", 7300),
    ("AE5E", "dxspots.com", 23),
]
CLUSTER_DEFAULT_PORT = 23
CLUSTER_MAX_SERVERS = 16


def read_cluster_conf():
    """Returns (host, port, servers): the server in use and [(name, host, port)]."""
    host, port, servers = CLUSTER_DEFAULT_HOST, CLUSTER_DEFAULT_PORT, []
    try:
        with open(settings.CLUSTER_CONF, errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        lines = []
    for line in lines:
        if line.startswith("host=") and line[5:]:
            host = line[5:]
        elif line.startswith("port="):
            m = re.match(r"\s*[+-]?\d+", line[5:])  # atoi()
            if m and 0 < int(m.group()) < 65536:
                port = int(m.group())
        elif line.startswith("server="):
            parts = line[7:].split("|")
            if len(parts) == 3 and parts[0] and parts[1] and parts[2].isdigit() and 0 < int(parts[2]) < 65536:
                servers.append((parts[0], parts[1], int(parts[2])))
    if not servers:
        servers = list(CLUSTER_DEFAULT_SERVERS)
    if (host, port) not in [(h, p) for _, h, p in servers]:
        servers.append((host, host, port))
    return host, port, servers


def write_cluster_conf(host, port, servers):
    tmp = settings.CLUSTER_CONF + ".tmp"
    with open(tmp, "w") as f:
        f.write(f"host={host}\nport={port}\n")
        for name, h, p in servers:
            f.write(f"server={name}|{h}|{p}\n")
        f.flush()
        os.fsync(f.fileno())
    os.rename(tmp, settings.CLUSTER_CONF)


def cluster_state():
    host, port, servers = read_cluster_conf()
    return {"host": host, "port": port,
            "servers": [{"name": n, "host": h, "port": p} for n, h, p in servers]}


def cluster_error(status, msg):
    bottle.response.status = status
    return {"status": "error", "msg": msg}


def cluster_save(host, port, servers):
    try:
        write_cluster_conf(host, port, servers)
    except OSError as e:
        return cluster_error(500, f"Can't save: {e}")
    return cluster_state()


# wfview server routes
# Same file and format as the radio GUI (src/wfview_server.c): enabled=0|1, user=, pass=
WFVIEW_DEFAULTS = {"enabled": "0", "user": "x6100", "pass": "x6100"}


def read_wfview_conf():
    conf = dict(WFVIEW_DEFAULTS)
    try:
        with open(settings.WFVIEW_CONF, errors="replace") as f:
            for line in f:
                key, sep, val = line.rstrip("\r\n").partition("=")
                if sep and key in conf:
                    conf[key] = val
    except OSError:
        pass
    return {"enabled": conf["enabled"] == "1", "user": conf["user"], "pass": conf["pass"]}


@app.route('/wfview')
def wfview():
    return bottle.template('wfview')


@app.get('/api/wfview')
def get_wfview():
    return read_wfview_conf()


@app.post('/api/wfview')
def set_wfview():
    data = bottle.request.json or {}
    user = str(data.get("user", "")).strip()
    passwd = str(data.get("pass", ""))
    # The GUI keeps 15 characters; login fields are 16 bytes
    for val, what in ((user, "user"), (passwd, "password")):
        if not re.fullmatch(r"[ -~]{1,15}", val) or "=" in val:
            bottle.response.status = 400
            return {"status": "error", "msg": f"Invalid {what}: 1-15 printable characters, no '='"}
    try:
        tmp = settings.WFVIEW_CONF + ".tmp"
        with open(tmp, "w") as f:
            f.write(f"enabled={1 if data.get('enabled') else 0}\nuser={user}\npass={passwd}\n")
        os.rename(tmp, settings.WFVIEW_CONF)
    except OSError as e:
        bottle.response.status = 500
        return {"status": "error", "msg": f"Can't save: {e}"}
    return read_wfview_conf()


@app.route('/cluster')
def cluster():
    return bottle.template('cluster')


@app.get('/api/cluster')
def get_cluster():
    return cluster_state()


@app.post('/api/cluster/servers')
def add_cluster_server():
    data = bottle.request.json or {}
    name = str(data.get("name", "")).strip()
    host = str(data.get("host", "")).strip()
    if not name or len(name) > 31 or "|" in name or "\r" in name or "\n" in name:
        return cluster_error(400, "Invalid name: 1-31 characters, no '|'")
    # Hosts the GUI accepts (cluster_parse_server): letters, digits, '.', '-'
    if not re.fullmatch(r"[A-Za-z0-9.-]{1,127}", host):
        return cluster_error(400, "Invalid host: use letters, digits, '.' and '-'")
    try:
        port = int(str(data.get("port", "")).strip())
    except ValueError:
        port = 0
    if not 0 < port < 65536:
        return cluster_error(400, "Invalid port: 1-65535")
    cur_host, cur_port, servers = read_cluster_conf()
    if len(servers) >= CLUSTER_MAX_SERVERS:
        return cluster_error(400, f"At most {CLUSTER_MAX_SERVERS} servers")
    if any(h == host and p == port for _, h, p in servers):
        return cluster_error(400, "This server is already in the list")
    return cluster_save(cur_host, cur_port, servers + [(name, host, port)])


@app.post('/api/cluster/use/<index:int>')
def use_cluster_server(index):
    _, _, servers = read_cluster_conf()
    if not 0 <= index < len(servers):
        return cluster_error(404, "No such server")
    _, host, port = servers[index]
    return cluster_save(host, port, servers)


@app.delete('/api/cluster/servers/<index:int>')
def delete_cluster_server(index):
    host, port, servers = read_cluster_conf()
    if not 0 <= index < len(servers):
        return cluster_error(404, "No such server")
    if servers[index][1:] == (host, port):
        return cluster_error(400, "This server is in use: choose another one first")
    del servers[index]
    return cluster_save(host, port, servers)


# Main routes

@app.route('/static/<filepath:path>')
def server_static(filepath):
    # The radio's WiFi is slow: send static files gzipped and let the browser keep them
    response = bottle.static_file(filepath, root=STATIC_PATH)
    if isinstance(response, bottle.HTTPError) or response.status_code != 200:
        return response
    response.set_header('Cache-Control', 'max-age=86400')
    if 'gzip' in bottle.request.headers.get('Accept-Encoding', ''):
        if filepath not in _gzip_cache:
            with open(STATIC_PATH.joinpath(filepath), 'rb') as f:
                _gzip_cache[filepath] = gzip.compress(f.read())
        response.body.close()
        response.body = _gzip_cache[filepath]
        response.set_header('Content-Encoding', 'gzip')
        response.set_header('Content-Length', str(len(response.body)))
    return response


_gzip_cache = {}


@app.route('/')
def home():
    return bottle.template('index')


@app.route('/bands')
def bands():
    return bottle.template('bands')


@app.route('/digital_modes')
def digital_modes():
    return bottle.template('digital_modes')


@app.route('/files/')
@app.route('/files/<filepath:path>')
@app.route('/files/<filepath:path>/')
def files(filepath=""):
    path = pathlib.Path(settings.FILEBROWSER_PATH) / filepath
    if path.is_file():
        os.sync()
        response = bottle.static_file(str(path.relative_to(settings.FILEBROWSER_PATH)), root=settings.FILEBROWSER_PATH, download=True)
        response.set_header("Cache-Control", "private, no-cache, no-store")
        return response
    else:
        dirs = []
        files = []
        for item in sorted(path.iterdir()):
            if item.is_dir():
                dirs.append(item.relative_to(path))
            else:
                files.append(item.relative_to(path))
        return bottle.template('files', dirs=dirs, files=files)

# Timezone routes


@app.route('/time')
def time_editor():
    return bottle.template('time')


@app.get('/api/get_time')
def get_time():
    tz = timezone(timedelta())
    server_time = datetime.now(tz).isoformat()
    bottle.response.content_type = 'application/json'
    return {"server_time": server_time}


def update_time_by_ntp(server_address):
    ntp_args = ["ntpdate", "-u", server_address]
    p = subprocess.Popen(
        ntp_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        _, errs = p.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        p.kill()
        _, errs = p.communicate()
        bottle.response.status = 500
        return {"status": "error", "msg": "NTP update timeout"}

    if p.returncode != 0:
        bottle.response.status = 500
        return {"status": "error", "msg": f"NTP update failed: {errs.decode()}"}

    return {"status": "success", "msg": "NTP update successful"}


@app.post('/api/update_time')
def update_time():
    data = bottle.request.json

    update_mode = data.get("update_mode")
    if not update_mode:
        bottle.response.status = 400
        return {"status": "error", "msg": "update_mode is required"}

    if update_mode == "ntp":
        server_address = data.get("server_address")
        return update_time_by_ntp(server_address)

    elif update_mode == "manual":
        manual_time = data.get("manual_time")
        if not manual_time:
            bottle.response.status = 400
            return {"status": "error", "msg": "manual_time is required"}

        try:
            # Update system time manually
            manual_time = datetime.strptime(manual_time, "%Y-%m-%d %H:%M:%S")
            subprocess.run(
                ["date", "-s", manual_time.strftime("%Y-%m-%d %H:%M:%S")], check=True)
            return {"status": "success", "msg": "Server time updated manually"}
        except Exception as e:
            bottle.response.status = 500
            return {"status": "error", "msg": f"Failed to set manual time: {str(e)}"}

    else:
        bottle.response.status = 400
        return {"status": "error", "msg": f"unknown update_mode: {update_mode}"}


@app.get('/api/get_timezone')
def get_timezone():
    """Get the current server timezone."""
    try:
        p = subprocess.run(["realpath", "/etc/localtime"],
                           stdout=subprocess.PIPE, check=True)
        timezone_path = p.stdout.decode().strip()
        tz_list = timezone_path.split("/posix/")
        if len(tz_list) < 2:
            tz_list = timezone_path.split("/zoneinfo/")
        tz = tz_list[-1]
        return {"timezone": tz}
    except Exception as e:
        bottle.response.status = 500
        return {"status": "error", "msg": f"Failed to fetch timezone: {str(e)}"}


@app.post('/api/set_timezone')
def set_timezone():
    """Set the server timezone."""
    data = bottle.request.json
    timezone = data.get("timezone")
    if not timezone:
        bottle.response.status = 400
        return {"status": "error", "msg": "Timezone is required"}

    target_tz = f"/usr/share/zoneinfo/{timezone}"
    if not os.path.exists(target_tz):
        bottle.response.status = 400
        return {"status": "error", "msg": f"Invalid timezone: {timezone}"}

    try:
        subprocess.run(["ln", "-sf", target_tz, "/etc/localtime"], check=True)
        return {"status": "success", "msg": "Timezone updated successfully"}
    except subprocess.CalledProcessError as e:
        bottle.response.status = 500
        return {"status": "error", "msg": f"Failed to set timezone: {str(e)}"}
