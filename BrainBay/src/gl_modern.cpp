/* -----------------------------------------------------------------------------

    BrainBay  -  OpenSource Biofeedback Software, contact: chris@shifz.org

  MODULE: GL_MODERN.CPP:  minimal modern-OpenGL loader + helper layer

  See gl_modern.h for the rationale. This resolves the modern GL entry points
  through wglGetProcAddress and provides a small matrix + shader helper API.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; See the
  GNU General Public License for more details.

-----------------------------------------------------------------------------*/

#include "gl_modern.h"
#include "brainBay.h"
#include <math.h>
#include <string.h>

// --- function pointer storage ------------------------------------------------
PFN_glCreateShader        glm_CreateShader        = NULL;
PFN_glShaderSource        glm_ShaderSource        = NULL;
PFN_glCompileShader       glm_CompileShader       = NULL;
PFN_glGetShaderiv         glm_GetShaderiv         = NULL;
PFN_glGetShaderInfoLog    glm_GetShaderInfoLog    = NULL;
PFN_glCreateProgram       glm_CreateProgram       = NULL;
PFN_glAttachShader        glm_AttachShader        = NULL;
PFN_glLinkProgram         glm_LinkProgram         = NULL;
PFN_glGetProgramiv        glm_GetProgramiv        = NULL;
PFN_glGetProgramInfoLog   glm_GetProgramInfoLog   = NULL;
PFN_glUseProgram          glm_UseProgram          = NULL;
PFN_glDeleteShader        glm_DeleteShader        = NULL;
PFN_glDeleteProgram       glm_DeleteProgram       = NULL;
PFN_glGetUniformLocation  glm_GetUniformLocation  = NULL;
PFN_glGetAttribLocation   glm_GetAttribLocation   = NULL;
PFN_glUniform1i           glm_Uniform1i           = NULL;
PFN_glUniform1f           glm_Uniform1f           = NULL;
PFN_glUniform2f           glm_Uniform2f           = NULL;
PFN_glUniform3f           glm_Uniform3f           = NULL;
PFN_glUniform4f           glm_Uniform4f           = NULL;
PFN_glUniformMatrix4fv    glm_UniformMatrix4fv    = NULL;
PFN_glGenBuffers          glm_GenBuffers          = NULL;
PFN_glDeleteBuffers       glm_DeleteBuffers       = NULL;
PFN_glBindBuffer          glm_BindBuffer          = NULL;
PFN_glBufferData          glm_BufferData          = NULL;
PFN_glBufferSubData       glm_BufferSubData       = NULL;
PFN_glGenVertexArrays     glm_GenVertexArrays     = NULL;
PFN_glDeleteVertexArrays  glm_DeleteVertexArrays  = NULL;
PFN_glBindVertexArray     glm_BindVertexArray     = NULL;
PFN_glEnableVertexAttribArray  glm_EnableVertexAttribArray  = NULL;
PFN_glDisableVertexAttribArray glm_DisableVertexAttribArray = NULL;
PFN_glVertexAttribPointer glm_VertexAttribPointer = NULL;
PFN_glVertexAttribDivisor glm_VertexAttribDivisor = NULL;
PFN_glDrawArraysInstanced glm_DrawArraysInstanced = NULL;
PFN_glActiveTexture       glm_ActiveTexture       = NULL;

static BOOL s_loaded    = FALSE;   // load attempted?
static BOOL s_available = FALSE;   // essential functions present?

static void *load_gl(const char *name)
{
	void *p = (void*)wglGetProcAddress(name);
	// wglGetProcAddress may return 1,2,3,-1 for some drivers on failure
	if (p == NULL || p == (void*)0x1 || p == (void*)0x2 ||
	    p == (void*)0x3 || p == (void*)-1)
		return NULL;
	return p;
}

BOOL ensureGladLoaded(void)
{
	if (s_loaded) return s_available;
	s_loaded = TRUE;

	glm_CreateShader       = (PFN_glCreateShader)      load_gl("glCreateShader");
	glm_ShaderSource       = (PFN_glShaderSource)      load_gl("glShaderSource");
	glm_CompileShader      = (PFN_glCompileShader)     load_gl("glCompileShader");
	glm_GetShaderiv        = (PFN_glGetShaderiv)       load_gl("glGetShaderiv");
	glm_GetShaderInfoLog   = (PFN_glGetShaderInfoLog)  load_gl("glGetShaderInfoLog");
	glm_CreateProgram      = (PFN_glCreateProgram)     load_gl("glCreateProgram");
	glm_AttachShader       = (PFN_glAttachShader)      load_gl("glAttachShader");
	glm_LinkProgram        = (PFN_glLinkProgram)       load_gl("glLinkProgram");
	glm_GetProgramiv       = (PFN_glGetProgramiv)      load_gl("glGetProgramiv");
	glm_GetProgramInfoLog  = (PFN_glGetProgramInfoLog) load_gl("glGetProgramInfoLog");
	glm_UseProgram         = (PFN_glUseProgram)        load_gl("glUseProgram");
	glm_DeleteShader       = (PFN_glDeleteShader)      load_gl("glDeleteShader");
	glm_DeleteProgram      = (PFN_glDeleteProgram)     load_gl("glDeleteProgram");
	glm_GetUniformLocation = (PFN_glGetUniformLocation)load_gl("glGetUniformLocation");
	glm_GetAttribLocation  = (PFN_glGetAttribLocation) load_gl("glGetAttribLocation");
	glm_Uniform1i          = (PFN_glUniform1i)         load_gl("glUniform1i");
	glm_Uniform1f          = (PFN_glUniform1f)         load_gl("glUniform1f");
	glm_Uniform2f          = (PFN_glUniform2f)         load_gl("glUniform2f");
	glm_Uniform3f          = (PFN_glUniform3f)         load_gl("glUniform3f");
	glm_Uniform4f          = (PFN_glUniform4f)         load_gl("glUniform4f");
	glm_UniformMatrix4fv   = (PFN_glUniformMatrix4fv)  load_gl("glUniformMatrix4fv");
	glm_GenBuffers         = (PFN_glGenBuffers)        load_gl("glGenBuffers");
	glm_DeleteBuffers      = (PFN_glDeleteBuffers)     load_gl("glDeleteBuffers");
	glm_BindBuffer         = (PFN_glBindBuffer)        load_gl("glBindBuffer");
	glm_BufferData         = (PFN_glBufferData)        load_gl("glBufferData");
	glm_BufferSubData      = (PFN_glBufferSubData)     load_gl("glBufferSubData");
	glm_GenVertexArrays    = (PFN_glGenVertexArrays)   load_gl("glGenVertexArrays");
	glm_DeleteVertexArrays = (PFN_glDeleteVertexArrays)load_gl("glDeleteVertexArrays");
	glm_BindVertexArray    = (PFN_glBindVertexArray)   load_gl("glBindVertexArray");
	glm_EnableVertexAttribArray  = (PFN_glEnableVertexAttribArray) load_gl("glEnableVertexAttribArray");
	glm_DisableVertexAttribArray = (PFN_glDisableVertexAttribArray)load_gl("glDisableVertexAttribArray");
	glm_VertexAttribPointer= (PFN_glVertexAttribPointer)load_gl("glVertexAttribPointer");
	glm_VertexAttribDivisor= (PFN_glVertexAttribDivisor)load_gl("glVertexAttribDivisor");
	glm_DrawArraysInstanced= (PFN_glDrawArraysInstanced)load_gl("glDrawArraysInstanced");
	glm_ActiveTexture      = (PFN_glActiveTexture)     load_gl("glActiveTexture");

	// essential set for shaders + VBOs
	s_available = (glm_CreateShader && glm_ShaderSource && glm_CompileShader &&
	               glm_CreateProgram && glm_AttachShader && glm_LinkProgram &&
	               glm_UseProgram && glm_GenBuffers && glm_BindBuffer &&
	               glm_BufferData && glm_VertexAttribPointer &&
	               glm_EnableVertexAttribArray && glm_GetUniformLocation) ? TRUE : FALSE;

	if (s_available) write_logfile("gl_modern: modern OpenGL functions loaded");
	else             write_logfile("gl_modern: modern OpenGL not available, falling back to fixed pipeline");

	return s_available;
}

BOOL glmModernAvailable(void)
{
	return s_available;
}

// ---------------------------------------------------------------------------
//  Matrix helpers (column-major)
// ---------------------------------------------------------------------------
glmMat4 glmIdentity(void)
{
	glmMat4 r; memset(r.m, 0, sizeof(r.m));
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
	return r;
}

glmMat4 glmOrtho(float l, float rt, float b, float t, float zn, float zf)
{
	glmMat4 r = glmIdentity();
	r.m[0]  = 2.0f/(rt-l);
	r.m[5]  = 2.0f/(t-b);
	r.m[10] = -2.0f/(zf-zn);
	r.m[12] = -(rt+l)/(rt-l);
	r.m[13] = -(t+b)/(t-b);
	r.m[14] = -(zf+zn)/(zf-zn);
	return r;
}

glmMat4 glmPerspective(float fovyDeg, float aspect, float zn, float zf)
{
	glmMat4 r; memset(r.m, 0, sizeof(r.m));
	float f = 1.0f / (float)tan(fovyDeg * 3.14159265358979f / 360.0f);
	r.m[0]  = f/aspect;
	r.m[5]  = f;
	r.m[10] = (zf+zn)/(zn-zf);
	r.m[11] = -1.0f;
	r.m[14] = (2.0f*zf*zn)/(zn-zf);
	return r;
}

glmMat4 glmMultiply(const glmMat4 a, const glmMat4 b)
{
	glmMat4 r;
	for (int c=0;c<4;c++)
		for (int rr=0;rr<4;rr++)
		{
			float s=0;
			for (int k=0;k<4;k++) s += a.m[k*4+rr] * b.m[c*4+k];
			r.m[c*4+rr]=s;
		}
	return r;
}

glmMat4 glmTranslate(float x, float y, float z)
{
	glmMat4 r = glmIdentity();
	r.m[12]=x; r.m[13]=y; r.m[14]=z;
	return r;
}

glmMat4 glmScale(float x, float y, float z)
{
	glmMat4 r = glmIdentity();
	r.m[0]=x; r.m[5]=y; r.m[10]=z;
	return r;
}

glmMat4 glmRotateX(float deg)
{
	glmMat4 r = glmIdentity();
	float a=deg*3.14159265358979f/180.0f, c=(float)cos(a), s=(float)sin(a);
	r.m[5]=c; r.m[6]=s; r.m[9]=-s; r.m[10]=c;
	return r;
}

glmMat4 glmRotateY(float deg)
{
	glmMat4 r = glmIdentity();
	float a=deg*3.14159265358979f/180.0f, c=(float)cos(a), s=(float)sin(a);
	r.m[0]=c; r.m[2]=-s; r.m[8]=s; r.m[10]=c;
	return r;
}

// ---------------------------------------------------------------------------
//  Shader helper
// ---------------------------------------------------------------------------
static GLuint compile_one(GLenum type, const char *src)
{
	GLuint sh = glm_CreateShader(type);
	glm_ShaderSource(sh, 1, &src, NULL);
	glm_CompileShader(sh);
	GLint ok = 0;
	glm_GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[1024]; GLsizei n=0;
		if (glm_GetShaderInfoLog) glm_GetShaderInfoLog(sh, sizeof(log)-1, &n, log);
		log[(n<(GLsizei)sizeof(log))?n:sizeof(log)-1]=0;
		write_logfile("gl_modern: shader compile error: %s", log);
		glm_DeleteShader(sh);
		return 0;
	}
	return sh;
}

GLuint glmBuildProgram(const char *vertexSrc, const char *fragmentSrc)
{
	if (!glmModernAvailable()) return 0;

	GLuint vs = compile_one(GL_VERTEX_SHADER, vertexSrc);
	if (!vs) return 0;
	GLuint fs = compile_one(GL_FRAGMENT_SHADER, fragmentSrc);
	if (!fs) { glm_DeleteShader(vs); return 0; }

	GLuint prog = glm_CreateProgram();
	glm_AttachShader(prog, vs);
	glm_AttachShader(prog, fs);
	glm_LinkProgram(prog);

	GLint ok=0;
	glm_GetProgramiv(prog, GL_LINK_STATUS, &ok);
	glm_DeleteShader(vs);
	glm_DeleteShader(fs);
	if (!ok)
	{
		char log[1024]; GLsizei n=0;
		if (glm_GetProgramInfoLog) glm_GetProgramInfoLog(prog, sizeof(log)-1, &n, log);
		log[(n<(GLsizei)sizeof(log))?n:sizeof(log)-1]=0;
		write_logfile("gl_modern: program link error: %s", log);
		glm_DeleteProgram(prog);
		return 0;
	}
	return prog;
}
