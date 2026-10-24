//-----------------------------------------------------------------------------
// Touch-screen fingers as the mouse events SolveSpace's tools and views take, as in the classic
// web interface: one finger is the left button (tools, picking, dragging), two fingers pan
// (PAN: the middle button with Shift, never a right click's context menu) and pinch to zoom
// (the wheel, about the fingers), three fingers drag with the middle button (orbit). A long press is a right click (the context menu; it also
// ends a tool, as Esc does).
//
// A lone finger presses only once it moves (or lifts, a tap), so a second finger landing just
// after it starts a pan instead of a click. Positions are window coordinates; times are
// milliseconds.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_PLATFORM_SDL_TOUCHGESTURES_H
#define SOLVESPACE_PLATFORM_SDL_TOUCHGESTURES_H

#include <cstdint>
#include <map>
#include <vector>

namespace SolveSpace {
namespace Platform {

class TouchGestures {
public:
    static constexpr uint64_t LONG_PRESS_MS = 500;
    static constexpr double   SLOP    = 8; // how far a waiting finger moves before it drags

    struct Out {
        enum class Kind { MOTION, PRESS, RELEASE, WHEEL };
        enum class Button { NONE, LEFT, MIDDLE, RIGHT, PAN };
        Kind   kind;
        double x, y;
        Button button; // MOTION: the button held, if any
        double wheel;  // WHEEL: zoom by 1.2^wheel
    };

    void FingerDown(uint64_t id, double x, double y, uint64_t ms);
    void FingerMove(uint64_t id, double x, double y, uint64_t ms);
    void FingerUp(uint64_t id, double x, double y, uint64_t ms);
    void Cancel();
    void Tick(uint64_t ms); // a lone finger held still long enough is a long press

    bool Waiting() const; // a lone finger has not pressed yet: call Tick() later
    bool Active() const;  // any finger is down
    // The mouse events since the last Take(), once a frame: two or three fingers' moves since
    // then are one motion (and one zoom step).
    std::vector<Out> Take();

private:
    enum class Mode { IDLE, WAITING, ONE, MULTI, ENDED };
    struct Pt {
        double x, y;
    };

    Pt     Centroid() const;
    double Spread() const;
    void   Emit(Out::Kind k, Pt p, Out::Button b, double wheel = 0);
    void   CommitOne();
    void   Click(Out::Button b);
    void   StartMulti();
    void   Flush();

    std::map<uint64_t, Pt> fingers;
    Mode                   mode = Mode::IDLE;
    uint64_t               oneId = 0, downMs = 0;
    Pt                     start = {}, last = {};
    Out::Button            button = Out::Button::NONE;
    double                 spread = 0;
    bool                   moved  = false; // fingers of a MULTI gesture moved since Flush()
    std::vector<Out>       outs;
};

} // namespace Platform
} // namespace SolveSpace

#endif
