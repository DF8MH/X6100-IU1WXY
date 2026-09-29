import argparse
from socketserver import ThreadingMixIn
from wsgiref.simple_server import WSGIRequestHandler, WSGIServer, make_server

import bottle
import bottle.ext.sqlite

from . import apps
from . import settings


class ThreadingWSGIServer(ThreadingMixIn, WSGIServer):
    daemon_threads = True


class RequestHandler(WSGIRequestHandler):
    # Browsers open connections in advance and may never use them: don't
    # let one of them hold a worker for long.
    timeout = 10


class ThreadedServer(bottle.ServerAdapter):
    """One thread per connection: bottle's default wsgiref server serves
    one connection at a time, so an idle browser connection blocked every
    page for up to a minute."""

    def run(self, handler):
        make_server(self.host, self.port, handler, ThreadingWSGIServer, RequestHandler).serve_forever()


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", help="path to SQLite3 database", required=True)
    parser.add_argument("--host", help="ip address to listen", default="localhost")
    parser.add_argument("--port", type=int, help="port to listen", default=8080)
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--filebrowser-path", help="path file browser root", default="/mnt")
    parser.add_argument("--cluster-conf", help="path to DX cluster config", default=settings.CLUSTER_CONF)
    args = parser.parse_args()
    plugin = bottle.ext.sqlite.Plugin(dbfile=args.db, keyword="dbcon")
    apps.app.install(plugin)
    settings.FILEBROWSER_PATH = args.filebrowser_path
    settings.DB_PATH = args.db
    settings.CLUSTER_CONF = args.cluster_conf
    apps.app.run(server=ThreadedServer, host=args.host, port=args.port, debug=args.debug, reloader=args.debug)

run()
