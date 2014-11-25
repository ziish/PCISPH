#pragma once

#include "sim/Fluid.h"
#include "vis/Fluid_Buffers.h"
#include "gl_libs.h"

#include <string>

namespace scene {
	void load(const std::string& name, vis::Fluid_Buffers& buffers, sim::Fluid& fluid, gl::Buffer& out_boundary_cubes, float& out_boundary_cube_size, float& out_cam_distance);
}