"""Serves the simulator page. Like `python -m http.server`, but every response
says Cache-Control: no-cache, so a rebuilt firmware.wasm is never taken from
the browser cache.  Usage: serve.py <port> <directory>

Two more things for the online library (MENU -> Online library):
  /proxy?url=...   fetches any http(s) URL for the firmware's HTTP client, so the
                   browser's CORS rules do not apply. Request headers come in as
                   X-Fwd-<name>; the answer is always 200 with the real status in
                   X-Status and the real headers as X-Fwd-<name>, so the browser
                   neither follows redirects nor asks for a password itself.
  /opds-demo/      a small OPDS catalog of the sample books, behind HTTP Digest
                   authentication (user demo, password demo) like a Calibre
                   content server.
"""
import functools
import hashlib
import html
import http.server
import os
import secrets
import sys
import urllib.error
import urllib.parse
import urllib.request

DEMO_USER, DEMO_PASS, DEMO_REALM = "demo", "demo", "calibre"
DEMO_NONCE = secrets.token_hex(16)
PER_PAGE = 2


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


opener = urllib.request.build_opener(NoRedirect)


def md5(s):
    return hashlib.md5(s.encode()).hexdigest()


def parse_auth(value):
    """Digest parameters of an Authorization header, or None."""
    if not value or not value.lower().startswith("digest "):
        return None
    out = {}
    for part in urllib.request.parse_http_list(value[7:]):
        k, _, v = part.partition("=")
        out[k.strip().lower()] = v.strip().strip('"')
    return out


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def do_GET(self):
        path = urllib.parse.urlsplit(self.path).path
        if path == "/proxy":
            return self.proxy()
        if path.startswith("/opds-demo/"):
            return self.demo(path[len("/opds-demo/"):])
        return super().do_GET()

    # ------------------------------------------------------------ proxy

    def proxy(self):
        query = urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)
        url = (query.get("url") or [""])[0]
        if urllib.parse.urlsplit(url).scheme not in ("http", "https"):
            return self.answer(502, "text/plain", b"only http and https")
        headers = {k[6:]: v for k, v in self.headers.items() if k.lower().startswith("x-fwd-")}
        try:
            with opener.open(urllib.request.Request(url, headers=headers), timeout=20) as r:
                status, rh, body = r.status, r.headers, r.read()
        except urllib.error.HTTPError as e:
            status, rh, body = e.code, e.headers, e.read()
        except Exception as e:  # connection refused, DNS, TLS ...
            return self.answer(502, "text/plain", str(e).encode())
        self.send_response(200)
        self.send_header("X-Status", str(status))
        for k, v in rh.items():
            if k.lower() not in ("transfer-encoding", "connection", "content-length", "content-encoding"):
                self.send_header("X-Fwd-" + k, v)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def answer(self, code, ctype, body, extra=()):
        self.send_response(code)
        for k, v in extra:
            self.send_header(k, v)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    # ------------------------------------------------------------ demo catalog

    def authorized(self):
        a = parse_auth(self.headers.get("Authorization"))
        if not a or a.get("username") != DEMO_USER or a.get("uri") != self.path:
            return False
        ha1 = md5(f"{DEMO_USER}:{DEMO_REALM}:{DEMO_PASS}")
        ha2 = md5(f"GET:{a.get('uri')}")
        if a.get("qop") == "auth":
            want = md5(f"{ha1}:{a.get('nonce')}:{a.get('nc')}:{a.get('cnonce')}:auth:{ha2}")
        else:
            want = md5(f"{ha1}:{a.get('nonce')}:{ha2}")
        return a.get("nonce") == DEMO_NONCE and a.get("response") == want

    def demo(self, rest):
        if not self.authorized():
            challenge = f'Digest realm="{DEMO_REALM}", qop="auth", algorithm="MD5", nonce="{DEMO_NONCE}"'
            return self.answer(401, "text/plain", b"login required", [("WWW-Authenticate", challenge)])
        books = sorted(n for n in os.listdir(os.path.join(self.directory, "samples")) if n.endswith(".epub"))
        if rest == "":
            return self.feed("Demo library", [
                self.entry("All books", "Every sample book, two per page",
                           '<link rel="subsection" href="books" type="application/atom+xml;profile=opds-catalog;kind=acquisition"/>'),
                self.entry("Kindle only", "A book the reader cannot show",
                           '<link href="kindle" type="application/atom+xml;profile=opds-catalog;kind=acquisition"/>'),
            ])
        if rest == "books":
            page = int((urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query).get("page") or ["0"])[0])
            items = books[page * PER_PAGE:(page + 1) * PER_PAGE]
            entries = []
            for n in items:
                size = os.path.getsize(os.path.join(self.directory, "samples", n))
                title = os.path.splitext(n)[0].replace("_", " ").title()
                entries.append(self.entry(title, "A sample book of the simulator.",
                                          f'<link rel="http://opds-spec.org/acquisition" type="application/epub+zip" '
                                          f'href="/opds-demo/get/{urllib.parse.quote(n)}" length="{size}"/>',
                                          author="E-Reader Project"))
            nxt = f'<link rel="next" href="books?page={page + 1}" type="application/atom+xml"/>' \
                if (page + 1) * PER_PAGE < len(books) else ""
            return self.feed("All books", entries, nxt)
        if rest == "kindle":
            return self.feed("Kindle only", [self.entry(
                "Only as MOBI", "", '<link rel="http://opds-spec.org/acquisition" type="application/x-mobipocket-ebook" href="/x.mobi"/>')])
        if rest.startswith("get/"):
            name = os.path.basename(urllib.parse.unquote(rest[4:]))
            p = os.path.join(self.directory, "samples", name)
            if name in books and os.path.isfile(p):
                with open(p, "rb") as f:
                    return self.answer(200, "application/epub+zip", f.read())
        return self.answer(404, "text/plain", b"not found")

    @staticmethod
    def entry(title, summary, links, author=""):
        a = f"<author><name>{html.escape(author)}</name></author>" if author else ""
        return (f"<entry><title>{html.escape(title)}</title>{a}<id>{html.escape(title)}</id>"
                f'<content type="text">{html.escape(summary)}</content>{links}</entry>')

    def feed(self, title, entries, extra=""):
        xml = ('<?xml version="1.0" encoding="utf-8"?>\n<feed xmlns="http://www.w3.org/2005/Atom">'
               f"<title>{html.escape(title)}</title>{extra}{''.join(entries)}</feed>")
        return self.answer(200, "application/atom+xml;profile=opds-catalog", xml.encode())


if __name__ == "__main__":
    port, root = int(sys.argv[1]), sys.argv[2]
    handler = functools.partial(Handler, directory=root)
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as httpd:
        httpd.serve_forever()
