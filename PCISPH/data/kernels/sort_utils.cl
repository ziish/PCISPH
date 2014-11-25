#ifndef SORT_HELPER_H
#define SORT_HELPER_H

#include <data/kernels/Simulation_Params.h>
#include <data/kernels/grid_utils.cl>

__kernel void reset_cell_offsets(__constant Simulation_Params* params, __global uint* cell_offsets) {
	if(get_global_id(0) >= params->bucket_count) return;
	vstore2((uint2)(0,0), get_global_id(0), cell_offsets);
}

__kernel void initialize(__constant Simulation_Params* params, uint particle_count, __global uint* particle_keys, __global float* particle_positions, __global uint* particle_src_locations) {
	if(get_global_id(0) >= particle_count) return;

	const uint self_id = get_global_id(0);
	const float3 self_pos = vload3(self_id, particle_positions);
	
	const int3 cell_pos = get_cell_pos(self_pos, params->cell_size);
	const uint hash_key = get_hash_key(cell_pos, params->bucket_count);
	
	particle_keys[self_id] = hash_key;
	particle_src_locations[self_id] = self_id;
}

__kernel void reorder_and_insert_boundary_offsets(uint boundary_count, 
	__global uint* cell_offsets, __global uint* src_locations, __global uint* boundary_keys,
	__global float* in_positions, __global float* out_positions) {
	uint dst_loc = get_global_id(0);
	if(dst_loc >= boundary_count)
		return;

	// load src position
	uint src_loc = src_locations[dst_loc];

	// reorder all attributes
	// -> positions
	vstore3(vload3(src_loc, in_positions), dst_loc, out_positions);

	// calculate offset
	uint cur_key = boundary_keys[dst_loc];
	// -> cur_key != prev_key => cell start
	if(dst_loc == 0 || cur_key != boundary_keys[dst_loc - 1]) {
		cell_offsets[2 * cur_key + 0] = dst_loc;
	}
	// -> cur_key != next_key => cell end
	if(dst_loc == boundary_count - 1 || cur_key != boundary_keys[dst_loc + 1]) {
		cell_offsets[2 * cur_key + 1] = dst_loc + 1;
	}
}

__kernel void reorder_and_insert_fluid_offsets(uint fluid_count, 
	__global uint* cell_offsets, __global uint* src_locations, __global uint* fluid_keys,
	__global float* in_positions, __global float* in_velocities,
	__global float* out_positions, __global float* out_velocities) {
	uint dst_loc = get_global_id(0);
	if(dst_loc >= fluid_count)
		return;

	// load src position
	uint src_loc = src_locations[dst_loc];

	// reorder all attributes
	// -> positions
	vstore3(vload3(src_loc, in_positions), dst_loc, out_positions);
	// -> velocities
	vstore3(vload3(src_loc, in_velocities), dst_loc, out_velocities);

	// calculate offset
	uint cur_key = fluid_keys[dst_loc];
	// -> cur_key != prev_key => cell start
	if(dst_loc == 0 || cur_key != fluid_keys[dst_loc - 1]) {
		cell_offsets[2 * cur_key + 0] = dst_loc;
	}
	// -> cur_key != next_key => cell end
	if(dst_loc == fluid_count - 1 || cur_key != fluid_keys[dst_loc + 1]) {
		cell_offsets[2 * cur_key + 1] = dst_loc + 1;
	}
}

#endif