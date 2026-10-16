//-----------------------------------------------------------------------------
// Pure helpers for the SDL backend's file dialogs.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_DIALOGHELPERS_H
#define SOLVESPACE_SDL_DIALOGHELPERS_H

#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

// Appends "." + the first extension of filters[filterIndex] (or filters[0] if the index is
// out of range) when `path` has no extension. Returns `path` unchanged otherwise.
std::string EnsureExtension(const std::string &path, const std::vector<FileFilter> &filters,
                            int filterIndex);
// The default path for a save dialog: the remembered folder (or the suggestion's own folder),
// the suggestion's stem ("untitled" if none) and the first filter's extension.
std::string SuggestedSavePath(const std::string &suggested, const std::string &folder,
                              const std::vector<FileFilter> &filters);
// "slvs;stl" style pattern for SDL_DialogFileFilter.
std::string SdlFilterPattern(const FileFilter &filter);

}
}

#endif
