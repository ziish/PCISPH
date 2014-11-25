#ifndef GRID_UTILS_H
#define GRID_UTILS_H

// 0 = conservative with checks for duplicate cell hashs
// 1 = no checks for duplicates because the hash should be unique in local neighborhood
#define NEIGHBOR_SEARCH_METHOD 1

// hashing
inline int3 get_cell_pos(float3 pos, float cell_size) {
	return convert_int3_sat_rtn(pos / cell_size);
}
inline uint get_hash_key(int3 cell_pos, uint bucket_count) {
#if 1
	return (
			// global hash
			(((cell_pos.x * 19349663 ^ cell_pos.y * 73856093 ^ cell_pos.z * 83492791) << 6) 
			// locally unique part
			+ (cell_pos.x & 63) + 4 * (cell_pos.y & 63) + 16 * (cell_pos.z & 63)
		) & 0x7FFFFFFF) % bucket_count;
#else
	return (
			// global hash
			((cell_pos.x * 19349663U ^ cell_pos.y * 73856093U ^ cell_pos.z * 83492791U) & 0x7FFFFFC0)
			// locally unique part
			| ((cell_pos.x & 3U) + ((cell_pos.y & 3U) << 2U) + ((cell_pos.z & 3U) << 4U))
		) % bucket_count;
#endif
}

void get_cell_start_end_offset(__global uint* cell_offsets, uint hash_key, uint bucket_count, uint* out_start, uint* out_end) {
	uint2 data = vload2(hash_key, cell_offsets);
	*out_start = data.x;
	*out_end = data.y;
}

#if NEIGHBOR_SEARCH_METHOD == 0
#define FOREACH_NEIGHBOR(params, cell_offsets, pos, FOREACH_NEIGHBOR_BODY) \
{ \
	uint processed_hash_key_count = 0; \
	uint processed_hash_keys[3 * 3 * 3]; \
	int3 cell_pos = get_cell_pos(pos, params->cell_size); \
	for(int i = 0; i < 3 * 3 * 3; i++) { \
		int3 offset = { (i / 1) % 3 - 1, (i / 3) % 3 - 1, (i / 9) % 3 - 1 }; \
		int3 cur_cell_pos = cell_pos + offset; \
		uint hash_key = get_hash_key(cur_cell_pos, params->bucket_count); \
		bool skip = false; \
		for(uint j = 0; j < processed_hash_key_count; j++) { \
			if(hash_key == processed_hash_keys[j]) { \
				skip = true; \
				break; \
			} \
		} \
		if(skip) \
			continue; \
		processed_hash_keys[processed_hash_key_count++] = hash_key; \
		uint start = 0; \
		uint end = 0; \
		get_cell_start_end_offset(cell_offsets, hash_key, params->bucket_count, &start, &end); \
		for(uint other_id = start; other_id < end; other_id++) { \
			FOREACH_NEIGHBOR_BODY; \
		} \
	} \
}
#elif NEIGHBOR_SEARCH_METHOD == 1
#define FOREACH_NEIGHBOR(params, cell_offsets, pos, FOREACH_NEIGHBOR_BODY) \
{ \
	int3 cell_pos = get_cell_pos(pos, params->cell_size); \
	for(int i = 0; i < 3 * 3 * 3; i++) { \
		int3 offset = { ((i / 1) % 3) - 1, ((i / 3) % 3) - 1, ((i / 9) % 3) - 1 }; \
		int3 cur_cell_pos = cell_pos + offset; \
		uint hash_key = get_hash_key(cur_cell_pos, params->bucket_count); \
		uint start = 0; \
		uint end = 0; \
		get_cell_start_end_offset(cell_offsets, hash_key, params->bucket_count, &start, &end); \
		for(uint other_id = start; other_id < end; other_id++) { \
			FOREACH_NEIGHBOR_BODY; \
		} \
	} \
}
#endif

#endif