#pragma once
// Helpers for getting books onto the card over Wi-Fi; no Arduino code, so the
// host tests can check them.
#include <string>
#include <vector>

namespace transfer {

struct WifiNetwork {
    std::string ssid;
    std::string password;   // empty = open network
};

// Parses /wifi.txt: one network per line, "SSID = password".
// The line is split at the first '=' and both sides are trimmed, so the
// password may contain '=' (and '#'). Blank lines and lines starting with '#'
// are skipped, as are lines without '=', an empty SSID, an SSID over 32 bytes
// or a password over 63 bytes. CRLF and a UTF-8 BOM are fine.
std::vector<WifiNetwork> parseWifiList(const std::string& text);

// File name to store an uploaded book under, or "" to reject the upload.
// Drops any folder part ("a/b.epub", "C:\\x\\b.epub" -> "b.epub") and rejects
// hidden names, "..", control characters, names over 120 bytes and anything
// that is not .txt, .epub or .xtp (any case).
std::string safeBookName(const std::string& uploadName);

}  // namespace transfer
