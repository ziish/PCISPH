#pragma once

#include <data/kernels/Simulation_Params.h>

#include <gl_libs.h>
#include <memory>

namespace clogs {
	class Radixsort;
}

namespace sim {
	class Fluid {
	public:
		Fluid(cl::Context ctx, cl::Device device, cl::CommandQueue queue);
		void checkBuffersConsistent() const;
		void update();
		const Simulation_Params& get_params() const;

		// parameters setter
		void set_boundary_count(unsigned int boundary_count);
		void set_fluid_count(unsigned int fluid_count);
		void set_delta_t(float delta_t);
		void set_rest_density(float rest_density);
		void set_particle_radius(float particle_radius);
		void set_gravity(float gravity);
		void set_viscosity(float viscosity);
		void set_surface_tension(float surface_tension_coefficient);
		void set_density_variation_threshold(float density_variation_threshold);

		// opencl objects
		cl::Context ctx;
		cl::Device device;
		cl::CommandQueue queue;
		
		// particle state
		//	NOTE: 
		//	If additional attributes are added which are not completely recalculated every frame you have to reorder them
		//	in every update stept. To do this add them in the function Fluid::reorder_particles and the corresponding kernel
		cl::Buffer boundary_positions;
		cl::Buffer boundary_pressures;

		cl::Buffer fluid_positions;
		cl::Buffer fluid_normals;
		cl::Buffer fluid_predicted_positions;
		cl::Buffer fluid_densities;
		cl::Buffer fluid_other_forces;
		cl::Buffer fluid_velocities;
		cl::Buffer fluid_pressures;
		cl::Buffer fluid_pressure_forces;
		
	private:
		void update_deduced_attributes();
		void sort_particles_cpu(cl::Buffer& src_locations, cl::Buffer cell_offsets);
		void reorder_particles(cl::Buffer& src_locations);
		
		// settings
		bool params_changed;
		bool boundary_updated;
		float density_variation_threshold;
		Simulation_Params params;

		// programs / kernels
		cl::Program cpu_sort_helper_prog;
		cl::Kernel cpu_sort_helper_calculate_hash_keys_kernel;
		cl::Kernel cpu_sort_helper_reorder_particles_kernel;

		cl::Program radix_prog;
		cl::Kernel radix_local_sort_kernel;

		cl::Program sort_utils_prog;
		cl::Kernel sort_utils_reset_cell_offsets;
		cl::Kernel sort_utils_initialize;
		cl::Kernel sort_utils_reorder_and_insert_boundary_offsets;
		cl::Kernel sort_utils_reorder_and_insert_fluid_offsets;

		cl::Program pcisph_prog;
		cl::Kernel pcisph_update_density;
		cl::Kernel pcisph_update_normal;
		cl::Kernel pcisph_boundary_pressure_initialization;
		cl::Kernel pcisph_force_initialization;
		cl::Kernel pcisph_update_position_and_velocity;
		cl::Kernel pcisph_initialize_boundary_boundary_pred_densities;
		cl::Kernel pcisph_update_pressure;
		cl::Kernel pcisph_update_pressure_force;

		// internal buffers
		cl::Buffer boundary_cell_offsets;
		cl::Buffer boundary_keys;
		cl::Buffer boundary_src_locations;
		cl::Buffer boundary_positions_tmp;
		cl::Buffer boundary_init_pred_densities;

		cl::Buffer fluid_cell_offsets;
		cl::Buffer fluid_keys;
		cl::Buffer fluid_src_locations;
		cl::Buffer fluid_positions_tmp;
		cl::Buffer fluid_velocities_tmp;
		cl::Buffer fluid_density_variations;
		// 
		std::shared_ptr<clogs::Radixsort> radixsort;
	};
}