//-----------------------------------------------------------------------------
// What the Properties panel says about the selection: the property browser's
// DescribeSelection as data.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_SELECTIONMODEL_H
#define SOLVESPACE_UI_SELECTIONMODEL_H

#include <string>
#include <vector>
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

// One labelled value. At most one link is set; clicking the row selects (or shows) it.
struct SelectionRow {
    std::string label, value;
    hEntity     entity;
    hConstraint constraint;
    hGroup      group;
    hStyle      style;
};

struct SelectionSection {
    std::string               title; // "constrained by", "measured by", "constrains", "measures"
    std::vector<SelectionRow> rows;
};

struct SelectionModel {
    bool                          empty = true;
    int                           count = 0;
    std::string                   title; // "LINE SEGMENT", "TWO POINTS", "DIMENSION", ...
    std::vector<SelectionRow>     rows;
    std::vector<SelectionSection> sections;

    // Exactly one constraint selected.
    hConstraint constraint;
    bool        isComment = false, hasValue = false, reference = false;
    bool        isDiameter = false, showAsRadius = false;
    std::string valueText; // as the dimension editor shows it

    // A TrueType text entity from a request.
    hRequest    ttf;
    std::string ttfText, ttfFont;
    bool        ttfKerning = false;

    bool styleAssigned = false; // some selected item has a style: offer to remove it
};

// Calls SS.GW.GroupSelection().
SelectionModel BuildSelection();

}
}

#endif
