#include "scenes.h"

#include <utils/file_io.h>

#include "gl_libs.h"

#include <cstdint>
#include <string>
#include <iostream>

namespace scene {
	// https://voxel.codeplex.com/wikipage?title=XRAW%20Format&referringTitle=Update
	namespace xraw {
		enum class Voxel_Type {
			BOUNDARY_VISIBLE,
			BOUNDARY_INVISIBLE,
			BLOCKER,
			FLUID,
			EMPTY
		};

#pragma pack(push, 1)
		struct Header {
			std::uint8_t magic_number[4];
			std::uint8_t channdel_data_type;
			std::uint8_t channel_count;
			std::uint8_t bits_per_channel;
			std::uint8_t bits_per_index;
			std::uint32_t vol_size[3];
			std::uint32_t palette_colors;
		};
#pragma pack(pop)

		class Data {
		public:
			Data(const std::string& path) {
				raw = utils::read_file(path);
				if(raw.empty())
					throw std::runtime_error("File not found: " + path);
			}

			const Header* header() const {
				return reinterpret_cast<const Header*>(raw.data());
			}

			Voxel_Type voxel_type(std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
				switch(voxel_value(x, y, z)) {
				case 1:
					return Voxel_Type::BOUNDARY_VISIBLE;
				case 2:
					return Voxel_Type::BOUNDARY_INVISIBLE;
				case 3:
					return Voxel_Type::BLOCKER;
				case 4:
					return Voxel_Type::FLUID;
				default:
					return Voxel_Type::EMPTY;
				}
			}

		private:
			std::uint8_t voxel_value(std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
				auto size = header()->vol_size;
				if(x >= size[0] || y >= size[1] || z >= size[2])
					return 0;

				auto idx = x + y * size[0] + z * size[0] * size[1];
				auto base_ptr = reinterpret_cast<const std::uint8_t*>(raw.data() + sizeof(Header));
				return base_ptr[idx];
			}

			std::string raw;
		};
	}

	void load_xraw_host(const std::string& path, std::uint32_t particles_per_dimension, float scaling, std::vector<float>& fluid_positions, std::vector<float>& boundary_positions, std::vector<float>& boundary_cubes) {
		xraw::Data data(path);
		
		float particle_scaling = scaling / particles_per_dimension;
		auto size = data.header()->vol_size;

		float position_offset[] = {
			-0.5f * scaling * size[0],
			-0.5f * scaling * size[2],
			-0.5f * scaling * size[1]
		};

		auto add_cube = [&](std::int32_t bb_lower[], std::int32_t bb_upper[], std::vector<float>& positions) {
			for(std::int32_t z = bb_lower[2]; z <= bb_upper[2]; z++) {
				for(std::int32_t y = bb_lower[1]; y <= bb_upper[1]; y++) {
					for(std::int32_t x = bb_lower[0]; x <= bb_upper[0]; x++) {
						positions.push_back(x * particle_scaling + position_offset[0]);
						positions.push_back(z * particle_scaling + position_offset[1]);
						positions.push_back(y * particle_scaling + position_offset[2]);
					}
				}
			}
		};

		for(std::uint32_t z = 0; z < size[2]; z++) {
			for(std::uint32_t y = 0; y < size[1]; y++) {
				for(std::uint32_t x = 0; x < size[0]; x++) {
					auto type = data.voxel_type(x, y, z);

					if(type == xraw::Voxel_Type::BOUNDARY_VISIBLE) {
						boundary_cubes.push_back(x * scaling + position_offset[0]);
						boundary_cubes.push_back(z * scaling + position_offset[1]);
						boundary_cubes.push_back(y * scaling + position_offset[2]);
					}

					std::int32_t bb_lower[] = { 
						x * particles_per_dimension, 
						y * particles_per_dimension, 
						z * particles_per_dimension 
					};
					std::int32_t bb_upper[] = { 
						bb_lower[0] + particles_per_dimension - 1,
						bb_lower[1] + particles_per_dimension - 1, 
						bb_lower[2] + particles_per_dimension - 1 
					};

					if(data.voxel_type(x + 1, y, z) == xraw::Voxel_Type::BLOCKER)
						bb_upper[0] = x * particles_per_dimension + 2;
					if(data.voxel_type(x - 1, y, z) == xraw::Voxel_Type::BLOCKER)
						bb_lower[0] = (x + 1) * particles_per_dimension - 3;

					if(data.voxel_type(x, y + 1, z) == xraw::Voxel_Type::BLOCKER)
						bb_upper[1] = y * particles_per_dimension + 2;
					if(data.voxel_type(x, y - 1, z) == xraw::Voxel_Type::BLOCKER)
						bb_lower[1] = (y + 1) * particles_per_dimension - 3;

					if(data.voxel_type(x, y, z + 1) == xraw::Voxel_Type::BLOCKER)
						bb_upper[2] = z * particles_per_dimension + 2;
					if(data.voxel_type(x, y, z - 1) == xraw::Voxel_Type::BLOCKER)
						bb_lower[2] = (z + 1) * particles_per_dimension - 3;

					switch(type) {
					case xraw::Voxel_Type::BOUNDARY_INVISIBLE:
					case xraw::Voxel_Type::BOUNDARY_VISIBLE:
						add_cube(bb_lower, bb_upper, boundary_positions);
						break;
					case xraw::Voxel_Type::FLUID:
						add_cube(bb_lower, bb_upper, fluid_positions);
						break;
					}
				}
			}
		}
	}

	template<typename T> 
	std::vector<T> non_empty_vec(const std::vector<T>& in) {
		if(in.empty())
			return {0};
		else
			return in;
	}

	void load_xraw(const std::string& path, std::uint32_t particles_per_dimension, float scaling, vis::Fluid_Buffers& buffers, sim::Fluid& fluid, gl::Buffer& out_boundary_cubes, float& out_boundary_cube_size) {
		std::vector<float> fluid_positions;
		std::vector<float> boundary_positions;
		std::vector<float> boundary_cubes;
		
		load_xraw_host(path, particles_per_dimension, scaling, fluid_positions, boundary_positions, boundary_cubes);

		fluid.set_particle_radius(0.5f * scaling / particles_per_dimension);
		fluid.set_fluid_count((unsigned int) fluid_positions.size() / 3);
		fluid.set_boundary_count((unsigned int) boundary_positions.size() / 3);

		std::vector<float> fluid_velocities(3 * fluid.get_params().fluid_count, 0.f);

		// set output boundary variables
		out_boundary_cube_size = scaling / particles_per_dimension;
		out_boundary_cubes.Bind(gl::Buffer::Target::Array);
		gl::Buffer::Data(gl::Buffer::Target::Array, non_empty_vec(boundary_cubes));

		// load into gl buffers
		buffers.fluid_positions.Bind(gl::Buffer::Target::Array);
		gl::Buffer::Data(gl::Buffer::Target::Array, fluid_positions);

		buffers.fluid_normals.Bind(gl::Buffer::Target::Array);
		gl::Buffer::Data<float>(gl::Buffer::Target::Array, (GLsizei)fluid.get_params().fluid_count * 3, nullptr);

		buffers.fluid_densities.Bind(gl::Buffer::Target::Array);
		gl::Buffer::Data<float>(gl::Buffer::Target::Array, (GLsizei)fluid.get_params().fluid_count, nullptr);

		buffers.fluid_velocities.Bind(gl::Buffer::Target::Array);
		gl::Buffer::Data(gl::Buffer::Target::Array, fluid_velocities);
		glFinish();

		// create cl buffers
		if(boundary_positions.empty()) {
			fluid.boundary_positions = cl::Buffer(fluid.ctx, CL_MEM_READ_WRITE, 1);
			fluid.boundary_pressures = cl::Buffer(fluid.ctx, CL_MEM_READ_WRITE, 1);
		}
		else {
			fluid.boundary_positions = cl::Buffer(fluid.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, fluid.get_params().boundary_count * 3 * sizeof(float), boundary_positions.data());
			fluid.boundary_pressures = cl::Buffer(fluid.ctx, CL_MEM_READ_WRITE, fluid.get_params().boundary_count * sizeof(float));
		}

		fluid.fluid_positions = cl::BufferGL(fluid.ctx, CL_MEM_READ_WRITE, gl::GetGLName(buffers.fluid_positions));
		fluid.fluid_normals = cl::BufferGL(fluid.ctx, CL_MEM_READ_WRITE, gl::GetGLName(buffers.fluid_normals));
		fluid.fluid_predicted_positions = cl::Buffer(fluid.ctx, CL_MEM_READ_WRITE, fluid.get_params().fluid_count * sizeof(float) * 3);
		fluid.fluid_densities = cl::BufferGL(fluid.ctx, CL_MEM_READ_WRITE, gl::GetGLName(buffers.fluid_densities));
		fluid.fluid_other_forces = cl::Buffer(fluid.ctx, CL_MEM_READ_WRITE, fluid.get_params().fluid_count * sizeof(float) * 3);
		fluid.fluid_velocities = cl::BufferGL(fluid.ctx, CL_MEM_READ_WRITE, gl::GetGLName(buffers.fluid_velocities));
		fluid.fluid_pressures = cl::Buffer(fluid.ctx, CL_MEM_READ_WRITE, fluid.get_params().fluid_count * sizeof(float));
		fluid.fluid_pressure_forces = cl::Buffer(fluid.ctx, CL_MEM_READ_WRITE, fluid.get_params().fluid_count * sizeof(float) * 3);
	}

	void print_info(sim::Fluid& fluid) {
		std::cout << "Scene loaded!" << std::endl;
		std::cout << "-> Fluid-Particles: " << fluid.get_params().fluid_count << std::endl;
		std::cout << "-> Boundary-Particles: " << fluid.get_params().boundary_count << std::endl;
	}

	void load(const std::string& name, vis::Fluid_Buffers& buffers, sim::Fluid& fluid, gl::Buffer& out_boundary_cubes, float& out_boundary_cube_size, float& out_cam_distance) {
		// set default values
		fluid.set_delta_t(0.002f);
		fluid.set_rest_density(999.972f);
		fluid.set_viscosity(0.00008f);
		fluid.set_surface_tension(1.0f);
		fluid.set_gravity(-9.81f);
		fluid.set_density_variation_threshold(0.01f);
		float scaling = 0.7f;
		out_cam_distance = 9.f;

		// custom scene settings
		std::uint32_t particles_per_dimension = 3;
		if(name == "simple_drop") {
			particles_per_dimension = 30;
			fluid.set_gravity(0.f);
			out_cam_distance = 2.f;
		}
		if(name == "cube_splash") {
			particles_per_dimension = 19;
		}
		else if(name == "dambreak") {
			particles_per_dimension = 18;
		}

		// load
		load_xraw("data/scenes/" + name + ".xraw", particles_per_dimension, scaling, buffers, fluid, out_boundary_cubes, out_boundary_cube_size);
		print_info(fluid);
	}
}