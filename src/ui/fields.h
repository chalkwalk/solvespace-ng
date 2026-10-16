//-----------------------------------------------------------------------------
// Panel fields for values the model owns.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_FIELDS_H
#define SOLVESPACE_UI_FIELDS_H

#include <functional>
#include <string>

namespace SolveSpace {
namespace Ui {

// A text field showing the model's value until it is edited; Enter (or leaving the field
// after editing) sets *run to apply the text after the frame.
void ValueField(const char *label, const std::string &value, std::function<void()> *run,
                std::function<void(std::string)> apply);

}
}

#endif
