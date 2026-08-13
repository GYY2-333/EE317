/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org

  MODULE: GL_MODERN.H:  minimal modern-OpenGL loader + helper layer

  BrainBay historically renders with the OpenGL 1.1 fixed-function pipeline
  (only <GL/gl.h> / <GL/glu.h> are included). Windows' opengl32.dll exports
  only the 1.1 entry points; everything from OpenGL 2.0+ (shaders, VBO/VAO,
  FBO, ...) must be fetched at runtime through wglGetProcAddress.

  This module is a lightweight, self-contained loader (a "mini GLAD") that
  resolves exactly the modern GL functions we need, plus a small helper API
  (shader compilation, mesh/VBO wrappers and a tiny mat4 math utility) used by
  the modernized particle renderer and the new medical signal scope object.

  It intentionally uses a *compatibility* context (the existing
  wglCreateContext), so the legacy fixed-function code keeps working unchanged.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.

-----------------------------------------------------------------------------*/

#ifndef GL_MODERN_H
#define GL_MODERN_H

#include <windows.h>
#include <GL/gl.h>

// ---------------------------------------------------------------------------
//  Modern GL constants that are not present in the ancient <GL/gl.h>
// ---------------------------------------------------------------------------
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_STREAM_DRAW                    0x88E0
#define GL_TEXTURE0                       0x84C0
#define GL_FRAMEBUFFER                    0x8D40
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_CLAMP_TO_EDGE                  0x812F
#endif

// Some old GL headers lack these pointer-size integer typedefs used by the
// buffer-data entry points.
#include <stddef.h>
#ifndef GLchar
typedef char      GLchar;
#endif
typedef ptrdiff_t GLsizeiptrARB_bb;
typedef ptrdiff_t GLintptrARB_bb;

// ---------------------------------------------------------------------------
//  Function-pointer typedefs + externs (resolved by glm_load_functions)
// ---------------------------------------------------------------------------
typedef GLuint (APIENTRY *PFN_glCreateShader)(GLenum);
typedef void   (APIENTRY *PFN_glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (APIENTRY *PFN_glCompileShader)(GLuint);
typedef void   (APIENTRY *PFN_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint (APIENTRY *PFN_glCreateProgram)(void);
typedef void   (APIENTRY *PFN_glAttachShader)(GLuint, GLuint);
typedef void   (APIENTRY *PFN_glLinkProgram)(GLuint);
typedef void   (APIENTRY *PFN_glGetProgramiv)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (APIENTRY *PFN_glUseProgram)(GLuint);
typedef void   (APIENTRY *PFN_glDeleteShader)(GLuint);
typedef void   (APIENTRY *PFN_glDeleteProgram)(GLuint);
typedef GLint  (APIENTRY *PFN_glGetUniformLocation)(GLuint, const GLchar*);
typedef GLint  (APIENTRY *PFN_glGetAttribLocation)(GLuint, const GLchar*);
typedef void   (APIENTRY *PFN_glUniform1i)(GLint, GLint);
typedef void   (APIENTRY *PFN_glUniform1f)(GLint, GLfloat);
typedef void   (APIENTRY *PFN_glUniform2f)(GLint, GLfloat, GLfloat);
typedef void   (APIENTRY *PFN_glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *PFN_glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *PFN_glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);

typedef void   (APIENTRY *PFN_glGenBuffers)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFN_glDeleteBuffers)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFN_glBindBuffer)(GLenum, GLuint);
typedef void   (APIENTRY *PFN_glBufferData)(GLenum, GLsizeiptrARB_bb, const void*, GLenum);
typedef void   (APIENTRY *PFN_glBufferSubData)(GLenum, GLintptrARB_bb, GLsizeiptrARB_bb, const void*);
typedef void   (APIENTRY *PFN_glGenVertexArrays)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFN_glDeleteVertexArrays)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFN_glBindVertexArray)(GLuint);
typedef void   (APIENTRY *PFN_glEnableVertexAttribArray)(GLuint);
typedef void   (APIENTRY *PFN_glDisableVertexAttribArray)(GLuint);
typedef void   (APIENTRY *PFN_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void   (APIENTRY *PFN_glVertexAttribDivisor)(GLuint, GLuint);
typedef void   (APIENTRY *PFN_glDrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei);
typedef void   (APIENTRY *PFN_glActiveTexture)(GLenum);

extern PFN_glCreateShader        glm_CreateShader;
extern PFN_glShaderSource        glm_ShaderSource;
extern PFN_glCompileShader       glm_CompileShader;
extern PFN_glGetShaderiv         glm_GetShaderiv;
extern PFN_glGetShaderInfoLog    glm_GetShaderInfoLog;
extern PFN_glCreateProgram       glm_CreateProgram;
extern PFN_glAttachShader        glm_AttachShader;
extern PFN_glLinkProgram         glm_LinkProgram;
extern PFN_glGetProgramiv        glm_GetProgramiv;
extern PFN_glGetProgramInfoLog   glm_GetProgramInfoLog;
extern PFN_glUseProgram          glm_UseProgram;
extern PFN_glDeleteShader        glm_DeleteShader;
extern PFN_glDeleteProgram       glm_DeleteProgram;
extern PFN_glGetUniformLocation  glm_GetUniformLocation;
extern PFN_glGetAttribLocation   glm_GetAttribLocation;
extern PFN_glUniform1i           glm_Uniform1i;
extern PFN_glUniform1f           glm_Uniform1f;
extern PFN_glUniform2f           glm_Uniform2f;
extern PFN_glUniform3f           glm_Uniform3f;
extern PFN_glUniform4f           glm_Uniform4f;
extern PFN_glUniformMatrix4fv    glm_UniformMatrix4fv;
extern PFN_glGenBuffers          glm_GenBuffers;
extern PFN_glDeleteBuffers       glm_DeleteBuffers;
extern PFN_glBindBuffer          glm_BindBuffer;
extern PFN_glBufferData          glm_BufferData;
extern PFN_glBufferSubData       glm_BufferSubData;
extern PFN_glGenVertexArrays     glm_GenVertexArrays;
extern PFN_glDeleteVertexArrays  glm_DeleteVertexArrays;
extern PFN_glBindVertexArray     glm_BindVertexArray;
extern PFN_glEnableVertexAttribArray  glm_EnableVertexAttribArray;
extern PFN_glDisableVertexAttribArray glm_DisableVertexAttribArray;
extern PFN_glVertexAttribPointer glm_VertexAttribPointer;
extern PFN_glVertexAttribDivisor glm_VertexAttribDivisor;
extern PFN_glDrawArraysInstanced glm_DrawArraysInstanced;
extern PFN_glActiveTexture       glm_ActiveTexture;

// ---------------------------------------------------------------------------
//  Loader
// ---------------------------------------------------------------------------
// Resolve all modern GL entry points via wglGetProcAddress. Must be called
// while a GL context is current. Safe to call multiple times (loads once).
// Returns TRUE if the essential shader/VBO functions are available.
BOOL ensureGladLoaded(void);

// TRUE once ensureGladLoaded() has succeeded and modern GL is usable.
BOOL glmModernAvailable(void);

// ---------------------------------------------------------------------------
//  Tiny 4x4 matrix helper (column-major, GL friendly)
// ---------------------------------------------------------------------------
typedef struct { float m[16]; } glmMat4;

glmMat4 glmIdentity(void);
glmMat4 glmOrtho(float left, float right, float bottom, float top, float znear, float zfar);
glmMat4 glmPerspective(float fovyDeg, float aspect, float znear, float zfar);
glmMat4 glmMultiply(const glmMat4 a, const glmMat4 b);
glmMat4 glmTranslate(float x, float y, float z);
glmMat4 glmScale(float x, float y, float z);
glmMat4 glmRotateX(float deg);
glmMat4 glmRotateY(float deg);

// ---------------------------------------------------------------------------
//  Shader helper
// ---------------------------------------------------------------------------
// Compile+link a vertex/fragment shader pair. Returns program id (0 on error).
GLuint glmBuildProgram(const char *vertexSrc, const char *fragmentSrc);

#endif // GL_MODERN_H
