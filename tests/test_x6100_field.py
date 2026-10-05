#!/usr/bin/env python3
# Self-check of rootfs/usr/sbin/x6100-field: python3 tests/test_x6100_field.py
# No network: downloads are patched, POSTs go to a local fake server.

import contextlib
import importlib.machinery
import importlib.util
import io
import json
import os
import sys
import tempfile
import threading
import urllib.parse
from datetime import date
from http.server import BaseHTTPRequestHandler, HTTPServer

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "rootfs", "usr", "sbin", "x6100-field")

POTA = b'''"reference","name","active","entityId","locationDesc","latitude","longitude","grid"
"IT-0123","Parco\tdel Monte","1","248","IT-PI","45.1234567","7.5","JN35"
"IT-0124","Old park","0","248","IT-PI","45.0","7.0","JN35"
"IT-0125","No coords","1","248","IT-PI","","","JN35"
'''

SOTA = b'''SOTA Summits List (Date=01/10/2026)
SummitCode,AssociationName,RegionName,SummitName,AltM,AltFt,GridRef1,GridRef2,Longitude,Latitude,Points,BonusPoints,ValidFrom,ValidTo,ActivationCount,ActivationDate,ActivationCall
I/PM-0001,Italy,Piemonte,Monte Uno,3841,12602,7.1,45.2,7.1000001,45.2000001,10,3,01/07/2010,31/12/2099,5,01/01/2020,IU1WXY
I/PM-0002,Italy,Piemonte,Retired,1000,3281,7.2,45.3,7.2,45.3,4,0,01/07/2010,31/12/2019,0,,
I/PM-0003,Italy,Piemonte,Future,1000,3281,7.3,45.4,7.3,45.4,4,0,01/01/2099,31/12/2099,0,,
'''


def load():
    sys.dont_write_bytecode = True   # no __pycache__ in rootfs/
    loader = importlib.machinery.SourceFileLoader("x6100_field", SCRIPT)
    mod = importlib.util.module_from_spec(importlib.util.spec_from_loader(loader.name, loader))
    loader.exec_module(mod)
    return mod


def run(field, argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = field.main(argv)
    line = out.getvalue()
    assert line.count("\n") == 1, line
    return code, line.strip()


def test_dates(field):
    today = date(2026, 10, 3)
    assert field.sota_valid("01/07/2010", "31/12/2099", today)
    assert field.sota_valid("03/10/2026", "03/10/2026", today)
    assert not field.sota_valid("01/07/2010", "02/10/2026", today)
    assert not field.sota_valid("04/10/2026", "31/12/2099", today)
    assert not field.sota_valid("", "31/12/2099", today)


def test_update(field, d):
    data = {field.POTA_CSV: POTA, field.SOTA_CSV: SOTA}

    def fake(url, *a, **k):
        if data[url] is None:
            raise OSError("down")
        return io.BytesIO(data[url])

    real, field.open_url = field.open_url, fake
    path = os.path.join(d, "refs.tsv")
    assert run(field, ["update"]) == (0, "ok 1 1")
    with open(path) as f:
        lines = f.read().splitlines()
    assert lines == ["P\tIT-0123\tParco del Monte\t45.12346\t7.50000\t\t",
                     "S\tI/PM-0001\tMonte Uno\t45.20000\t7.10000\t3841\t10"], lines

    # one list down: keep its previous lines
    data[field.SOTA_CSV] = None
    assert run(field, ["update"]) == (0, "ok 1 1")
    with open(path) as f:
        assert f.read().splitlines() == lines
    data[field.SOTA_CSV] = b"title\nnot,a,summit,list\n"
    assert run(field, ["update"]) == (0, "ok 1 1")

    # both down: error, file untouched
    data[field.POTA_CSV] = None
    code, line = run(field, ["update"])
    assert code == 1 and line.startswith("error "), line
    with open(path) as f:
        assert f.read().splitlines() == lines
    assert not os.path.exists(path + ".tmp")
    field.open_url = real


class Fake(BaseHTTPRequestHandler):
    requests = []
    replies = []

    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"]))
        Fake.requests.append((self.path, dict(self.headers), body))
        code, text = Fake.replies.pop(0)
        self.send_response(code)
        self.end_headers()
        self.wfile.write(text.encode())

    def log_message(self, *args):
        pass


def test_pota_spot(field, base):
    field.POTA_API = base
    Fake.requests.clear()
    Fake.replies[:] = [(200, "[]")]
    argv = ["pota-spot", "--call", "iu1wxy/p", "--freq-khz", "14062.0", "--ref", "it-0123", "--mode", "cw", "--comments", "QRP"]
    assert run(field, argv) == (0, "ok")
    path, headers, body = Fake.requests[0]
    assert path == "/spot"
    assert headers["Content-Type"] == "application/json"
    assert headers["User-Agent"] == "X6100-IU1WXY/IU1WXY/P"
    assert json.loads(body) == {"activator": "IU1WXY/P", "spotter": "IU1WXY/P", "frequency": "14062.0",
                                "reference": "IT-0123", "mode": "CW", "source": "X6100-IU1WXY", "comments": "QRP"}

    Fake.replies[:] = [(400, "Invalid reference " + "x" * 100)]
    code, line = run(field, argv)
    assert code == 1 and line == "error HTTP 400 Invalid reference " + "x" * 42, line

    n = len(Fake.requests)
    argv[argv.index("--ref") + 1] = "IT0123"
    assert run(field, argv) == (1, "error bad reference")
    assert len(Fake.requests) == n


def test_qrz_upload(field, base, d):
    field.QRZ_API = base + "/api"
    adi = os.path.join(d, "log.adi")
    with open(adi, "w") as f:
        f.write("Field log <ADIF_VER:5>3.1.4 <EOH>\n"
                "<CALL:5>DL1AB <QSO_DATE:8>20261003 <MODE:2>CW <EOR>\n"
                "<call:5>F4XYZ <qso_date:8>20261003 <mode:2>CW <eor>\n"
                "<CALL:5>G0AAA <MODE:3>SSB <eor>\n")

    assert run(field, ["qrz-upload", adi]) == (1, "error no QRZ key")

    with open(os.path.join(d, "qrz.conf"), "w") as f:
        f.write("key=ABCD-1234\n")
    Fake.requests.clear()
    Fake.replies[:] = [(200, "RESULT=OK&LOGID=1&COUNT=1"),
                       (200, "RESULT=FAIL&REASON=Unable to add QSO to database: duplicate&COUNT=0"),
                       (200, "RESULT=OK&LOGID=3&COUNT=1")]
    assert run(field, ["qrz-upload", "--call", "IU1WXY", adi]) == (0, "ok 2 1")
    assert len(Fake.requests) == 3
    path, headers, body = Fake.requests[1]
    assert path == "/api"
    assert headers["Content-Type"] == "application/x-www-form-urlencoded"
    assert headers["User-Agent"] == "X6100-IU1WXY/IU1WXY"
    form = urllib.parse.parse_qs(body.decode())
    assert form == {"KEY": ["ABCD-1234"], "ACTION": ["INSERT"],
                    "ADIF": ["<call:5>F4XYZ <qso_date:8>20261003 <mode:2>CW<eor>"]}, form

    Fake.replies[:] = [(200, "RESULT=AUTH&REASON=invalid api key")]
    code, line = run(field, ["qrz-upload", adi])
    assert code == 1 and line == "error QRZ invalid api key after 0 sent", line


def main():
    # The GUI and the web server run it directly: without the exec bit every job is "error no answer"
    assert os.access(SCRIPT, os.X_OK), "x6100-field is not executable (git update-index --chmod=+x)"
    with tempfile.TemporaryDirectory() as d:
        os.environ["X6100_FIELD_DIR"] = os.path.join(d, "field")
        os.environ.pop("X6100_CALL", None)
        field = load()
        test_dates(field)
        test_update(field, field.FIELD_DIR)
        server = HTTPServer(("127.0.0.1", 0), Fake)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        base = f"http://127.0.0.1:{server.server_port}"
        test_pota_spot(field, base)
        test_qrz_upload(field, base, field.FIELD_DIR)
        server.shutdown()
    print("x6100-field: all tests passed")


if __name__ == "__main__":
    main()
