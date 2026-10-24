//-----------------------------------------------------------------------------
// Platform::Settings stored as a key=value text file, for the SDL backend.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_SETTINGSFILE_H
#define SOLVESPACE_SDL_SETTINGSFILE_H

#include <map>
#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

class SettingsFile final : public Settings {
public:
    explicit SettingsFile(Path path);
    ~SettingsFile() override;
    bool Save();
    // Changed since it was read or last saved.
    bool Dirty() const { return dirty; }

    void FreezeInt(const std::string &key, uint32_t value) override;
    uint32_t ThawInt(const std::string &key, uint32_t defaultValue = 0) override;
    void FreezeFloat(const std::string &key, double value) override;
    double ThawFloat(const std::string &key, double defaultValue = 0.0) override;
    void FreezeString(const std::string &key, const std::string &value) override;
    std::string ThawString(const std::string &key,
                           const std::string &defaultValue = "") override;

private:
    Path                               path;
    std::map<std::string, std::string> values;
    bool                               dirty = false;
};

}
}

#endif
