#pragma once

#include <gl_libs.h>

namespace vis {
	gl::VertexShader* get_cached_vertex_shader(const std::string& path);
	gl::FragmentShader* get_cached_fragment_shader(const std::string& path);
	gl::Program* get_cached_program(const std::string& vertex_shader_path, const std::string& fragment_shader_path);
	gl::Program* get_cached_compute_program(const std::string& compute_shader_path);

	void reset_shader_cache();
}