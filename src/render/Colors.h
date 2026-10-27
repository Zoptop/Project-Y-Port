// Colors.h - ProjectY::CVector3::COL_* colour constants (render-side Vec3).
// Values read from the original's static initialisers under emulation
// (bit patterns in the comments where they are not exact decimals).
#pragma once

#include "core/FloatBits.h"
#include "render/Math3D.h"

namespace py::colors {

inline const Vec3 COL_BLACK{0.0f, 0.0f, 0.0f};
inline const Vec3 COL_WHITE{1.0f, 1.0f, 1.0f};
inline const Vec3 COL_RED{1.0f, 0.0f, 0.0f};
inline const Vec3 COL_GREEN{0.0f, 1.0f, 0.0f};
inline const Vec3 COL_BLUE{0.0f, 0.0f, 1.0f};
inline const Vec3 COL_YELLOW{1.0f, 1.0f, 0.0f};
inline const Vec3 COL_CYAN{0.0f, 1.0f, 1.0f};
inline const Vec3 COL_MAGENTA{1.0f, 0.0f, 1.0f};
inline const Vec3 COL_ORANGE{1.0f, 0.5f, 0.0f};
inline const Vec3 COL_VIOLET{1.0f, 0.0f, 0.5f};
inline const Vec3 COL_DARKMAGENTA{0.5f, 0.0f, 1.0f};
inline const Vec3 COL_SKYBLUE{0.0f, 0.5f, 1.0f};
inline const Vec3 COL_SPRINGGREEN{0.0f, 1.0f, 0.5f};
inline const Vec3 COL_LIME{0.5f, 1.0f, 0.0f};
inline const Vec3 COL_GRAY15{bitsf(0x3e19999a), bitsf(0x3e19999a), bitsf(0x3e19999a)};
inline const Vec3 COL_GRAY25{0.25f, 0.25f, 0.25f};
inline const Vec3 COL_GRAY50{0.5f, 0.5f, 0.5f};
inline const Vec3 COL_GRAY75{0.75f, 0.75f, 0.75f};
inline const Vec3 COL_GRAY90{bitsf(0x3f666666), bitsf(0x3f666666), bitsf(0x3f666666)};
inline const Vec3 COL_GOLD{1.0f, bitsf(0x3f57d7d4), 0.0f};                              // 215/255
inline const Vec3 COL_SILVER{bitsf(0x3f53d3d4), bitsf(0x3f53d3d4), bitsf(0x3f53d3d4)};   // 211/255
inline const Vec3 COL_BRONZE{bitsf(0x3f52d2cc), bitsf(0x3ed2d2dd), bitsf(0x3df0f0e9)};
inline const Vec3 COL_RESOURCE{bitsf(0x3f547ae1), bitsf(0x3e78d4fe), bitsf(0x3e3c6a7f)};   // 0.83 0.243 0.184

}  // namespace py::colors
