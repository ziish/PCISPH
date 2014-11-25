#pragma once

#ifndef __CL_ENABLE_EXCEPTIONS
#define __CL_ENABLE_EXCEPTIONS
#endif
#ifndef CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#endif

#pragma warning(push, 0) 
#include <gl/glew.h>
#include <GLFW/glfw3.h>
#include <oglplus/all.hpp>
#include <CL/cl.hpp>
#pragma warning(pop)

namespace gl = oglplus;
