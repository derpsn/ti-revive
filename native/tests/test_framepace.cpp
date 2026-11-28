// The editor's redraws paced to their own cost (src/editor/framepace.h, #37 E607/E608): on a slow Mac each partial
// repaint took 25-35 ms and a knob drag repainted on every mouse move, so Logic's main thread was busy up to 73 % and
// recorded the drag as a coarse staircase. The values still go to the host on every move; only drawing waits.
#include "check.h"
#include "editor/framepace.h"

TEST(framepace_fast_draws_keep_one_frame_per_display_refresh)
{
    vti::FramePacer p;
    CHECK(p.due(0));
    p.drawn(0, 500); // 0.5 ms (our M3, E608)
    CHECK(!p.due(10000));
    CHECK(p.due(500 + vti::FramePacer::MIN_FRAME_US));
}

TEST(framepace_slow_draws_leave_two_thirds_of_the_main_thread)
{
    vti::FramePacer p;
    p.drawn(0, 30000); // 30 ms (the reporter's Mac, E607)
    CHECK(!p.due(30000 + 59000));
    CHECK(p.due(30000 + 60000)); // the next frame after twice the cost: drawing <= 1/3 of the time
}

TEST(framepace_rects_of_one_frame_add_up)
{
    vti::FramePacer p; // the host draws each dirty rect in its own call, right after each other
    p.drawn(0, 10000);
    p.drawn(10500, 20500); // started within the same frame: 20 ms in all
    CHECK(!p.due(20500 + 39000));
    CHECK(p.due(20500 + 40000));
    p.drawn(100000, 101000); // a new frame later: its own cost only
    CHECK(p.due(101000 + vti::FramePacer::MIN_FRAME_US));
}

TEST(framepace_dirty_rects_merge_only_what_overlaps)
{
    vti::DirtyRects d;
    CHECK(d.empty());
    d.add(10, 10, 50, 50);
    d.add(20, 20, 40, 40); // inside: nothing new
    d.add(45, 10, 90, 50); // overlaps: one area
    d.add(200, 470, 560, 512); // the LCD, far away: stays its own area (a union would repaint the panel between)
    auto r = d.take();
    CHECK_EQ((int)r.size(), 2);
    CHECK(r[0].x0 == 10 && r[0].y0 == 10 && r[0].x1 == 90 && r[0].y1 == 50);
    CHECK(r[1].x0 == 200 && r[1].x1 == 560);
    CHECK(d.empty());
}

TEST(framepace_dirty_rects_merge_chains)
{
    vti::DirtyRects d;
    d.add(0, 0, 10, 10);
    d.add(20, 0, 30, 10);
    d.add(5, 0, 25, 10); // bridges both: one area
    auto r = d.take();
    CHECK_EQ((int)r.size(), 1);
    CHECK(r[0].x0 == 0 && r[0].x1 == 30);
}
