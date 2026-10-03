#include "lib/gfx/shader.h"

#include <vector>

namespace ow {

static GLuint compile(GLenum type, const char* src, std::string& err) {
	GLuint s = gl::CreateShader(type);
	gl::ShaderSource(s, 1, &src, nullptr);
	gl::CompileShader(s);
	GLint ok = 0;
	gl::GetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		gl::GetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len + 1);
		gl::GetShaderInfoLog(s, len, nullptr, log.data());
		err = std::string(type == GL_VERTEX_SHADER ? "vertex" : "fragment") + " shader: " + log.data();
		gl::DeleteShader(s);
		return 0;
	}
	return s;
}

ShaderProgram::~ShaderProgram() {
	if (id_) gl::DeleteProgram(id_);
}

bool ShaderProgram::build(const char* vs, const char* fs, std::string& err) {
	GLuint v = compile(GL_VERTEX_SHADER, vs, err);
	if (!v) return false;
	GLuint f = compile(GL_FRAGMENT_SHADER, fs, err);
	if (!f) {
		gl::DeleteShader(v);
		return false;
	}
	GLuint p = gl::CreateProgram();
	gl::AttachShader(p, v);
	gl::AttachShader(p, f);
	gl::LinkProgram(p);
	gl::DeleteShader(v);
	gl::DeleteShader(f);
	GLint ok = 0;
	gl::GetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		gl::GetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len + 1);
		gl::GetProgramInfoLog(p, len, nullptr, log.data());
		err = std::string("link: ") + log.data();
		gl::DeleteProgram(p);
		return false;
	}
	if (id_) gl::DeleteProgram(id_);
	id_ = p;
	return true;
}

} // namespace ow
