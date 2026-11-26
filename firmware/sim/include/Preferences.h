#pragma once
// NVS of the simulator: one small text file per namespace in /nvs (IndexedDB).
#include <Arduino.h>

#include <map>
#include <string>

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false, const char* partition_label = nullptr);
    void end();
    bool clear();
    bool remove(const char* key);
    bool isKey(const char* key);

    size_t putUChar(const char* key, uint8_t value) { return put(key, std::to_string(value)) ? 1 : 0; }
    size_t putUInt(const char* key, uint32_t value) { return put(key, std::to_string(value)) ? 4 : 0; }
    size_t putInt(const char* key, int32_t value) { return put(key, std::to_string(value)) ? 4 : 0; }
    size_t putBool(const char* key, bool value) { return put(key, value ? "1" : "0") ? 1 : 0; }
    size_t putString(const char* key, const String& value) { return put(key, value.c_str()) ? value.length() : 0; }

    uint8_t getUChar(const char* key, uint8_t defaultValue = 0) { return uint8_t(getNum(key, defaultValue)); }
    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) { return uint32_t(getNum(key, defaultValue)); }
    int32_t getInt(const char* key, int32_t defaultValue = 0) { return int32_t(getNum(key, defaultValue)); }
    bool getBool(const char* key, bool defaultValue = false) { return getNum(key, defaultValue) != 0; }
    String getString(const char* key, const String& defaultValue = String());

private:
    bool put(const char* key, const std::string& value);
    long long getNum(const char* key, long long defaultValue);
    void load();
    void save();

    std::string file_;
    bool open_ = false;
    bool readOnly_ = false;
    std::map<std::string, std::string> kv_;
};
