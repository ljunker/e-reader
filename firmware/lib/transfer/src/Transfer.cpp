#include "Transfer.h"

#include <cctype>

namespace transfer {

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}

std::vector<WifiNetwork> parseWifiList(const std::string& text) {
    std::vector<WifiNetwork> out;
    size_t start = 0;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) start = 3;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        std::string line = trim(text.substr(start, nl - start));
        start = nl + 1;
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        WifiNetwork n{trim(line.substr(0, eq)), trim(line.substr(eq + 1))};
        if (n.ssid.empty() || n.ssid.size() > 32 || n.password.size() > 63) continue;
        out.push_back(n);
    }
    return out;
}

static bool endsWithNoCase(const std::string& s, const char* suffix) {
    size_t n = 0;
    while (suffix[n]) n++;
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++)
        if (std::tolower(uint8_t(s[s.size() - n + i])) != suffix[i]) return false;
    return true;
}

std::string safeBookName(const std::string& uploadName) {
    size_t slash = uploadName.find_last_of("/\\");
    std::string name = trim(slash == std::string::npos ? uploadName : uploadName.substr(slash + 1));
    if (name.empty() || name[0] == '.' || name.size() > 120) return "";
    if (name.find("..") != std::string::npos) return "";
    for (char c : name) {
        uint8_t u = uint8_t(c);
        if (u < 0x20 || u == 0x7F || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            return "";
    }
    if (!endsWithNoCase(name, ".txt") && !endsWithNoCase(name, ".epub") && !endsWithNoCase(name, ".xtp")) return "";
    return name;
}

}  // namespace transfer
