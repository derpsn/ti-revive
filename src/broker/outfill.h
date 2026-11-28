// outfill.h - the brokers' audio OUT source (vua_set_out / vla_start): frames the writing client (ASIO driver, Core
// Audio) appended to the output ring. Playback starts once VTI_OUT_PREFILL frames are buffered (so one late delivery
// does not underrun); if the client got ahead by more than VTI_OUT_MAX (e.g. after a stall), the oldest frames are
// dropped to bound the latency. Silence otherwise. Plain C, no platform headers: both brokers use it (they had drifted
// copies, review A11); the unit test calls it.
#pragma once
#include <stdint.h>
#include "vtiaudioshm.h" // VTI_AUDIO_RING

#define VTI_OUT_PREFILL 512
#define VTI_OUT_MAX 2048

typedef struct {
    int playing;
    uint64_t underruns, dropped, frames;
} vti_outfill;

// w = the ring's write index, *r = its read index (moved on a resync or a drop); returns the frames to copy from *r on,
// the caller fills the rest of `frames` with silence and stores *r + n as the new read index
static inline uint32_t vti_outfill_take(vti_outfill *s, int64_t w, int64_t *r, uint32_t frames)
{
    int64_t avail = w - *r;
    if (avail < 0 || avail > VTI_AUDIO_RING) { // writer restarted or far ahead: resynchronise
        *r = w;
        avail = 0;
        s->playing = 0;
    }
    if (!s->playing && avail >= VTI_OUT_PREFILL)
        s->playing = 1;
    if (s->playing && avail > VTI_OUT_MAX) {
        s->dropped += (uint64_t)(avail - VTI_OUT_PREFILL);
        *r = w - VTI_OUT_PREFILL;
        avail = VTI_OUT_PREFILL;
    }
    if (!s->playing)
        return 0;
    const uint32_t n = avail < frames ? (uint32_t)avail : frames;
    if (n < frames) { // ran dry: refill before playing again
        s->underruns++;
        s->playing = 0;
    }
    s->frames += n;
    return n;
}
