// CkVorbis.h - [PC] a minimal Ogg Vorbis file decoder interface (stb_vorbis)
#pragma once

namespace py::ckvorbis {

// null on failure; channels 1 or 2
void* open(const char* path, int* channels, int* rate, long* frames);
// interleaved floats; returns the frames decoded (0 at the end)
int decode(void* h, float* out, int channels, int frames);
bool seek(void* h, long frame);
void close(void* h);

}  // namespace py::ckvorbis
