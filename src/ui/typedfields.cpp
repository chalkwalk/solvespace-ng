//-----------------------------------------------------------------------------
// Number fields typed into the HUD.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "ui/typedfields.h"

namespace SolveSpace {
namespace Ui {

TypedFields::TypedFields(std::vector<std::string> l)
    : labels(std::move(l)), texts(labels.size()) {}

static char TypedChar(const keymap::Chord &c) {
    if(c.ctrl || c.alt) return 0;
    const std::string &k = c.key;
    if(k.size() == 1 && !c.shift &&
       ((k[0] >= '0' && k[0] <= '9') || k[0] == '.' || k[0] == '-')) {
        return k[0];
    }
    if(k.size() == 4 && k.rfind("Num", 0) == 0 && k[3] >= '0' && k[3] <= '9') return k[3];
    if(k == "Num.") return '.';
    if(k == "Num-") return '-';
    return 0;
}

bool TypedFields::HandleChord(const keymap::Chord &c) {
    if(labels.empty() || c.ctrl || c.alt) return false;
    if(char ch = TypedChar(c)) {
        texts[active] += ch;
        return true;
    }
    if(c.key == "Backspace") {
        if(!texts[active].empty()) texts[active].pop_back();
        return true;
    }
    if(c.key == "Tab") {
        int n  = (int)labels.size();
        active = c.shift ? (active + n - 1) % n : (active + 1) % n;
        return true;
    }
    return false;
}

bool TypedFields::Any() const {
    for(const std::string &t : texts) {
        if(!t.empty()) return true;
    }
    return false;
}

void TypedFields::Clear() {
    for(std::string &t : texts) t.clear();
    active = 0;
}

std::optional<double> TypedFields::ValueRaw(int i) const {
    if(texts[i].empty()) return std::nullopt;
    Expr *e = Expr::From(texts[i], /*popUpError=*/false);
    if(!e) return std::nullopt;
    return e->Eval();
}

std::optional<double> TypedFields::ValueMm(int i) const {
    if(texts[i].empty()) return std::nullopt;
    Expr *e = Expr::From(texts[i], /*popUpError=*/false);
    if(!e) return std::nullopt;
    return SS.ExprToMm(e);
}

}
}
