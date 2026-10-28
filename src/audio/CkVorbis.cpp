// CkVorbis.cpp - [PC] the Ogg Vorbis decoder for Cricket stream sounds
// (Cki::VorbisStreamSound in the original), using stb_vorbis (public domain,
// third_party/stb/stb_vorbis.c v1.22). Kept in its own translation unit:
// stb_vorbis defines short macro names.
#include "audio/CkVorbis.h"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#pragma warning(disable : 4244 4245 4456 4457 4701 4100 4996)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#define STB_VORBIS_NO_PUSHDATA_API
#include "stb/stb_vorbis.c"

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace py::ckvorbis {

void* open(const char* path, int* channels, int* rate, long* frames) {
    int err = 0;
    stb_vorbis* v = stb_vorbis_open_filename(path, &err, nullptr);
    if (!v) return nullptr;
    const stb_vorbis_info info = stb_vorbis_get_info(v);
    if (info.channels < 1 || info.channels > 2 || info.sample_rate == 0) {
        stb_vorbis_close(v);
        return nullptr;
    }
    if (channels) *channels = info.channels;
    if (rate) *rate = static_cast<int>(info.sample_rate);
    if (frames) *frames = static_cast<long>(stb_vorbis_stream_length_in_samples(v));
    return v;
}

int decode(void* h, float* out, int channels, int frames) {
    if (!h || frames <= 0) return 0;
    return stb_vorbis_get_samples_float_interleaved(static_cast<stb_vorbis*>(h), channels, out, frames * channels);
}

bool seek(void* h, long frame) {
    if (!h) return false;
    if (frame <= 0) return stb_vorbis_seek_start(static_cast<stb_vorbis*>(h)) != 0;
    return stb_vorbis_seek(static_cast<stb_vorbis*>(h), static_cast<unsigned>(frame)) != 0;
}

void close(void* h) {
    if (h) stb_vorbis_close(static_cast<stb_vorbis*>(h));
}

}  // namespace py::ckvorbis
