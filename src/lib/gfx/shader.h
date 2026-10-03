// Program GLSL: kompilacja vertex + fragment shadera z czytelnym komunikatem błędu.
#pragma once

#include "lib/gfx/gl.h"

#include <string>

namespace ow {

class ShaderProgram {
public:
	ShaderProgram() = default;
	~ShaderProgram();
	ShaderProgram(const ShaderProgram&) = delete;
	ShaderProgram& operator=(const ShaderProgram&) = delete;

	// Zwraca false i opis błędu w err.
	bool build(const char* vs, const char* fs, std::string& err);
	void use() const { gl::UseProgram(id_); }
	GLint uniform(const char* name) const { return gl::GetUniformLocation(id_, name); }
	GLuint id() const { return id_; }

private:
	GLuint id_ = 0;
};

} // namespace ow
