#include "nowplaying.h"

#include "util.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace {

int64_t monoMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string jsonEscape(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(char(c));
            }
        }
    }
    return out;
}

} // namespace

std::string NowPlayingPublisher::filePath()
{
#ifdef _WIN32
    // Same location Linyrics reads on Windows. XDG_RUNTIME_DIR does not exist
    // there, and a path relative to the working directory would depend on how
    // the player was launched.
    if (const char *x = std::getenv("LOCALAPPDATA"); x && *x)
        return std::string(x) + "/linamp/nowplaying.json";
#endif
    if (const char *x = std::getenv("XDG_RUNTIME_DIR"); x && *x)
        return std::string(x) + "/linamp/nowplaying.json";
    return util::cacheDir() + "/nowplaying.json";
}

NowPlayingPublisher::~NowPlayingPublisher() = default;

void NowPlayingPublisher::update(const NowPlaying &np)
{
    const int64_t mono = monoMs();
    // 10 Hz while the position is moving, 2 Hz while paused — readers treat a
    // snapshot older than ~2.5 s as "player gone".
    const int64_t interval = np.playing ? 90 : 500;
    if (m_wrote && mono - m_lastWriteMono < interval)
        return;
    m_lastWriteMono = mono;
    write(np);
}

void NowPlayingPublisher::clear()
{
    if (!m_wrote)
        return;
    // An empty title marks the player as idle. Readers also drop snapshots
    // whose `mono` stamp is old, so even a hard crash ends up as idle.
    m_last.clear();
    NowPlaying np;
    write(np);
    m_lastWriteMono = monoMs();
}

bool NowPlayingPublisher::write(const NowPlaying &np)
{
    const int64_t mono = monoMs();
    char buf[64];
    std::string js;
    js.reserve(512);
    js += "{\"v\":1";
    js += ",\"title\":\"" + jsonEscape(np.title) + "\"";
    js += ",\"artist\":\"" + jsonEscape(np.artist) + "\"";
    js += ",\"album\":\"" + jsonEscape(np.album) + "\"";
    js += ",\"path\":\"" + jsonEscape(np.path) + "\"";
    std::snprintf(buf, sizeof buf, ",\"duration\":%lld", (long long)np.duration);
    js += buf;
    std::snprintf(buf, sizeof buf, ",\"position\":%lld", (long long)np.position);
    js += buf;
    js += ",\"playing\":";
    js += np.playing ? "true" : "false";
    std::snprintf(buf, sizeof buf, ",\"seq\":%d", np.seq);
    js += buf;
    std::snprintf(buf, sizeof buf, ",\"mono\":%lld", (long long)mono);
    js += buf;
    js += "}\n";

    if (js == m_last) // nothing changed (paused & idle) -> no write at all
        return false;
    m_last = js;

    const std::string path = filePath();
    const std::string tmp = path + ".tmp";
    if (!util::writeFile(tmp, js))
        return false;
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        util::writeFile(path, js);
        std::remove(tmp.c_str());
    }
    m_wrote = true;
    return true;
}
