#ifndef Simulation_Params_H
#define Simulation_Params_H

#ifdef OPENCL_COMPILING
#define STRUCT_ATTRIBUTE_PACKED  __attribute__ ((packed))
#define OPENCL_FLOAT3 float3
#define OPENCL_UINT uint
#define OPENCL_FLOAT float
#else
#include <gl_libs.h>
#define STRUCT_ATTRIBUTE_PACKED  
#define OPENCL_FLOAT3 cl_float3
#define OPENCL_FLOAT cl_float
#define OPENCL_UINT cl_uint
#endif

#pragma pack(push, 1)
typedef struct STRUCT_ATTRIBUTE_PACKED {
	OPENCL_FLOAT particle_radius;
	OPENCL_FLOAT particle_mass;
	OPENCL_FLOAT rest_density;
	OPENCL_UINT boundary_count;
	OPENCL_UINT fluid_count;

	OPENCL_FLOAT kernel_radius;
	OPENCL_FLOAT kernel_radius2;

	OPENCL_UINT bucket_count;
	OPENCL_FLOAT cell_size;

	OPENCL_FLOAT poly6_normalization;
	OPENCL_FLOAT poly6_d1_normalization;
	OPENCL_FLOAT viscosity_d2_normalization;
	OPENCL_FLOAT spiky_d1_normalization;
	OPENCL_FLOAT surface_tension_coefficient;
	OPENCL_FLOAT surface_tension_term;
	OPENCL_FLOAT surface_tension_normalization;

	OPENCL_FLOAT viscosity_constant;
	OPENCL_FLOAT gravity;
	OPENCL_FLOAT delta_t;

	OPENCL_FLOAT density_variation_scaling_factor;
} Simulation_Params;
#pragma pack(pop)

#endif