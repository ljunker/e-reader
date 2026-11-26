#include "WifiShare.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "Storage.h"
#include "Transfer.h"
#include "config.h"

namespace wifishare {

static const char kPage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>E-Reader</title>
<style>
:root{color-scheme:light dark;--fg:#1a1a1a;--bg:#fafaf7;--mute:#777;--line:#ddd;--acc:#2b6cb0}
@media(prefers-color-scheme:dark){:root{--fg:#eee;--bg:#1b1b1b;--mute:#999;--line:#333;--acc:#63a4ff}}
body{font:16px/1.4 system-ui,sans-serif;color:var(--fg);background:var(--bg);margin:0 auto;max-width:640px;padding:16px}
h1{font-size:22px;margin:0 0 12px}
#drop{border:2px dashed var(--line);border-radius:10px;padding:24px;text-align:center;cursor:pointer}
#drop.over{border-color:var(--acc)}
progress{width:100%;height:10px;margin-top:10px}
#msg{color:var(--mute);min-height:1.4em;margin:8px 0}
ul{list-style:none;padding:0;margin:16px 0}
li{display:flex;gap:8px;align-items:center;padding:8px 0;border-bottom:1px solid var(--line)}
li span{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
small{color:var(--mute)}
button{font:inherit;border:1px solid var(--line);background:none;color:var(--fg);border-radius:6px;padding:4px 10px;cursor:pointer}
</style></head><body>
<h1>E-Reader</h1>
<div id="drop">Drop .epub, .txt or .xtp files here<br><small>or tap to choose</small>
<input id="pick" type="file" multiple accept=".epub,.txt,.xtp" hidden></div>
<progress id="bar" max="1" value="0" hidden></progress>
<div id="msg"></div>
<ul id="list"></ul>
<script>
const $=s=>document.querySelector(s),msg=t=>$('#msg').textContent=t;
const size=b=>b>1048576?(b/1048576).toFixed(1)+' MB':Math.ceil(b/1024)+' KB';
async function load(){
  const books=await (await fetch('/books')).json();
  $('#list').innerHTML='';
  if(!books.length)$('#list').innerHTML='<li><small>No books yet.</small></li>';
  for(const b of books){
    const li=document.createElement('li'),s=document.createElement('span'),k=document.createElement('small'),d=document.createElement('button');
    s.textContent=b.name;k.textContent=b.kind+' · '+size(b.size);d.textContent='Delete';
    d.onclick=async()=>{if(!confirm('Delete '+b.name+'?'))return;
      const r=await fetch('/delete?path='+encodeURIComponent(b.path),{method:'POST'});msg(await r.text());load();};
    li.append(s,k,d);$('#list').append(li);
  }
}
function send(f){return new Promise(ok=>{
  const x=new XMLHttpRequest(),fd=new FormData();fd.append('file',f,f.name);
  x.upload.onprogress=e=>{$('#bar').value=e.loaded/e.total};
  x.onload=()=>ok(x.status==200?'':f.name+': '+x.responseText);
  x.onerror=()=>ok(f.name+': connection lost');
  x.open('POST','/upload');x.send(fd);})}
async function upload(files){
  const errs=[];$('#bar').hidden=false;
  for(let i=0;i<files.length;i++){msg('Sending '+(i+1)+' / '+files.length+': '+files[i].name);
    $('#bar').value=0;const e=await send(files[i]);if(e)errs.push(e);}
  $('#bar').hidden=true;msg(errs.length?errs.join(' · '):files.length+' sent.');load();
}
$('#drop').onclick=()=>$('#pick').click();
$('#pick').onchange=e=>upload([...e.target.files]);
$('#drop').ondragover=e=>{e.preventDefault();$('#drop').classList.add('over')};
$('#drop').ondragleave=()=>$('#drop').classList.remove('over');
$('#drop').ondrop=e=>{e.preventDefault();$('#drop').classList.remove('over');upload([...e.dataTransfer.files])};
load();
</script></body></html>)HTML";

static WebServer* server = nullptr;
static WiFiMulti* multi = nullptr;
static bool active_ = false;
static uint32_t received = 0;
static String last;
static uint32_t lastRequest = 0;
static bool changed = false;

// Upload in progress: written through a PSRAM buffer, since the web server
// hands over ~1.4 KB at a time and small SD writes are slow.
static constexpr size_t kBufSize = 32 * 1024;
static uint8_t* buf = nullptr;
static size_t bufUsed = 0;
static File upFile;
static String upPath, upError;

static String jsonEscape(const String& s) {
    String o;
    o.reserve(s.length() + 2);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (uint8_t(c) < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); o += e; }
        else o += c;
    }
    return o;
}

static void handleList() {
    lastRequest = millis();
    String out = "[";
    for (const auto& b : storage::listBooks()) {
        if (out.length() > 1) out += ',';
        const char* kind = b.kind == BookKind::Xtp ? "PDF" : b.kind == BookKind::Epub ? "EPUB" : "TXT";
        out += "{\"path\":\"" + jsonEscape(b.path) + "\",\"name\":\"" + jsonEscape(b.name) +
               "\",\"kind\":\"" + kind + "\",\"size\":" + String(b.size) + "}";
    }
    out += ']';
    server->send(200, "application/json", out);
}

static void handleDelete() {
    lastRequest = millis();
    String path = server->arg("path");
    // only files the library lists, so nothing outside the books can go
    for (const auto& b : storage::listBooks()) {
        if (b.path != path) continue;
        if (!storage::fs().remove(path)) {
            server->send(500, "text/plain", "Could not delete " + b.name);
            return;
        }
        storage::forgetBook(path);
        changed = true;
        server->send(200, "text/plain", "Deleted " + b.name + ".");
        return;
    }
    server->send(404, "text/plain", "No such book.");
}

static bool flush() {
    if (!bufUsed) return true;
    bool ok = upFile.write(buf, bufUsed) == bufUsed;
    bufUsed = 0;
    return ok;
}

static void abortUpload(const String& why) {
    if (upFile) upFile.close();
    if (upPath.length()) storage::fs().remove(upPath + ".part");
    upError = why;
    upPath = "";
}

static void handleUploadData() {
    HTTPUpload& up = server->upload();
    lastRequest = millis();
    switch (up.status) {
        case UPLOAD_FILE_START: {
            upError = "";
            upPath = "";
            bufUsed = 0;
            std::string name = transfer::safeBookName(up.filename.c_str());
            if (name.empty()) { upError = "only .epub, .txt and .xtp files"; return; }
            if (!buf) buf = static_cast<uint8_t*>(ps_malloc(kBufSize));
            if (!buf) { upError = "out of memory"; return; }
            if (!storage::fs().exists(BOOKS_DIR)) storage::fs().mkdir(BOOKS_DIR);
            upPath = String(BOOKS_DIR) + "/" + name.c_str();
            upFile = storage::fs().open(upPath + ".part", FILE_WRITE);
            if (!upFile) abortUpload("cannot write to the card");
            break;
        }
        case UPLOAD_FILE_WRITE: {
            if (!upFile) return;
            size_t n = up.currentSize;
            const uint8_t* p = up.buf;
            while (n) {
                size_t k = std::min(n, kBufSize - bufUsed);
                memcpy(buf + bufUsed, p, k);
                bufUsed += k;
                p += k;
                n -= k;
                if (bufUsed == kBufSize && !flush()) { abortUpload("card full?"); return; }
            }
            break;
        }
        case UPLOAD_FILE_END: {
            if (!upFile) return;
            if (!flush()) { abortUpload("card full?"); return; }
            upFile.close();
            auto& fs = storage::fs();
            if (fs.exists(upPath)) fs.remove(upPath);
            if (!fs.rename(upPath + ".part", upPath)) { abortUpload("cannot rename on the card"); return; }
            storage::forgetBook(upPath);   // a replaced book must lay out again
            received++;
            last = upPath.substring(upPath.lastIndexOf('/') + 1);
            changed = true;
            upPath = "";
            break;
        }
        case UPLOAD_FILE_ABORTED:
            abortUpload("upload aborted");
            break;
    }
}

static void handleUploadDone() {
    if (upError.length()) server->send(400, "text/plain", upError);
    else server->send(200, "text/plain", "ok");
}

bool start(String& error) {
    if (active_) return true;
    if (!storage::mounted()) { error = "The microSD card is not ready."; return false; }
    File f = storage::fs().open(WIFI_FILE, FILE_READ);
    if (!f) {
        error = "No " WIFI_FILE " on the card.\n\nAdd one line per network:\nSSID = password";
        return false;
    }
    String text = f.readString();
    f.close();
    auto nets = transfer::parseWifiList(text.c_str());
    if (nets.empty()) {
        error = WIFI_FILE " lists no network.\n\nOne line per network:\nSSID = password";
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("ereader");
    multi = new WiFiMulti();
    for (const auto& n : nets) multi->addAP(n.ssid.c_str(), n.password.empty() ? nullptr : n.password.c_str());
    uint32_t t0 = millis();
    while (multi->run(8000) != WL_CONNECTED) {
        if (millis() - t0 > WIFI_CONNECT_MS) {
            stop();
            error = "None of the " + String(nets.size()) + " networks in " WIFI_FILE " answered.";
            return false;
        }
        delay(200);
    }

    MDNS.begin("ereader");
    MDNS.addService("http", "tcp", 80);
    server = new WebServer(80);
    server->on("/", HTTP_GET, [] { lastRequest = millis(); server->send(200, "text/html", kPage); });
    server->on("/books", HTTP_GET, handleList);
    server->on("/delete", HTTP_POST, handleDelete);
    server->on("/upload", HTTP_POST, handleUploadDone, handleUploadData);
    server->onNotFound([] { server->send(404, "text/plain", "Not found"); });
    server->begin();

    active_ = true;
    received = 0;
    last = "";
    changed = false;
    lastRequest = millis();
    return true;
}

void stop() {
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
    if (upFile) abortUpload("stopped");
    if (active_) MDNS.end();
    delete multi;
    multi = nullptr;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    if (buf) {
        free(buf);
        buf = nullptr;
    }
    active_ = false;
}

void loop() {
    if (server) server->handleClient();
}

bool active() { return active_; }
String network() { return WiFi.SSID(); }
String address() { return "http://" + WiFi.localIP().toString() + "/"; }
uint32_t filesReceived() { return received; }
const String& lastFile() { return last; }
uint32_t lastRequestMs() { return lastRequest; }

bool takeChanged() {
    bool c = changed;
    changed = false;
    return c;
}

}  // namespace wifishare
