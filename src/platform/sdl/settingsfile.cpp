//-----------------------------------------------------------------------------
// Platform::Settings stored as a key=value text file, for the SDL backend.
//-----------------------------------------------------------------------------
#include "platform/sdl/settingsfile.h"

namespace SolveSpace {
namespace Platform {

static std::string Escape(const std::string &s) {
    std::string r;
    for(char c : s) {
        if(c == '\\') r += "\\\\";
        else if(c == '\n') r += "\\n";
        else r += c;
    }
    return r;
}

static std::string Unescape(const std::string &s) {
    std::string r;
    for(size_t i = 0; i < s.size(); i++) {
        if(s[i] == '\\' && i + 1 < s.size()) {
            r += (s[i + 1] == 'n') ? '\n' : s[i + 1];
            i++;
        } else {
            r += s[i];
        }
    }
    return r;
}

SettingsFile::SettingsFile(Path path) : path(path) {
    std::string data;
    if(!ReadFile(path, &data)) return;
    size_t pos = 0;
    while(pos < data.size()) {
        size_t nl = data.find('\n', pos);
        std::string line = data.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? data.size() : nl + 1;
        if(!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if(eq == std::string::npos || eq == 0) continue;
        values[line.substr(0, eq)] = Unescape(line.substr(eq + 1));
    }
}

SettingsFile::~SettingsFile() {
    Save();
}

bool SettingsFile::Save() {
    if(!dirty) return true;
    std::string data;
    for(const auto &kv : values) data += kv.first + "=" + Escape(kv.second) + "\n";
    Path tmp = Path::From(path.raw + ".tmp");
    if(!WriteFile(tmp, data)) return false;
    remove(path.raw.c_str()); // rename() can't overwrite on Windows
    if(rename(tmp.raw.c_str(), path.raw.c_str()) != 0) return false;
    dirty = false;
    return true;
}

void SettingsFile::FreezeInt(const std::string &key, uint32_t value) {
    FreezeString(key, std::to_string(value));
}

uint32_t SettingsFile::ThawInt(const std::string &key, uint32_t defaultValue) {
    auto it = values.find(key);
    if(it == values.end() || it->second.empty()) return defaultValue;
    char *end;
    unsigned long v = strtoul(it->second.c_str(), &end, 10);
    return (*end == '\0') ? (uint32_t)v : defaultValue;
}

void SettingsFile::FreezeFloat(const std::string &key, double value) {
    FreezeString(key, ssprintf("%.17g", value));
}

double SettingsFile::ThawFloat(const std::string &key, double defaultValue) {
    auto it = values.find(key);
    if(it == values.end() || it->second.empty()) return defaultValue;
    char *end;
    double v = strtod(it->second.c_str(), &end);
    return (*end == '\0') ? v : defaultValue;
}

void SettingsFile::FreezeString(const std::string &key, const std::string &value) {
    auto it = values.find(key);
    if(it != values.end() && it->second == value) return;
    values[key] = value;
    dirty = true;
}

std::string SettingsFile::ThawString(const std::string &key, const std::string &defaultValue) {
    auto it = values.find(key);
    return it == values.end() ? defaultValue : it->second;
}

}
}
