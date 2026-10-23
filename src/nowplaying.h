/* ------------------------------------------------------------------
   Linamp — "now playing" publisher.

   Companion tools (linyrics, scripts, status bars) need to know what the
   player is doing. Instead of embedding a D-Bus/MPRIS stack (which would
   pull in a whole IPC library for ~5 fields), the player publishes a tiny
   JSON snapshot to the runtime dir and whoever cares reads it.

   $XDG_RUNTIME_DIR/linamp/nowplaying.json   (fallback: cache dir)

   `mono` is CLOCK_MONOTONIC in milliseconds taken at the same instant as
   `position`, so a reader can interpolate the exact playback position
   instead of being limited by the publish interval.
   ------------------------------------------------------------------ */
#pragma once

#include <cstdint>
#include <string>

struct NowPlaying {
    std::string title;
    std::string artist;
    std::string album;
    std::string path; // file:// URL
    int64_t duration = 0;
    int64_t position = 0;
    bool playing = false;
    int seq = 0; // bumps on every track change
};

class NowPlayingPublisher {
public:
    ~NowPlayingPublisher();

    /* Publishes at most ~10 times per second; unchanged snapshots are skipped. */
    void update(const NowPlaying &np);
    /* Final publish with playing=false (called on shutdown). */
    void clear();

    static std::string filePath();

private:
    bool write(const NowPlaying &np);

    std::string m_last;
    int64_t m_lastWriteMono = -1000000;
    bool m_wrote = false;
};
