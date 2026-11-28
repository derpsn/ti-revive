// framepace.h - the editor's redraws paced to what they cost (#37, E607/E608). A knob drag repainted on every mouse
// move; on a slow Mac a partial repaint took 25-35 ms, so the host's main thread drew most of the time, its own GUI
// lagged and fewer mouse moves (values) reached Logic's automation. Now the dirty areas are collected and drawn at most
// once per frame, and a frame waits until twice its drawing cost has passed: drawing stays below a third of the main
// thread on any machine, at display rate where drawing is cheap. Values go to the host at once, as before.
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace vti {

struct FramePacer {
    static constexpr int64_t MIN_FRAME_US = 16000; // ~60 frames a second where drawing is cheap
    static constexpr int64_t SAME_FRAME_US = 2000; // a draw call this soon after the last one belongs to its frame

    bool due(int64_t now_us) const { return now_us >= next_us_; }

    // one draw call (the host draws each dirty rect in its own call): the frame's cost adds up over its calls
    void drawn(int64_t start_us, int64_t end_us)
    {
        cost_us_ = start_us - last_end_us_ <= SAME_FRAME_US ? cost_us_ + (end_us - start_us) : end_us - start_us;
        last_end_us_ = end_us;
        next_us_ = end_us + std::max(MIN_FRAME_US, 2 * cost_us_);
    }

  private:
    int64_t next_us_ = 0, last_end_us_ = INT64_MIN / 2, cost_us_ = 0;
};

// the dirty areas waiting for the next frame; only overlapping ones merge (a union of the knob and the LCD would
// repaint the whole panel between them)
struct DirtyRects {
    struct Rect {
        int x0, y0, x1, y1;
    };

    void add(int x0, int y0, int x1, int y1)
    {
        Rect n{x0, y0, x1, y1};
        for (bool merged = true; merged;) { // a grown area may now touch others
            merged = false;
            for (size_t i = 0; i < rects_.size(); i++)
                if (n.x0 <= rects_[i].x1 && rects_[i].x0 <= n.x1 && n.y0 <= rects_[i].y1 && rects_[i].y0 <= n.y1) {
                    n = {std::min(n.x0, rects_[i].x0), std::min(n.y0, rects_[i].y0), std::max(n.x1, rects_[i].x1),
                         std::max(n.y1, rects_[i].y1)};
                    rects_.erase(rects_.begin() + (long)i);
                    merged = true;
                    break;
                }
        }
        rects_.push_back(n);
    }
    bool empty() const { return rects_.empty(); }
    std::vector<Rect> take()
    {
        std::vector<Rect> r;
        r.swap(rects_);
        return r;
    }

  private:
    std::vector<Rect> rects_;
};

} // namespace vti
