#include "shader_cache.h"

#include <utils/file_io.h>
#include <utils/Cache.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <oglplus/all.hpp>

#include <tuple>
#include <map>
#include <memory>

namespace vis {
	//////////////////////////////////
	// vertex/fragment/compute shader cache //
	class Vertex_Shader_Cache : public utils::Cache<std::string, std::shared_ptr<gl::VertexShader>> {
	protected:
		std::shared_ptr<gl::VertexShader> create_entry(const std::string& key) override {
			auto shader = std::make_shared<gl::VertexShader>();
			shader->Source(utils::read_file(key));
			shader->Compile();
			return shader;
		}
	} vertex_shader_cache;

	gl::VertexShader* get_cached_vertex_shader(const std::string& path) {
		return vertex_shader_cache(path).get();
	}

	class Fragment_Shader_Cache : public utils::Cache<std::string, std::shared_ptr<gl::FragmentShader>> {
	protected:
		std::shared_ptr<gl::FragmentShader> create_entry(const std::string& key) override {
			auto shader = std::make_shared<gl::FragmentShader>();
			shader->Source(utils::read_file(key));
			shader->Compile();
			return shader;
		}
	} fragment_shader_cache;

	gl::FragmentShader* get_cached_fragment_shader(const std::string& path) {
		return fragment_shader_cache(path).get();
	}

	class Compute_Shader_Cache : public utils::Cache<std::string, std::shared_ptr<gl::ComputeShader>> {
	protected:
		std::shared_ptr<gl::ComputeShader> create_entry(const std::string& key) override {
			auto shader = std::make_shared<gl::ComputeShader>();
			shader->Source(utils::read_file(key));
			shader->Compile();
			return shader;
		}
	} compute_shader_cache;

	gl::ComputeShader* get_cached_compute_shader(const std::string& path) {
		return compute_shader_cache(path).get();
	}

	///////////////////
	// program cache //
	struct Program_Cache_Key {
		std::string vertex_shader_path;
		std::string fragment_shader_path;
		std::string compute_shader_path;

		bool operator<(const Program_Cache_Key& other) const {
			return std::tie(vertex_shader_path, fragment_shader_path, compute_shader_path) < std::tie(other.vertex_shader_path, other.fragment_shader_path, compute_shader_path);
		}
	};
	class Program_Cache : public utils::Cache<Program_Cache_Key, std::shared_ptr<gl::Program>> {
	protected:
		std::shared_ptr<gl::Program> create_entry(const Program_Cache_Key& key) override {
			auto program = std::make_shared<gl::Program>();
			if(!key.vertex_shader_path.empty())
				program->AttachShader(*get_cached_vertex_shader(key.vertex_shader_path));
			if(!key.fragment_shader_path.empty())
				program->AttachShader(*get_cached_fragment_shader(key.fragment_shader_path));
			if(!key.compute_shader_path.empty())
				program->AttachShader(*get_cached_compute_shader(key.compute_shader_path));
			program->Link();
			return program;
		}
	} program_cache;

	gl::Program* get_cached_program(const std::string& vertex_shader_path, const std::string& fragment_shader_path) {
		Program_Cache_Key key;
		key.vertex_shader_path = vertex_shader_path;
		key.fragment_shader_path = fragment_shader_path;
		return program_cache(key).get();
	}

	gl::Program* get_cached_compute_program(const std::string& compute_shader_path) {
		Program_Cache_Key key;
		key.compute_shader_path = compute_shader_path;
		return program_cache(key).get();
	}

	////////////
	// common //
	void reset_shader_cache() {
		vertex_shader_cache.clear();
		fragment_shader_cache.clear();
		compute_shader_cache.clear();
		program_cache.clear();
	}
}