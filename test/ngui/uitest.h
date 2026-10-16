//-----------------------------------------------------------------------------
// Helpers for UI tests (solvespace-ng-uitest): where things are on screen, and the steps a
// person takes that need more than one driver call.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_TEST_NGUI_UITEST_H
#define SOLVESPACE_TEST_NGUI_UITEST_H

#include <optional>
#include <string>
#include "solvespace.h"
#include "platform/sdl/uidriver.h"

namespace Driver = SolveSpace::Platform::Driver;

namespace SolveSpace {
namespace UiTest {

// Model points and entities on screen (window coordinates).
Point2d At(Vector modelPoint);
Point2d AtPoint(hEntity point);
Point2d AtMiddle(hRequest line); // the middle of a line segment
Point2d AtLabel(hConstraint c);  // a dimension's label

// Clicks, as a person would, with Shift held for adding to the selection.
void ClickAt(Point2d p);
void ShiftClickAt(Point2d p);
void DoubleClickAt(Point2d p);

// The property browser: its text, row by row (non-ASCII shown as '?'), and clicking a piece of
// text in it, e.g. a link. False if the text is not shown or is scrolled out of view.
std::vector<std::string> BrowserRows();
bool                     BrowserShows(const std::string &text);
bool                     ClickBrowserText(const std::string &text, int occurrence = 0);

// Draws a line segment with the line tool by clicking its two ends; returns its request.
hRequest DrawLine(Point2d a, Point2d b);
// The most recently added request / constraint of a type, or none.
std::optional<hRequest>    LastRequest(Request::Type type);
std::optional<hConstraint> LastConstraint(Constraint::Type type);
int                        CountConstraints(Constraint::Type type);

// The open pie menu's item labels, and picking one by moving the mouse onto it and clicking.
std::vector<std::string> PieLabels();
bool                     PickPie(const std::string &label);

// Right-clicks at p and, inside the context menu's nested loop, clicks the item labelled
// `item`. False if the menu had no such item.
bool ContextMenu(Point2d p, const std::string &item);

}
}

#endif
