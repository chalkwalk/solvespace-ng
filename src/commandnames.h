//-----------------------------------------------------------------------------
// Stable, human-readable identifiers for menu commands, used by keymap files,
// the command palette and test scripts.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_COMMANDNAMES_H
#define SOLVESPACE_COMMANDNAMES_H

#include <cstdint>
#include <string>

namespace SolveSpace {

enum class Command : uint32_t;

// Returns the stable id for `cmd`, or nullptr for Command::NONE / unknown values.
const char *CommandName(Command cmd);
// Exact, case-sensitive lookup; returns Command::NONE for unknown names.
Command CommandFromName(const std::string &name);

}

#endif
