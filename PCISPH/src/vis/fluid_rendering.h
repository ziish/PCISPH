#pragma once

#include "Fluid_Buffers.h"
#include <sim/Fluid.h>

#include <gl_libs.h>

namespace vis {
	void init_renderer();
	void deinit_renderer();
	void render_fluid_simple(const gl::Mat4f& trans, const sim::Fluid& fluid, const Fluid_Buffers& buffers);
	void render_boundary_cubes(const gl::Mat4f& trans, const gl::Buffer& boundary_cubes, float boundary_cube_size);
}