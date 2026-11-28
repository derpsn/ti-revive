// The brokers' audio OUT source (outfill.h): playback waits for a pre-fill, ends on a dry ring, and drops the oldest
// frames when the writer got too far ahead (review A11: one copy for both brokers). Hardware-free.
#include "check.h"
extern "C" {
#include "outfill.h"
}

TEST(outfill_waits_for_the_prefill_then_plays)
{
    vti_outfill s{};
    int64_t r = 0;
    CHECK_EQ(vti_outfill_take(&s, VTI_OUT_PREFILL - 1, &r, 64), 0u); // not enough yet: silence
    CHECK_EQ(r, 0);
    CHECK_EQ(vti_outfill_take(&s, VTI_OUT_PREFILL, &r, 64), 64u);
    CHECK_EQ(s.playing, 1);
}

TEST(outfill_dry_ring_counts_an_underrun_and_refills)
{
    vti_outfill s{};
    int64_t r = 0;
    CHECK_EQ(vti_outfill_take(&s, VTI_OUT_PREFILL, &r, VTI_OUT_PREFILL + 10), (uint32_t)VTI_OUT_PREFILL);
    CHECK_EQ(s.underruns, 1u);
    CHECK_EQ(s.playing, 0);
}

TEST(outfill_drops_the_oldest_when_far_ahead)
{
    vti_outfill s{};
    int64_t r = 0;
    vti_outfill_take(&s, VTI_OUT_PREFILL, &r, 0); // playing
    const int64_t w = VTI_OUT_MAX + 100;
    CHECK_EQ(vti_outfill_take(&s, w, &r, 64), 64u);
    CHECK_EQ(r, w - VTI_OUT_PREFILL);
    CHECK_EQ(s.dropped, (uint64_t)(w - VTI_OUT_PREFILL));
}

TEST(outfill_writer_restart_resynchronises)
{
    vti_outfill s{};
    s.playing = 1;
    int64_t r = 5000;
    CHECK_EQ(vti_outfill_take(&s, 100, &r, 64), 0u); // write index went back
    CHECK_EQ(r, 100);
    CHECK_EQ(s.playing, 0);
}
