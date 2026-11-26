"""Serves the simulator page. Like `python -m http.server`, but every response
says Cache-Control: no-cache, so a rebuilt firmware.wasm is never taken from
the browser cache.  Usage: serve.py <port> <directory>"""
import functools
import http.server
import sys


class NoCache(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


if __name__ == "__main__":
    port, root = int(sys.argv[1]), sys.argv[2]
    handler = functools.partial(NoCache, directory=root)
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as httpd:
        httpd.serve_forever()
