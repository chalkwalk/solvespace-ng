//-----------------------------------------------------------------------------
// A tiny scripting language for driving solvespace-ng in development and tests.
//-----------------------------------------------------------------------------
#include <cctype>
#include "platform/sdl/debugscript.h"

namespace SolveSpace {
namespace Platform {

static std::string Trim(const std::string &s) {
    size_t b = 0, e = s.size();
    while(b < e && isspace((unsigned char)s[b])) b++;
    while(e > b && isspace((unsigned char)s[e - 1])) e--;
    return s.substr(b, e - b);
}

std::optional<std::vector<DebugStep>> ParseDebugScript(const std::string &text,
                                                       std::string *error) {
    std::vector<DebugStep> steps;
    size_t pos = 0;
    int    index = 0;
    while(pos <= text.size()) {
        size_t semi = text.find(';', pos);
        std::string item = Trim(text.substr(pos, semi == std::string::npos ? std::string::npos
                                                                          : semi - pos));
        pos = (semi == std::string::npos) ? text.size() + 1 : semi + 1;
        if(item.empty()) continue;
        index++;
        auto fail = [&](const std::string &why) -> std::optional<std::vector<DebugStep>> {
            if(error) *error = "step " + std::to_string(index) + ": " + why;
            return std::nullopt;
        };
        size_t space = item.find(' ');
        std::string verb = item.substr(0, space);
        std::string arg  = space == std::string::npos ? "" : Trim(item.substr(space + 1));
        DebugStep step;
        if(verb == "action")     step.kind = DebugStep::Kind::ACTION;
        else if(verb == "keys")  step.kind = DebugStep::Kind::KEYS;
        else if(verb == "key")   step.kind = DebugStep::Kind::KEY;
        else if(verb == "wait")  step.kind = DebugStep::Kind::WAIT;
        else if(verb == "shot")  step.kind = DebugStep::Kind::SHOT;
        else if(verb == "quit")  step.kind = DebugStep::Kind::QUIT;
        else return fail("unknown step '" + verb + "'");
        if(step.kind != DebugStep::Kind::QUIT && arg.empty()) return fail(verb + " needs an argument");
        if(step.kind == DebugStep::Kind::WAIT) {
            char *end;
            long n = strtol(arg.c_str(), &end, 10);
            if(*end != '\0' || n < 0) return fail("bad frame count '" + arg + "'");
            step.frames = (int)n;
        }
        step.arg = arg;
        steps.push_back(step);
    }
    return steps;
}

}
}
