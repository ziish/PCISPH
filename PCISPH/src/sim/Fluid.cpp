#include "Fluid.h"

#include <utils/file_io.h>
#include <utils/constants.h>

#pragma warning(push, 0) 
#include <clogs/clogs.h>
#pragma warning(pop)
#include <stdexcept>
#include <vector>
#include <limits>
#include <cstdint>
#include <algorithm>
#include <iostream>

namespace sim {
	cl::NDRange make_NDRange(std::uint32_t actual, std::uint32_t local_size) {
		auto remainder = actual % local_size;
		std::uint32_t result;
		if (remainder > 0)
			result = (actual / local_size + 1) * local_size;
		else
			result = actual;
		return result;
	}

	float duration_in_ms(cl::Event& e) {
		return (e.getProfilingInfo<CL_PROFILING_COMMAND_END>() - e.getProfilingInfo<CL_PROFILING_COMMAND_START>()) / 1000000.f;
	}


	Fluid::Fluid(cl::Context ctx, cl::Device device, cl::CommandQueue queue) {
		this->ctx = ctx;
		this->device = device;
		this->queue = queue;
		params_changed = true;
		boundary_updated = true;

		// compile 
		std::string build_params = "-I ./ -DOPENCL_COMPILING";
		auto create_program = [&](const std::string& path) {
			auto source = utils::read_file(path);
			return cl::Program(ctx, { std::make_pair(source.c_str(), source.size()) });
		};
		
		// -> sort utils
		try {
			sort_utils_prog = create_program("data/kernels/sort_utils.cl");
			sort_utils_prog.build({ device }, build_params.c_str());
			sort_utils_reset_cell_offsets = cl::Kernel(sort_utils_prog, "reset_cell_offsets");
			sort_utils_initialize = cl::Kernel(sort_utils_prog, "initialize");
			sort_utils_reorder_and_insert_boundary_offsets = cl::Kernel(sort_utils_prog, "reorder_and_insert_boundary_offsets");
			sort_utils_reorder_and_insert_fluid_offsets = cl::Kernel(sort_utils_prog, "reorder_and_insert_fluid_offsets");
		}
		catch(cl::Error&) {
			std::cout << "sort_utils_prog program failed to build" << std::endl;
			std::cout << sort_utils_prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(this->device) << std::endl;
			std::getchar();
			std::rethrow_exception(std::current_exception());
		}

		// -> pcisph
		try {
			pcisph_prog = create_program("data/kernels/pcisph.cl");
			pcisph_prog.build({ device }, build_params.c_str());

			pcisph_update_density = cl::Kernel(pcisph_prog, "update_density");
			pcisph_update_normal = cl::Kernel(pcisph_prog, "update_normal");
			pcisph_boundary_pressure_initialization = cl::Kernel(pcisph_prog, "boundary_pressure_initialization");
			pcisph_force_initialization = cl::Kernel(pcisph_prog, "force_initialization");
			pcisph_update_position_and_velocity = cl::Kernel(pcisph_prog, "update_position_and_velocity");
			pcisph_initialize_boundary_boundary_pred_densities = cl::Kernel(pcisph_prog, "initialize_boundary_boundary_pred_densities");
			pcisph_update_pressure = cl::Kernel(pcisph_prog, "update_pressure");
			pcisph_update_pressure_force = cl::Kernel(pcisph_prog, "update_pressure_force");
		}
		catch(cl::Error&) {
			std::cout << "PCISPH program failed to build" << std::endl;
			std::cout << pcisph_prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(this->device) << std::endl;
			std::getchar();
			std::rethrow_exception(std::current_exception());
		}

		// initialize radixsort
		clogs::RadixsortProblem sort_problem;
		sort_problem.setKeyType(clogs::TYPE_UINT);
		sort_problem.setValueType(clogs::TYPE_UINT);
		radixsort.reset(new clogs::Radixsort(ctx, device, sort_problem));
	}

	void Fluid::checkBuffersConsistent() const {
		auto expected_fluid_count = params.fluid_count;

		auto check = [&](std::size_t size, std::size_t multiplier, const std::string& name) {
			if(size != expected_fluid_count * multiplier)
				throw std::runtime_error("Inconsistent " + name + " (" + std::to_string(size) + ") for " + std::to_string(expected_fluid_count) + " particles");
		};

		check(fluid_positions.getInfo<CL_MEM_SIZE>(), 3 * sizeof(cl_float), "positions_size");
		check(fluid_normals.getInfo<CL_MEM_SIZE>(), 3 * sizeof(cl_float), "normals_size");
		check(fluid_predicted_positions.getInfo<CL_MEM_SIZE>(), 3 * sizeof(cl_float), "predicted_positions_size");
		check(fluid_densities.getInfo<CL_MEM_SIZE>(), sizeof(cl_float), "densities_size");
		check(fluid_other_forces.getInfo<CL_MEM_SIZE>(), 3 * sizeof(cl_float), "other_forces_size");
		check(fluid_velocities.getInfo<CL_MEM_SIZE>(), 3 * sizeof(cl_float), "velocities_size");
		check(fluid_pressures.getInfo<CL_MEM_SIZE>(), sizeof(cl_float), "pressures_size");
		check(fluid_pressure_forces.getInfo<CL_MEM_SIZE>(), 3 * sizeof(cl_float), "pressure_forces_size");
	}
	
	void Fluid::update() {
		if(params_changed) {
			update_deduced_attributes();
			params_changed = false;
		}

		checkBuffersConsistent();
		
		if (params.fluid_count == 0)
			return;
		const std::uint32_t local_group_size = 64;

		cl::Buffer params_buffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(Simulation_Params), &params);
		std::vector<float> density_variations(params.fluid_count);

		cl::Event first_event, last_event;

		auto sort_bit_count = [](unsigned int elements) {
			unsigned int max_value = 0;
			for(int i = 0; i < 32; i++) {
				max_value = (max_value << 1) | 1;
				if(max_value >= elements - 1)
					return i + 1;
			}
			throw std::runtime_error("sort_bit_count failed");
		};

		if(boundary_updated && params.boundary_count > 0) {
			/////////////////////////////
			// sort boundary particles //

			// -> reset offsets
			sort_utils_reset_cell_offsets.setArg(0, params_buffer);
			sort_utils_reset_cell_offsets.setArg(1, boundary_cell_offsets);
			queue.enqueueNDRangeKernel(sort_utils_reset_cell_offsets, cl::NDRange(0), make_NDRange(params.bucket_count, local_group_size), local_group_size, 0, nullptr);

			// -> initialize
			sort_utils_initialize.setArg(0, params_buffer);
			sort_utils_initialize.setArg(1, params.boundary_count);
			sort_utils_initialize.setArg(2, boundary_keys);
			sort_utils_initialize.setArg(3, boundary_positions);
			sort_utils_initialize.setArg(4, boundary_src_locations);
			queue.enqueueNDRangeKernel(sort_utils_initialize, cl::NDRange(0), make_NDRange(params.boundary_count, local_group_size), local_group_size, 0, 0);

			// -> sort
			radixsort->enqueue(queue, boundary_keys, boundary_src_locations, params.boundary_count, sort_bit_count(params.boundary_count));

			// -> reorder
			queue.enqueueCopyBuffer(boundary_positions, boundary_positions_tmp, 0, 0, 3 * params.boundary_count * sizeof(cl_float));

			sort_utils_reorder_and_insert_boundary_offsets.setArg(0, (cl_uint)params.boundary_count);
			sort_utils_reorder_and_insert_boundary_offsets.setArg(1, boundary_cell_offsets);
			sort_utils_reorder_and_insert_boundary_offsets.setArg(2, boundary_src_locations);
			sort_utils_reorder_and_insert_boundary_offsets.setArg(3, boundary_keys);
			sort_utils_reorder_and_insert_boundary_offsets.setArg(4, boundary_positions_tmp);
			sort_utils_reorder_and_insert_boundary_offsets.setArg(5, boundary_positions);
			queue.enqueueNDRangeKernel(sort_utils_reorder_and_insert_boundary_offsets, cl::NDRange(0), make_NDRange(params.boundary_count, local_group_size), local_group_size);

			/////////////////////////////////////////////////////////////
			// initialize predicted densities for boundary VS boundary //
			pcisph_initialize_boundary_boundary_pred_densities.setArg(0, params_buffer);
			pcisph_initialize_boundary_boundary_pred_densities.setArg(1, boundary_cell_offsets);
			pcisph_initialize_boundary_boundary_pred_densities.setArg(2, boundary_positions);
			pcisph_initialize_boundary_boundary_pred_densities.setArg(3, boundary_init_pred_densities);

			boundary_updated = false;
		}

		//////////////////////////
		// sort fluid particles //

		// -> reset offsets
		sort_utils_reset_cell_offsets.setArg(0, params_buffer);
		sort_utils_reset_cell_offsets.setArg(1, fluid_cell_offsets);
		queue.enqueueNDRangeKernel(sort_utils_reset_cell_offsets, cl::NDRange(0), make_NDRange(params.bucket_count, local_group_size), local_group_size, 0, &first_event);

		// -> initialize
		sort_utils_initialize.setArg(0, params_buffer);
		sort_utils_initialize.setArg(1, params.fluid_count);
		sort_utils_initialize.setArg(2, fluid_keys);
		sort_utils_initialize.setArg(3, fluid_positions);
		sort_utils_initialize.setArg(4, fluid_src_locations);
		queue.enqueueNDRangeKernel(sort_utils_initialize, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size, 0, 0);
		
		// -> sort
		radixsort->enqueue(queue, fluid_keys, fluid_src_locations, params.fluid_count, sort_bit_count(params.fluid_count));
		
		// -> reorder
		queue.enqueueCopyBuffer(fluid_positions, fluid_positions_tmp, 0, 0, 3 * params.fluid_count * sizeof(cl_float));
		queue.enqueueCopyBuffer(fluid_velocities, fluid_velocities_tmp, 0, 0, 3 * params.fluid_count * sizeof(cl_float));

		sort_utils_reorder_and_insert_fluid_offsets.setArg(0, (cl_uint)params.fluid_count);
		sort_utils_reorder_and_insert_fluid_offsets.setArg(1, fluid_cell_offsets);
		sort_utils_reorder_and_insert_fluid_offsets.setArg(2, fluid_src_locations);
		sort_utils_reorder_and_insert_fluid_offsets.setArg(3, fluid_keys);
		sort_utils_reorder_and_insert_fluid_offsets.setArg(4, fluid_positions_tmp);
		sort_utils_reorder_and_insert_fluid_offsets.setArg(5, fluid_velocities_tmp);
		sort_utils_reorder_and_insert_fluid_offsets.setArg(6, fluid_positions);
		sort_utils_reorder_and_insert_fluid_offsets.setArg(7, fluid_velocities);
		queue.enqueueNDRangeKernel(sort_utils_reorder_and_insert_fluid_offsets, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size);

		///////////////////////
		// Actual simulation //

		// calculate density
		cl::Event density_event;
		pcisph_update_density.setArg(0, params_buffer);
		pcisph_update_density.setArg(1, boundary_cell_offsets);
		pcisph_update_density.setArg(2, boundary_positions);
		pcisph_update_density.setArg(3, fluid_cell_offsets);
		pcisph_update_density.setArg(4, fluid_positions);
		pcisph_update_density.setArg(5, fluid_densities);
		queue.enqueueNDRangeKernel(pcisph_update_density, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size, 0, &density_event);

		// calculate normal
		pcisph_update_normal.setArg(0, params_buffer);
		pcisph_update_normal.setArg(1, fluid_cell_offsets);
		pcisph_update_normal.setArg(2, fluid_positions);
		pcisph_update_normal.setArg(3, fluid_densities);
		pcisph_update_normal.setArg(4, fluid_normals);
		queue.enqueueNDRangeKernel(pcisph_update_normal, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size, 0, 0);

		// initialize boundary pressure
		if(params.boundary_count > 0) {
			pcisph_boundary_pressure_initialization.setArg(0, params_buffer);
			pcisph_boundary_pressure_initialization.setArg(1, boundary_pressures);
			queue.enqueueNDRangeKernel(pcisph_boundary_pressure_initialization, cl::NDRange(0), make_NDRange(params.boundary_count, local_group_size), local_group_size, 0, 0);
		}

		// calculate viscosity/surface tension
		pcisph_force_initialization.setArg(0, params_buffer);
		pcisph_force_initialization.setArg(1, fluid_cell_offsets);
		pcisph_force_initialization.setArg(2, fluid_positions);
		pcisph_force_initialization.setArg(3, fluid_normals);
		pcisph_force_initialization.setArg(4, fluid_densities);
		pcisph_force_initialization.setArg(5, fluid_velocities);
		pcisph_force_initialization.setArg(6, fluid_other_forces);
		pcisph_force_initialization.setArg(7, fluid_pressures);
		pcisph_force_initialization.setArg(8, fluid_pressure_forces);
		queue.enqueueNDRangeKernel(pcisph_force_initialization, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size, 0, 0);
		
		// PCISPH iterations
		const int min_iterations = 2;
		for (int i = 0; i < 7; i++) {
			// -> predict position
			pcisph_update_position_and_velocity.setArg(0, params_buffer);
			pcisph_update_position_and_velocity.setArg(1, fluid_positions);
			pcisph_update_position_and_velocity.setArg(2, fluid_velocities);
			pcisph_update_position_and_velocity.setArg(3, fluid_other_forces);
			pcisph_update_position_and_velocity.setArg(4, fluid_pressure_forces);
			pcisph_update_position_and_velocity.setArg(5, fluid_predicted_positions);
			pcisph_update_position_and_velocity.setArg(6, nullptr);
			queue.enqueueNDRangeKernel(pcisph_update_position_and_velocity, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size, 0, 0);
			
			// -> predict density / predict density variation / update pressure
			pcisph_update_pressure.setArg(0, params_buffer);
			pcisph_update_pressure.setArg(1, 1);
			pcisph_update_pressure.setArg(2, boundary_cell_offsets);
			pcisph_update_pressure.setArg(3, boundary_positions);
			pcisph_update_pressure.setArg(4, boundary_init_pred_densities);
			pcisph_update_pressure.setArg(5, fluid_cell_offsets);
			pcisph_update_pressure.setArg(6, fluid_positions);
			pcisph_update_pressure.setArg(7, fluid_predicted_positions);
			pcisph_update_pressure.setArg(8, nullptr);
			pcisph_update_pressure.setArg(9, boundary_pressures);
			if(params.boundary_count > 0) {
				queue.enqueueNDRangeKernel(pcisph_update_pressure, cl::NDRange(0), make_NDRange(params.boundary_count, local_group_size), local_group_size, 0, 0);
			}

			pcisph_update_pressure.setArg(1, 0);
			pcisph_update_pressure.setArg(8, fluid_density_variations);
			pcisph_update_pressure.setArg(9, fluid_pressures);
			queue.enqueueNDRangeKernel(pcisph_update_pressure, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size, 0, 0);

			// -> read density variations
			cl::Event density_variation_read_ev;
			if(i >= min_iterations) {
				queue.enqueueReadBuffer(fluid_density_variations, CL_FALSE, 0, params.fluid_count * sizeof(float), density_variations.data(), nullptr, &density_variation_read_ev);
			}
			
			// -> compute pressure force
			pcisph_update_pressure_force.setArg(0, params_buffer);
			pcisph_update_pressure_force.setArg(1, boundary_cell_offsets);
			pcisph_update_pressure_force.setArg(2, boundary_positions);
			pcisph_update_pressure_force.setArg(3, boundary_pressures);
			pcisph_update_pressure_force.setArg(4, fluid_cell_offsets);
			pcisph_update_pressure_force.setArg(5, fluid_positions);
			pcisph_update_pressure_force.setArg(6, fluid_densities);
			pcisph_update_pressure_force.setArg(7, fluid_pressures);
			pcisph_update_pressure_force.setArg(8, fluid_pressure_forces);
			queue.enqueueNDRangeKernel(pcisph_update_pressure_force, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size, 0, 0);
			
			// -> check if we can stop (max density variation below threshold)
			if(i >= min_iterations) {
				density_variation_read_ev.wait();
				float max_density_variation = *std::max_element(density_variations.begin(), density_variations.end());
				if(max_density_variation / params.rest_density < density_variation_threshold)
					break;
			}
		}
		// time integration
		pcisph_update_position_and_velocity.setArg(0, params_buffer);
		pcisph_update_position_and_velocity.setArg(1, fluid_positions);
		pcisph_update_position_and_velocity.setArg(2, fluid_velocities);
		pcisph_update_position_and_velocity.setArg(3, fluid_other_forces);
		pcisph_update_position_and_velocity.setArg(4, fluid_pressure_forces);
		pcisph_update_position_and_velocity.setArg(5, fluid_positions);
		pcisph_update_position_and_velocity.setArg(6, fluid_velocities);
		queue.enqueueNDRangeKernel(pcisph_update_position_and_velocity, cl::NDRange(0), make_NDRange(params.fluid_count, local_group_size), local_group_size, 0, &last_event);


#if 0
		queue.finish();
		auto start = first_event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
		auto end = last_event.getProfilingInfo<CL_PROFILING_COMMAND_END>();

		auto duration = end - start;
		std::cout << duration / 1000000.f << "ms" << std::endl;
#endif
	}

	const Simulation_Params& Fluid::get_params() const {
		return params;
	}

	void Fluid::set_boundary_count(unsigned int boundary_count) {
		params.boundary_count = boundary_count;
		params_changed = true;
	}

	void Fluid::set_fluid_count(unsigned int fluid_count) {
		params.fluid_count = fluid_count;
		params_changed = true;
	}

	void Fluid::set_delta_t(float delta_t) {
		params.delta_t = delta_t;
		params_changed = true;
	}

	void Fluid::set_rest_density(float rest_density) {
		params.rest_density = rest_density;
		params_changed = true;
	}

	void Fluid::set_particle_radius(float particle_radius) {
		params.particle_radius = particle_radius;
		params_changed = true;
	}

	void Fluid::set_gravity(float gravity) {
		params.gravity = gravity;
		params_changed = true;
	}

	void Fluid::set_viscosity(float viscosity) {
		params.viscosity_constant = viscosity;
		params_changed = true;
	}

	void Fluid::set_surface_tension(float surface_tension_coefficient) {
		params.surface_tension_coefficient = surface_tension_coefficient;
		params_changed = true;
	}

	void Fluid::set_density_variation_threshold(float density_variation_threshold) {
		this->density_variation_threshold = density_variation_threshold;
	}

	void Fluid::update_deduced_attributes() {
		// only following parameters are set directly:
		// delta_t, rest_density, particle_radius, viscosity
		// all other attibutes are deduces from those

		params.kernel_radius = 4.f * params.particle_radius;
		params.kernel_radius2 = std::pow(params.kernel_radius, 2.f);
		params.particle_mass = params.rest_density / std::pow(1.f / (2.f * params.particle_radius), 3);
		
		// -> grid (bucket count needs to be a multiple of 64 to make the hash keys locally unique
		params.bucket_count = params.fluid_count / 2;
		params.bucket_count -= params.bucket_count % 64;
		params.bucket_count = std::max(64U, params.bucket_count);

		params.cell_size = params.kernel_radius;

		// -> smoothing kernels
		params.poly6_normalization = 315.f / (64.f * utils::PI * std::pow(params.kernel_radius, 9.f));
		params.poly6_d1_normalization = -945.f / (32.f * utils::PI * std::pow(params.kernel_radius, 9.f));
		params.viscosity_d2_normalization = 45.f / (utils::PI * std::pow(params.kernel_radius, 6.0f));
		params.spiky_d1_normalization = -45.f / (utils::PI * std::pow(params.kernel_radius, 6.0f));
		params.surface_tension_normalization = 32.f / (utils::PI * std::pow(params.kernel_radius, 9.f));

		// -> surface tension
		params.surface_tension_term = std::pow(params.kernel_radius, 6.f) / 64.f;

		const float particle_size = 2.f * params.particle_radius;

		// -> PCISPH density variation scaling factor
		{
			auto beta = params.delta_t * params.delta_t * params.particle_mass * params.particle_mass * 2.f / (params.rest_density * params.rest_density);
			float value_sum[] = {0.f, 0.f, 0.f};
			float value_dot_value_sum = 0.f;
			for(float z = -params.kernel_radius - particle_size; z <= params.kernel_radius + particle_size; z += 2.f * params.particle_radius) {
				for(float y = -params.kernel_radius - particle_size; y <= params.kernel_radius + particle_size; y += 2.f * params.particle_radius) {
					for(float x = -params.kernel_radius - particle_size; x <= params.kernel_radius + particle_size; x += 2.f * params.particle_radius) {
						// calculate poly6 diff1 
						auto r = std::sqrt(x * x + y * y + z * z);
						auto r2 = r * r;
						
						if(r2 < params.kernel_radius2) {
							float factor = params.poly6_d1_normalization * std::pow(params.kernel_radius2 - r2, 2);
							float value[] = {
								-factor * x,
								-factor * y,
								-factor * z,
							};

							for(int i = 0; i < 3; i++) {
								// add value
								value_sum[i] += value[i];

								// dot product of value
								value_dot_value_sum += value[i] * value[i];
							}
						}
					}
				}
			}

			// dot product of value sum
			float value_sum_dot_value_sum = 0.f;
			for(int i = 0; i < 3; i++) {
				value_sum_dot_value_sum += value_sum[i] * value_sum[i];
			}
			params.density_variation_scaling_factor = -1.f / (beta * (-value_sum_dot_value_sum - value_dot_value_sum));
		}

		// buffers
		if(params.boundary_count > 0) {
			boundary_cell_offsets = cl::Buffer(ctx, CL_MEM_READ_WRITE, std::max((std::size_t) 1, params.bucket_count * 2 * sizeof(cl_uint)));
			boundary_keys = cl::Buffer(ctx, CL_MEM_READ_WRITE, params.boundary_count * sizeof(cl_uint));
			boundary_src_locations = cl::Buffer(ctx, CL_MEM_READ_WRITE, params.boundary_count * sizeof(cl_uint));
			boundary_positions_tmp = cl::Buffer(ctx, CL_MEM_READ_WRITE, 3 * params.boundary_count * sizeof(cl_float));
			boundary_init_pred_densities = cl::Buffer(ctx, CL_MEM_READ_WRITE, params.boundary_count * sizeof(cl_float));
		}

		fluid_cell_offsets = cl::Buffer(ctx, CL_MEM_READ_WRITE, std::max((std::size_t) 1, params.bucket_count * 2 * sizeof(cl_uint)));
		fluid_keys = cl::Buffer(ctx, CL_MEM_READ_WRITE, params.fluid_count * sizeof(cl_uint));
		fluid_src_locations = cl::Buffer(ctx, CL_MEM_READ_WRITE, params.fluid_count * sizeof(cl_uint));
		fluid_positions_tmp = cl::Buffer(ctx, CL_MEM_READ_WRITE, 3 * params.fluid_count * sizeof(cl_float));
		fluid_velocities_tmp = cl::Buffer(ctx, CL_MEM_READ_WRITE, 3 * params.fluid_count * sizeof(cl_float));
		fluid_density_variations = cl::Buffer(ctx, CL_MEM_READ_WRITE, params.fluid_count * sizeof(cl_float));

		// initialize buffers
		std::vector<float> zero_data(params.fluid_count * 3, 0.f);
		queue.enqueueWriteBuffer(fluid_velocities, CL_TRUE, 0, zero_data.size() * sizeof(float), zero_data.data());

		boundary_updated = true;
	}
}