//-----------------------------------------------------------------------------
// Touch-screen fingers as mouse events (touchgestures.h).
//-----------------------------------------------------------------------------
#include <cmath>
#include "touchgestures.h"

namespace SolveSpace {
namespace Platform {

TouchGestures::Pt TouchGestures::Centroid() const {
    Pt c = {};
    for(const auto &f : fingers) {
        c.x += f.second.x;
        c.y += f.second.y;
    }
    if(!fingers.empty()) {
        c.x /= (double)fingers.size();
        c.y /= (double)fingers.size();
    }
    return c;
}

// The distance between the first two fingers, for pinching.
double TouchGestures::Spread() const {
    if(fingers.size() < 2) return 0;
    auto a = fingers.begin(), b = std::next(a);
    return std::hypot(a->second.x - b->second.x, a->second.y - b->second.y);
}

void TouchGestures::Emit(Out::Kind k, Pt p, Out::Button b, double wheel) {
    outs.push_back({ k, p.x, p.y, b, wheel });
}

// The lone finger is a mouse with its left button down at where it landed.
void TouchGestures::CommitOne() {
    mode   = Mode::ONE;
    button = Out::Button::LEFT;
    Emit(Out::Kind::MOTION, start, Out::Button::NONE);
    Emit(Out::Kind::PRESS, start, button);
    last = start;
}

// A click where the lone finger landed; the fingers then do nothing until all lift.
void TouchGestures::Click(Out::Button b) {
    Emit(Out::Kind::MOTION, start, Out::Button::NONE);
    Emit(Out::Kind::PRESS, start, b);
    Emit(Out::Kind::RELEASE, start, b);
    mode = Mode::ENDED;
}

// Two fingers or three: pan or orbit about their centre.
void TouchGestures::StartMulti() {
    if(mode == Mode::MULTI) Emit(Out::Kind::RELEASE, last, button);
    mode   = Mode::MULTI;
    moved  = false;
    button = fingers.size() == 2 ? Out::Button::PAN : Out::Button::MIDDLE;
    last   = Centroid();
    spread = Spread();
    Emit(Out::Kind::MOTION, last, Out::Button::NONE);
    Emit(Out::Kind::PRESS, last, button);
}

// Fingers move one at a time: their gesture moves once for all of them.
void TouchGestures::Flush() {
    if(mode != Mode::MULTI || !moved) return;
    moved = false;
    last  = Centroid();
    Emit(Out::Kind::MOTION, last, button);
    if(fingers.size() == 2) {
        double s = Spread();
        if(spread > 0 && s > 0 && s != spread) {
            Emit(Out::Kind::WHEEL, last, Out::Button::NONE, std::log(s / spread) / std::log(1.2));
        }
        spread = s;
    }
}

void TouchGestures::FingerDown(uint64_t id, double x, double y, uint64_t ms) {
    Flush();
    fingers[id] = { x, y };
    switch(mode) {
        case Mode::IDLE:
            mode   = Mode::WAITING;
            oneId  = id;
            downMs = ms;
            start  = { x, y };
            break;
        case Mode::WAITING: StartMulti(); break;
        case Mode::MULTI:
            if(fingers.size() <= 3) StartMulti();
            break;
        case Mode::ONE:
        case Mode::ENDED: break; // a drag has begun, or the gesture is over
    }
}

void TouchGestures::FingerMove(uint64_t id, double x, double y, uint64_t ms) {
    auto it = fingers.find(id);
    if(it == fingers.end()) return;
    it->second = { x, y };
    switch(mode) {
        case Mode::WAITING:
            if(std::hypot(x - start.x, y - start.y) < SLOP) break;
            CommitOne();
            [[fallthrough]]; // and drag
        case Mode::ONE:
            if(id != oneId) break;
            last = { x, y };
            Emit(Out::Kind::MOTION, last, button);
            break;
        case Mode::MULTI: moved = true; break;
        case Mode::IDLE:
        case Mode::ENDED: break;
    }
}

void TouchGestures::FingerUp(uint64_t id, double x, double y, uint64_t ms) {
    if(fingers.find(id) == fingers.end()) return;
    Flush();
    switch(mode) {
        case Mode::WAITING: Click(Out::Button::LEFT); break; // a tap
        case Mode::ONE:
            if(id != oneId) break;
            last = { x, y };
            Emit(Out::Kind::RELEASE, last, button);
            mode = Mode::ENDED;
            break;
        case Mode::MULTI:
            Emit(Out::Kind::RELEASE, last, button);
            mode = Mode::ENDED;
            break;
        case Mode::IDLE:
        case Mode::ENDED: break;
    }
    fingers.erase(id);
    if(fingers.empty()) {
        mode   = Mode::IDLE;
        button = Out::Button::NONE;
    }
}

void TouchGestures::Cancel() {
    Flush();
    if(mode == Mode::ONE || mode == Mode::MULTI) Emit(Out::Kind::RELEASE, last, button);
    fingers.clear();
    mode   = Mode::IDLE;
    button = Out::Button::NONE;
}

void TouchGestures::Tick(uint64_t ms) {
    if(mode == Mode::WAITING && ms >= downMs + LONG_PRESS_MS) Click(Out::Button::RIGHT);
}

bool TouchGestures::Waiting() const {
    return mode == Mode::WAITING;
}

bool TouchGestures::Active() const {
    return !fingers.empty();
}

std::vector<TouchGestures::Out> TouchGestures::Take() {
    Flush();
    std::vector<Out> r;
    r.swap(outs);
    return r;
}

} // namespace Platform
} // namespace SolveSpace
