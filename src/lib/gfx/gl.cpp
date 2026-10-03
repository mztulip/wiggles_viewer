#include "lib/gfx/gl.h"

namespace gl {

#define OW_GL_DEFINE(type, name) type name = nullptr;
OW_GL_FUNCTIONS(OW_GL_DEFINE)
#undef OW_GL_DEFINE

const char* load(ProcLoader loader) {
#define OW_GL_LOAD(type, name) \
	name = reinterpret_cast<type>(loader("gl" #name)); \
	if (!name) return "gl" #name;
	OW_GL_FUNCTIONS(OW_GL_LOAD)
#undef OW_GL_LOAD
	return nullptr;
}

} // namespace gl
