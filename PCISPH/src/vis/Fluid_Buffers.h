#pragma once

#include <gl_libs.h>

namespace vis {
	struct Fluid_Buffers {
		gl::Buffer fluid_positions;
		gl::Buffer fluid_normals;
		gl::Buffer fluid_densities;
		gl::Buffer fluid_velocities;
	};
}