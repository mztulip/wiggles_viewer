// Minimalny loader OpenGL 3.3 core (zamiast glad). Funkcje są w przestrzeni nazw gl::,
// żeby nie kolidowały z symbolami libGL, np. gl::GenBuffers(...).
// Ładowanie: gl::load(SDL_GL_GetProcAddress) po utworzeniu kontekstu.
#pragma once

#include <GL/glcorearb.h>

#define OW_GL_FUNCTIONS(X) \
	X(PFNGLGETSTRINGPROC, GetString) \
	X(PFNGLGETERRORPROC, GetError) \
	X(PFNGLVIEWPORTPROC, Viewport) \
	X(PFNGLCLEARPROC, Clear) \
	X(PFNGLCLEARCOLORPROC, ClearColor) \
	X(PFNGLENABLEPROC, Enable) \
	X(PFNGLDISABLEPROC, Disable) \
	X(PFNGLBLENDFUNCPROC, BlendFunc) \
	X(PFNGLDEPTHMASKPROC, DepthMask) \
	X(PFNGLCULLFACEPROC, CullFace) \
	X(PFNGLPOLYGONMODEPROC, PolygonMode) \
	X(PFNGLREADPIXELSPROC, ReadPixels) \
	X(PFNGLPIXELSTOREIPROC, PixelStorei) \
	X(PFNGLGENBUFFERSPROC, GenBuffers) \
	X(PFNGLDELETEBUFFERSPROC, DeleteBuffers) \
	X(PFNGLBINDBUFFERPROC, BindBuffer) \
	X(PFNGLBUFFERDATAPROC, BufferData) \
	X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays) \
	X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays) \
	X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray) \
	X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer) \
	X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray) \
	X(PFNGLDRAWELEMENTSPROC, DrawElements) \
	X(PFNGLDRAWARRAYSPROC, DrawArrays) \
	X(PFNGLCREATESHADERPROC, CreateShader) \
	X(PFNGLSHADERSOURCEPROC, ShaderSource) \
	X(PFNGLCOMPILESHADERPROC, CompileShader) \
	X(PFNGLGETSHADERIVPROC, GetShaderiv) \
	X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog) \
	X(PFNGLDELETESHADERPROC, DeleteShader) \
	X(PFNGLCREATEPROGRAMPROC, CreateProgram) \
	X(PFNGLATTACHSHADERPROC, AttachShader) \
	X(PFNGLLINKPROGRAMPROC, LinkProgram) \
	X(PFNGLGETPROGRAMIVPROC, GetProgramiv) \
	X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) \
	X(PFNGLDELETEPROGRAMPROC, DeleteProgram) \
	X(PFNGLUSEPROGRAMPROC, UseProgram) \
	X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) \
	X(PFNGLUNIFORM1IPROC, Uniform1i) \
	X(PFNGLUNIFORM1FPROC, Uniform1f) \
	X(PFNGLUNIFORM3FPROC, Uniform3f) \
	X(PFNGLUNIFORM3FVPROC, Uniform3fv) \
	X(PFNGLUNIFORM4FPROC, Uniform4f) \
	X(PFNGLUNIFORMMATRIX4FVPROC, UniformMatrix4fv) \
	X(PFNGLGENTEXTURESPROC, GenTextures) \
	X(PFNGLDELETETEXTURESPROC, DeleteTextures) \
	X(PFNGLBINDTEXTUREPROC, BindTexture) \
	X(PFNGLACTIVETEXTUREPROC, ActiveTexture) \
	X(PFNGLTEXIMAGE2DPROC, TexImage2D) \
	X(PFNGLTEXPARAMETERIPROC, TexParameteri) \
	X(PFNGLGENERATEMIPMAPPROC, GenerateMipmap)

namespace gl {

#define OW_GL_DECLARE(type, name) extern type name;
OW_GL_FUNCTIONS(OW_GL_DECLARE)
#undef OW_GL_DECLARE

using ProcLoader = void* (*)(const char*);

// Zwraca nazwę pierwszej brakującej funkcji albo nullptr, gdy wszystko się wczytało.
const char* load(ProcLoader loader);

} // namespace gl
