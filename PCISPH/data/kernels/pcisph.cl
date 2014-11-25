#ifndef PCISPH_H
#define PCISPH_H

#include <data/kernels/Simulation_Params.h>
#include <data/kernels/grid_utils.cl>

// smoothing kernels
inline float kernel_poly6(float r2, float h2) {
	if(r2 > h2) {
		return 0.f;
	}
	else {
		const float dr = h2 - r2;
		return dr * dr * dr;
	}
}

inline float3 kernel_poly6_d1(float3 d, float h2) {
	float r2 = dot(d, d);
	if(r2 > h2) {
		return (float3)(0.f, 0.f, 0.f);
	}
	else {
		return (r2 - h2) * (r2 - h2) * d;
	}
}

inline float3 kernel_spiky_d1(float3 d, float h) {
	float r = fast_length(d);
	if(r > h || r < 0.0001f) {
		return (float3) (0.f, 0.f, 0.f);
	}
	else {
		const float dr = h - r;
		return d * dr * dr / r;
	}
}

inline float kernel_viscosity_d2(float r, float h) {
	if(r > h) {
		return 0.f;
	}
	else {
		return h - r;
	}
}

inline float kernel_surface_tension(float r, float h, float st_term) {
	if( r <= 0.5 * h ) {
		float t1 = (h - r);
		float t2 = (t1 * t1 * t1) * (r * r * r);
		return 2.f * t2 - st_term;
	}
	else if( r <= h ) {
		float t1 = (h - r);
		return (t1 * t1 * t1) * (r * r * r);
	}
	else {
		return 0.f;
	}
}
		

// OpenCL kernels
__kernel void update_density(__constant Simulation_Params* params, 
                             __global uint* boundary_cell_offsets, __global float* boundary_positions,
                             __global uint* fluid_cell_offsets, __global float* fluid_positions, __global float* fluid_densitites) {
	if(get_global_id(0) >= params->fluid_count) return;

	const uint self_id = get_global_id(0);
	const float3 self_pos = vload3(self_id, fluid_positions);
	
	float density = 0.f;

	// boundary neighbors
	FOREACH_NEIGHBOR(params, boundary_cell_offsets, self_pos, {
		float3 other_pos = vload3(other_id, boundary_positions);
		float3 diff = self_pos - other_pos;
		float r2 = dot(diff, diff);
		density += kernel_poly6(r2, params->kernel_radius2);
	});

	// fluid neighbors
	FOREACH_NEIGHBOR(params, fluid_cell_offsets, self_pos, {
		float3 other_pos = vload3(other_id, fluid_positions);
		float3 diff = self_pos - other_pos;
		float r2 = dot(diff, diff);
		density += kernel_poly6(r2, params->kernel_radius2);
	});
	density *= params->particle_mass * params->poly6_normalization;
	fluid_densitites[self_id] = density;
}

__kernel void update_normal(__constant Simulation_Params* params, 
                            __global uint* fluid_cell_offsets, __global float* fluid_positions, __global float* fluid_densitites, __global float* fluid_normals) {
	if(get_global_id(0) >= params->fluid_count) return;

	const uint self_id = get_global_id(0);
	const float3 self_pos = vload3(self_id, fluid_positions);
	
	float3 normal = (float3)(0.f, 0.f, 0.f);
	FOREACH_NEIGHBOR(params, fluid_cell_offsets, self_pos, {
		const float3 other_pos = vload3(other_id, fluid_positions);
		const float other_density = fluid_densitites[other_id];
		const float3 diff = self_pos - other_pos;
		
		normal += kernel_poly6_d1(diff, params->kernel_radius2) / other_density;
	});
	normal *= params->kernel_radius * params->particle_mass * params->poly6_d1_normalization;
	
	vstore3(normal, self_id, fluid_normals);
}

__kernel void boundary_pressure_initialization(__constant Simulation_Params* params, __global float* boundary_pressures) {
	if(get_global_id(0) >= params->boundary_count) return;
	
	const uint self_id = get_global_id(0);
	boundary_pressures[self_id] = 0.f;
}

__kernel void force_initialization(__constant Simulation_Params* params, __global uint* fluid_cell_offsets, 
                                   __global float* fluid_positions, __global float* fluid_normals, __global float* fluid_densitites, __global float* fluid_velocities, 
                                   __global float* fluid_other_forces, __global float* fluid_pressures, __global float* fluid_pressure_forces) {
	if(get_global_id(0) >= params->fluid_count) return;
	
	const uint self_id = get_global_id(0);
	const float3 self_pos = vload3(self_id, fluid_positions);
	const float3 self_vel = vload3(self_id, fluid_velocities);
	const float self_density = fluid_densitites[self_id];
	const float3 self_normal = vload3(self_id, fluid_normals);
				
	// other forces
	float3 viscosity_force = (float3) (0.f, 0.f, 0.f);
	float3 st_cohesion = (float3) (0.f, 0.f, 0.f);
	float3 st_curvature = (float3) (0.f, 0.f, 0.f);
	
	FOREACH_NEIGHBOR(params, fluid_cell_offsets, self_pos, {
		const float3 other_pos = vload3(other_id, fluid_positions);
		const float3 other_vel = vload3(other_id, fluid_velocities);
		const float other_density = fluid_densitites[other_id];
		const float3 other_normal = vload3(other_id, fluid_normals);
		float dist = distance(self_pos, other_pos);

		// -> viscosity
		if(other_density > 0.0001f) {
			viscosity_force += (other_vel - self_vel) * (kernel_viscosity_d2(dist, params->kernel_radius) / other_density);
		}
		
		if(dist > 0.0001f && dist < params->kernel_radius) {
			float st_correction_factor = 2.f * params->rest_density / (self_density + other_density);
			// -> surface tension (cohesion)
			float st_kernel =  kernel_surface_tension(dist, params->kernel_radius, params->surface_tension_term);
			float3 direction = (self_pos - other_pos) / dist;
			st_cohesion += st_correction_factor * st_kernel * direction;
			
			// -> surface tension (curvature)
			st_curvature += st_correction_factor * (self_normal - other_normal);
		}
	});
	viscosity_force *= params->viscosity_constant * params->particle_mass * params->viscosity_d2_normalization;
	
	st_cohesion *= -params->surface_tension_coefficient * params->particle_mass * params->particle_mass * params->surface_tension_normalization;
	st_curvature *= -params->surface_tension_coefficient * params->particle_mass;
	float3 surface_tension_force = (st_cohesion + st_curvature);

	// -> store: viscosity + gravity
	float3 other_forces = (float3) (0.f, params->particle_mass * params->gravity, 0.f) + viscosity_force + surface_tension_force;
	vstore3(other_forces, self_id, fluid_other_forces);

	// pressure
	fluid_pressures[self_id] = 0.f;
	vstore3((float3) (0.f, 0.f, 0.f), self_id, fluid_pressure_forces);
}

__kernel void update_position_and_velocity(__constant Simulation_Params* params, __global float* fluid_positions, __global float* fluid_velocities, 
                                           __global float* fluid_other_forces, __global float* fluid_pressure_forces,
										   __global float* fluid_new_positions, __global float* fluid_new_velocities) {
	if(get_global_id(0) >= params->fluid_count) return;
	
	const uint self_id = get_global_id(0);
	float3 self_pos = vload3(self_id, fluid_positions);
	float3 self_vel = vload3(self_id, fluid_velocities);
	float3 self_force = vload3(self_id, fluid_other_forces) + vload3(self_id, fluid_pressure_forces);
	
	float3 acceleration = self_force / params->particle_mass;

	self_vel += acceleration * params->delta_t;
	self_pos += self_vel * params->delta_t;
	
	vstore3(self_pos, self_id, fluid_new_positions);
	if(fluid_new_velocities != 0x0)
		vstore3(self_vel, self_id, fluid_new_velocities);
}

__kernel void initialize_boundary_boundary_pred_densities(__constant Simulation_Params* params,
                                                          __global uint* boundary_cell_offsets, __global float* boundary_positions, __global float* boundary_init_pred_densities) {
	if(get_global_id(0) >= params->boundary_count) return;

	const uint self_id = get_global_id(0);
	const float3 self_pos = vload3(self_id, boundary_positions);
	
	float result = 0.f;
	FOREACH_NEIGHBOR(params, boundary_cell_offsets, self_pos, {
		float3 other_pos = vload3(other_id, boundary_positions);
		float3 diff = self_pos - other_pos;
		float r2 = dot(diff, diff);
		result += kernel_poly6(r2, params->kernel_radius2);
	});
	boundary_init_pred_densities[self_id] = result;
} 

__kernel void update_pressure(__constant Simulation_Params* params, int boundary_update,
                              __global uint* boundary_cell_offsets, __global float* boundary_positions, __global float* boundary_init_pred_densities,
                              __global uint* fluid_cell_offsets,  __global float* fluid_positions, __global float* fluid_predicted_positions, __global float* fluid_density_variations, __global float* output_pressures) {
	const uint self_id = get_global_id(0);
	float3 self_pos;
	float3 self_pred_pos;

	if(boundary_update) {
		if(get_global_id(0) >= params->boundary_count) return;
		self_pos = vload3(self_id, boundary_positions);
		self_pred_pos = self_pos;
	}
	else {
		if(get_global_id(0) >= params->fluid_count) return;
		self_pos = vload3(self_id, fluid_positions);
		self_pred_pos = vload3(self_id, fluid_predicted_positions);
	}

	
	// calculate predicted density
	float pred_density = 0.f;
	// -> boundary particles
	if(boundary_update) {
		pred_density += boundary_init_pred_densities[self_id];
	}
	else {
		FOREACH_NEIGHBOR(params, boundary_cell_offsets, self_pos, {
			float3 other_pred_pos = vload3(other_id, boundary_positions);
			float3 diff = self_pred_pos - other_pred_pos;
			float r2 = dot(diff, diff);
			pred_density += kernel_poly6(r2, params->kernel_radius2);
		});
	}
	// -> fluid particles
	FOREACH_NEIGHBOR(params, fluid_cell_offsets, self_pos, {
		float3 other_pred_pos = vload3(other_id, fluid_predicted_positions);
		float3 diff = self_pred_pos - other_pred_pos;
		float r2 = dot(diff, diff);
		pred_density += kernel_poly6(r2, params->kernel_radius2);
	});
	pred_density *= params->particle_mass * params->poly6_normalization;
	
	// calculate density variation
	float density_variation = max(0.f, pred_density - params->rest_density);
	if(fluid_density_variations != 0x0)
		fluid_density_variations[self_id] = density_variation;
	if(density_variation == 0.f)
		return;

	// update pressure
	output_pressures[self_id] += density_variation * params->density_variation_scaling_factor;
}

__kernel void update_pressure_force(__constant Simulation_Params* params, 
                                    __global uint* boundary_cell_offsets, __global float* boundary_positions, __global float* boundary_pressures,
							        __global uint* fluid_cell_offsets, __global float* fluid_positions, 
                                    __global float* fluid_densities, __global float* fluid_pressures, __global float* fluid_pressure_forces) {
	if(get_global_id(0) >= params->fluid_count) return;

	const uint self_id = get_global_id(0);
	const float3 self_pos = vload3(self_id, fluid_positions);
	const float self_pressure = fluid_pressures[self_id];
	const float self_density = fluid_densities[self_id];
	const float self_factor = self_pressure / (self_density * self_density);

	float3 pressure_force = (float3) (0.f, 0.f, 0.f);
	// -> boundary particles
	FOREACH_NEIGHBOR(params, boundary_cell_offsets, self_pos, {
		float3 other_pos = vload3(other_id, boundary_positions);
		float other_pressure = boundary_pressures[other_id];
		float other_density = params->rest_density;
		float other_factor = other_pressure / (other_density * other_density);
		float factor = self_factor + other_factor;
		pressure_force += kernel_spiky_d1(self_pos - other_pos, params->kernel_radius) * factor;
	});
	// -> fluid particles
	FOREACH_NEIGHBOR(params, fluid_cell_offsets, self_pos, {
		float3 other_pos = vload3(other_id, fluid_positions);
		float other_pressure = fluid_pressures[other_id];
		float other_density = fluid_densities[other_id];
		float other_factor = other_pressure / (other_density * other_density);
		float factor = self_factor + other_factor;
		pressure_force += kernel_spiky_d1(self_pos - other_pos, params->kernel_radius) * factor;
	});

	pressure_force *= -params->spiky_d1_normalization * params->particle_mass * params->particle_mass;

	vstore3(pressure_force, self_id, fluid_pressure_forces);
}

#endif