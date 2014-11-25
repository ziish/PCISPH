#include "fluid_rendering.h"

#include "shader_cache.h"

#pragma warning(push, 0) 
#include <oglplus/shapes/sphere.hpp>
#include <oglplus/shapes/screen.hpp>
#include <oglplus/shapes/wrapper.hpp>
#include <oglplus/dsa/framebuffer.hpp>
#include <oglplus/dsa/renderbuffer.hpp>
#include <oglplus/dsa/texture.hpp>
#pragma warning(pop)

#include <memory>

namespace vis {
	struct Data {
		// particle data
		gl::shapes::Sphere particle_mesh;
		gl::VertexArray particel_mesh_va;
		gl::Buffer particle_mesh_vertices;
		gl::Buffer particle_mesh_normals;

		// boundary data
		gl::shapes::Sphere boundary_mesh;
		gl::VertexArray boundary_mesh_va;
		gl::Buffer boundary_mesh_vertices;
		gl::Buffer boundary_mesh_normals;
	};
	std::unique_ptr<Data> data;
	// render texture stuff
	GLuint FramebufferName = 0;
	GLuint renderedTexture;
	GLuint quad_vertexbuffer;

	void init_renderer() {
		data = std::make_unique<Data>();

		//////////////
		// Particle //
		data->particle_mesh = gl::shapes::Sphere(1.0, 8, 8);

		data->particel_mesh_va.Bind();
		// vertices
		data->particle_mesh_vertices.Bind(gl::Buffer::Target::Array);
		{
			std::vector<GLfloat> vertices;
			GLuint n_per_vertex = data->particle_mesh.Positions(vertices);
			gl::Buffer::Data(gl::Buffer::Target::Array, vertices);
		}

		// normals
		data->particle_mesh_normals.Bind(gl::Buffer::Target::Array);
		{
			std::vector<GLfloat> normals;
			GLuint n_per_vertex = data->particle_mesh.Normals(normals);
			gl::Buffer::Data(gl::Buffer::Target::Array, normals);
		}
	}
	
	void deinit_renderer() {
		data.reset();
	}

	void render_fluid_simple(const gl::Mat4f& trans, const sim::Fluid& fluid, const Fluid_Buffers& buffers) {
		auto program = get_cached_program("data/shaders/simple_particle.vert", "data/shaders/simple_particle.frag");
		program->Use();

		gl::Uniform<gl::Mat4f>(*program, "trans").SetValue(trans);
		gl::Uniform<float>(*program, "particle_radius").SetValue(fluid.get_params().particle_radius * 1.f);
		gl::Uniform<float>(*program, "color_factor_min").SetValue(0.95f * fluid.get_params().rest_density);
		gl::Uniform<float>(*program, "color_factor_neutral").SetValue(fluid.get_params().rest_density);
		gl::Uniform<float>(*program, "color_factor_max").SetValue(1.05f * fluid.get_params().rest_density);

		buffers.fluid_positions.Bind(gl::Buffer::Target::Array);
		auto particle_count = buffers.fluid_positions.Size(gl::Buffer::Target::Array) / (3 * sizeof(GLfloat));

		(*program | "particle_pos")
			.Setup<GLfloat>(3)
			.Enable()
			.Divisor(1);
			
		buffers.fluid_densities.Bind(gl::Buffer::Target::Array);
		(*program | "color_factor")
			.Setup<GLfloat>(1)
			.Enable()
			.Divisor(1);

		data->particle_mesh_vertices.Bind(gl::Buffer::Target::Array);
		(*program | "pos")
			.Setup<GLfloat>(3)
			.Enable();
		
		data->particle_mesh_normals.Bind(gl::Buffer::Target::Array);
		(*program | "vert_normal")
			.Setup<GLfloat>(3)
			.Enable();


		data->particel_mesh_va.Bind();
		auto indices = data->particle_mesh.Indices();
		gl::Context::DrawElementsInstanced(gl::PrimitiveType::TriangleStrip, (GLsizei) indices.size(), indices.data(), (GLsizei)particle_count);
	}

	void render_boundary_cubes(const gl::Mat4f& trans, const gl::Buffer& boundary_cubes, float boundary_cube_size) {
		auto program = get_cached_program("data/shaders/boundary_cube.vert", "data/shaders/boundary_cube.frag");
		program->Use();
		gl::Uniform<gl::Mat4f>(*program, "trans").SetValue(trans);
		gl::Uniform<float>(*program, "cube_size").SetValue(boundary_cube_size);

		boundary_cubes.Bind(gl::Buffer::Target::Array);
		auto count = boundary_cubes.Size(gl::Buffer::Target::Array) / (3 * sizeof(GLfloat));
		(*program | "cube_pos")
			.Setup<GLfloat>(3)
			.Enable()
			.Divisor(1);

		data->boundary_mesh_vertices.Bind(gl::Buffer::Target::Array);
		(*program | "pos")
			.Setup<GLfloat>(3)
			.Enable();

		data->boundary_mesh_normals.Bind(gl::Buffer::Target::Array);
		(*program | "vert_normal")
			.Setup<GLfloat>(3)
			.Enable();
		
		data->boundary_mesh_va.Bind();
		auto indices = data->boundary_mesh.Indices();
		gl::Context::DrawElementsInstanced(gl::PrimitiveType::TriangleStrip, (GLsizei)indices.size(), indices.data(), (GLsizei)count);
	}
}