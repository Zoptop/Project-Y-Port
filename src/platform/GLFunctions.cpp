#include "GLFunctions.h"

#include "core/CUtils.h"

namespace py::gl {

#define PY_GL_DEFINE(ret, name, ...) PFN_##name name = nullptr;
PY_GL_FUNCS(PY_GL_DEFINE)
#undef PY_GL_DEFINE

int load(ProcLoader loader) {
    int missing = 0;
#define PY_GL_LOAD(ret, name, ...)                                                   \
    name = reinterpret_cast<PFN_##name>(loader("gl" #name));                         \
    if (!name) {                                                                     \
        CUtils::log("GL: missing gl%s", #name);                                      \
        ++missing;                                                                   \
    }
    PY_GL_FUNCS(PY_GL_LOAD)
#undef PY_GL_LOAD
    return missing;
}

}  // namespace py::gl
