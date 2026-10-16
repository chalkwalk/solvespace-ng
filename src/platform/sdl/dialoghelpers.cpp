//-----------------------------------------------------------------------------
// Pure helpers for the SDL backend's file dialogs.
//-----------------------------------------------------------------------------
#include "platform/sdl/dialoghelpers.h"

namespace SolveSpace {
namespace Platform {

std::string EnsureExtension(const std::string &path, const std::vector<FileFilter> &filters,
                            int filterIndex) {
    if(filters.empty()) return path;
    size_t slash = path.find_last_of("/\\");
    size_t dot   = path.find_last_of('.');
    if(dot != std::string::npos && (slash == std::string::npos || dot > slash)) return path;
    const FileFilter &f =
        filters[(filterIndex >= 0 && filterIndex < (int)filters.size()) ? filterIndex : 0];
    if(f.extensions.empty()) return path;
    return path + "." + f.extensions[0];
}

std::string SdlFilterPattern(const FileFilter &filter) {
    std::string out;
    for(size_t i = 0; i < filter.extensions.size(); i++) {
        if(i) out += ";";
        out += filter.extensions[i];
    }
    return out;
}

}
}
