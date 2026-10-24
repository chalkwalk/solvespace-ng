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

std::string SuggestedSavePath(const std::string &suggested, const std::string &folder,
                              const std::vector<FileFilter> &filters) {
    size_t slash = suggested.find_last_of("/\\");
    std::string dir  = slash == std::string::npos ? "" : suggested.substr(0, slash);
    std::string name = slash == std::string::npos ? suggested : suggested.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    std::string stem = (dot == std::string::npos || dot == 0) ? name : name.substr(0, dot);
    if(stem.empty()) stem = "untitled";
    if(!folder.empty()) dir = folder;
    std::string path = dir.empty() ? stem : dir + "/" + stem;
    if(!filters.empty() && !filters[0].extensions.empty()) path += "." + filters[0].extensions[0];
    return path;
}

std::string SdlFilterPattern(const FileFilter &filter) {
    std::string out;
    for(size_t i = 0; i < filter.extensions.size(); i++) {
        if(i) out += ";";
        out += filter.extensions[i];
    }
    return out;
}

std::string WebFileName(const std::string &name) {
    size_t      slash = name.find_last_of("/\\");
    std::string file  = slash == std::string::npos ? name : name.substr(slash + 1);
    size_t      first = file.find_first_not_of(" \t");
    if(first == std::string::npos) return "";
    file = file.substr(first, file.find_last_not_of(" \t") - first + 1);
    if(file == "." || file == "..") return "";
    return file;
}

}
}
