// GLFunctions.h - runtime-loaded OpenGL entry points.
//
// The original renderer uses only OpenGL ES 2.0 calls (see the import table:
// glAttachShader ... glViewport). Desktop OpenGL 3.3 core and GLES 3 both
// provide all of them, so the port loads exactly that set through
// SDL_GL_GetProcAddress. Phase 0 only needs a handful; the full list is
// declared so later phases can use it without touching the loader.
#pragma once

#include <cstddef>
#include <cstdint>

namespace py::gl {

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
using GLubyte = unsigned char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

constexpr GLenum COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum DEPTH_BUFFER_BIT = 0x00000100;
constexpr GLenum VENDOR = 0x1F00;
constexpr GLenum RENDERER = 0x1F01;
constexpr GLenum VERSION = 0x1F02;
constexpr GLenum SHADING_LANGUAGE_VERSION = 0x8B8C;

#if defined(_WIN32)
#define PY_GLAPI __stdcall
#else
#define PY_GLAPI
#endif

#define PY_GL_FUNCS(X)                                                                  \
    X(void, ClearColor, GLfloat, GLfloat, GLfloat, GLfloat)                             \
    X(void, Clear, GLbitfield)                                                          \
    X(void, Viewport, GLint, GLint, GLsizei, GLsizei)                                   \
    X(const GLubyte*, GetString, GLenum)                                                \
    X(GLenum, GetError)                                                                 \
    X(void, Enable, GLenum)                                                             \
    X(void, Disable, GLenum)                                                            \
    X(void, BlendFunc, GLenum, GLenum)                                                  \
    X(void, DepthFunc, GLenum)                                                          \
    X(void, DepthMask, GLboolean)                                                       \
    X(void, CullFace, GLenum)                                                           \
    X(void, FrontFace, GLenum)                                                          \
    X(void, Scissor, GLint, GLint, GLsizei, GLsizei)                                    \
    X(void, ActiveTexture, GLenum)                                                      \
    X(void, BindTexture, GLenum, GLuint)                                                \
    X(void, GenTextures, GLsizei, GLuint*)                                              \
    X(void, DeleteTextures, GLsizei, const GLuint*)                                     \
    X(void, TexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) \
    X(void, TexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) \
    X(void, TexParameteri, GLenum, GLenum, GLint)                                       \
    X(void, GenerateMipmap, GLenum)                                                     \
    X(void, PixelStorei, GLenum, GLint)                                                 \
    X(void, GenBuffers, GLsizei, GLuint*)                                               \
    X(void, DeleteBuffers, GLsizei, const GLuint*)                                      \
    X(void, BindBuffer, GLenum, GLuint)                                                 \
    X(void, BufferData, GLenum, GLsizeiptr, const void*, GLenum)                        \
    X(void, GenVertexArrays, GLsizei, GLuint*)                                          \
    X(void, BindVertexArray, GLuint)                                                    \
    X(void, EnableVertexAttribArray, GLuint)                                            \
    X(void, DisableVertexAttribArray, GLuint)                                           \
    X(void, VertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) \
    X(void, DrawArrays, GLenum, GLint, GLsizei)                                         \
    X(void, DrawElements, GLenum, GLsizei, GLenum, const void*)                         \
    X(GLuint, CreateShader, GLenum)                                                     \
    X(void, ShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*)          \
    X(void, CompileShader, GLuint)                                                      \
    X(void, GetShaderiv, GLuint, GLenum, GLint*)                                        \
    X(void, GetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)                       \
    X(void, DeleteShader, GLuint)                                                       \
    X(GLuint, CreateProgram)                                                            \
    X(void, AttachShader, GLuint, GLuint)                                               \
    X(void, BindAttribLocation, GLuint, GLuint, const GLchar*)                          \
    X(void, LinkProgram, GLuint)                                                        \
    X(void, GetProgramiv, GLuint, GLenum, GLint*)                                       \
    X(void, GetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)                      \
    X(void, UseProgram, GLuint)                                                         \
    X(void, DeleteProgram, GLuint)                                                      \
    X(GLint, GetUniformLocation, GLuint, const GLchar*)                                 \
    X(GLint, GetAttribLocation, GLuint, const GLchar*)                                  \
    X(void, Uniform1i, GLint, GLint)                                                    \
    X(void, Uniform1f, GLint, GLfloat)                                                  \
    X(void, Uniform3fv, GLint, GLsizei, const GLfloat*)                                 \
    X(void, Uniform4fv, GLint, GLsizei, const GLfloat*)                                 \
    X(void, UniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat*)

#define PY_GL_DECLARE(ret, name, ...) \
    using PFN_##name = ret(PY_GLAPI*)(__VA_ARGS__); \
    extern PFN_##name name;
PY_GL_FUNCS(PY_GL_DECLARE)
#undef PY_GL_DECLARE

// Loads every function above with the given loader (SDL_GL_GetProcAddress).
// Returns the number of functions that could not be resolved.
using ProcLoader = void* (*)(const char*);
int load(ProcLoader loader);

}  // namespace py::gl
