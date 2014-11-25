#include "scenes.h"
#include "sim/Fluid.h"
#include "vis/Fluid_Buffers.h"
#include "vis/fluid_rendering.h"
#include "vis/shader_cache.h"
#include <gl_libs.h>

#pragma warning(push) 
#pragma warning(disable: 4996)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "utils/stb_image_write.h"
#pragma warning(pop)

#include <CL/cl.hpp>

#include <limits>
#include <vector>
#include <map>
#include <functional>
#include <iostream>

int main(int argc, char** argv) {
	std::string scene_name;
	bool recording = false;
	float recording_interval = 1.f / 60.f;
	float simulation_duration = std::numeric_limits<float>::infinity();

	// parse arguments 
	auto get_arg = [&](int i) -> std::string {
		if(i >= argc || i < 0)
			return "";
		return argv[i];
	};

	int current_arg_i = 1;
	std::map<std::string, std::function<void()>> params_mapping;
	params_mapping["-i"] = [&]() {
		scene_name = get_arg(current_arg_i++);
	};
	params_mapping["-o"] = [&]() {
		recording = true;
	};
	params_mapping["-d"] = [&]() {
		simulation_duration = 0.001f * std::stoi(get_arg(current_arg_i++));
	};

	while(current_arg_i < argc) {
		auto v = get_arg(current_arg_i++);
		if(!params_mapping.count(v)) {
			std::cout << "Invalid argument: " << v << std::endl;
			return -1;
		}
		params_mapping[v]();
	}

	if(scene_name.empty()) {
		std::cout << "-i <scene_name>" << std::endl;
		std::getchar();
		return -1;
	}


	///////////////
	// GLFW init //
	GLFWwindow* window;
	if(!glfwInit())
		return -1;

	glfwWindowHint(GLFW_SAMPLES, 8);
	window = glfwCreateWindow(1400, 1050, "PCISPH", NULL, NULL);
	auto major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
	auto minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
	if(!window) {
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	std::cout << "GL-Version: " << major << "." << minor << std::endl;

	///////////////
	// GLEW init //
	if(glewInit() != GLEW_OK) {
		glfwTerminate();
		return -1;
	}

	try {
		/////////////////
		// OpenCL init //
		cl::Device device;
		std::vector<cl::Platform> platforms;
		cl::Platform::get(&platforms);

		cl::Context cl_ctx;
		for (auto& platform : platforms) {
			std::vector<cl::Device> devices;
			platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);

			cl_context_properties props[] =
			{
#ifdef WIN32
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
				CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
				CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
#else
#error "No implementation to retrieve context information for the current platform"
#endif
				0
			};

			for (auto& current_device : devices) {
				try {
					cl::Context current_ctx({ current_device}, props, 0, 0);
					cl_ctx = current_ctx;
					device = current_device;
					break;
				}
				catch (...) {
				}
			}
		}

		if (!cl_ctx()){
			std::cout << "Couldn't create OpenCL context" << std::endl;
			glfwTerminate();
			return -1;
		}

		cl::CommandQueue cl_queue = cl::CommandQueue(cl_ctx, device, CL_QUEUE_PROFILING_ENABLE);

		////////////////
		// Simulation //
		float cam_distance = 9.0f;
		vis::init_renderer();
		gl::Context::ClearColor(0.1f, 0.1f, 0.1f, 1.f);
		gl::Context::Enable(gl::Capability::DepthTest);

		/////////////////
		// Setup fluid //
		gl::Buffer boundary_cubes;
		float boundary_cube_size = 0.f;

		vis::Fluid_Buffers fluid_buffers;
		sim::Fluid fluid(cl_ctx, device, cl_queue);
		scene::load(scene_name, fluid_buffers, fluid, boundary_cubes, boundary_cube_size, cam_distance);

		///////////////
		// Main loop //
		float simulation_time = 0.f;

		std::vector<unsigned char> record_data;
		std::vector<unsigned char> record_data_flipped;
		unsigned int record_frame_counter = 0;
		float last_recorded_frame_time = -std::numeric_limits<float>::infinity();

		float cam_angle = 0.f;
		while(!glfwWindowShouldClose(window)) {
			glfwPollEvents();

			if(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
				fluid_buffers = vis::Fluid_Buffers();
				fluid = sim::Fluid(cl_ctx, device, cl_queue);
				scene::load(scene_name, fluid_buffers, fluid, boundary_cubes, boundary_cube_size, cam_distance);

				simulation_time = 0.f;
				record_frame_counter = 0;
				last_recorded_frame_time = -std::numeric_limits<float>::infinity();
			}

			int width, height;
			glfwGetFramebufferSize(window, &width, &height);
			gl::Context::Viewport(width, height);

			auto perspective = gl::CamMatrixf::PerspectiveX(gl::Degrees(60.f), (float) width / (float) height, 1.f, 15.f);
			auto trans = perspective * gl::CamMatrixf::LookingAt(
				cam_distance * gl::Vec3f(std::sin(cam_angle), 0.2f, std::cos(cam_angle)),
				gl::Vec3f(0.f, 0.f, 0.f),
				gl::Vec3f(0.f, 1.f, 0.f));

			gl::Context::Clear().ColorBuffer().DepthBuffer();

			// simulation
			std::vector<cl::Memory> gl_buffers{
				fluid.fluid_positions,
				fluid.fluid_normals,
				fluid.fluid_densities,
				fluid.fluid_velocities
			};

			glFinish();
			cl_queue.enqueueAcquireGLObjects(&gl_buffers);
			do {
				fluid.update();
				simulation_time += fluid.get_params().delta_t;
			} while(recording && (simulation_time - last_recorded_frame_time) <= recording_interval);

			cl_queue.enqueueReleaseGLObjects(&gl_buffers);
			cl_queue.finish();

			// rendering
			vis::render_fluid_simple(trans, fluid, fluid_buffers);
			//vis::render_boundary_cubes(trans, boundary_cubes, boundary_cube_size);

			if(recording && (simulation_time - last_recorded_frame_time) >= recording_interval) {
				const auto row_pitch = 3 * width;
				record_data.resize(width * height * 3);
				record_data_flipped.resize(record_data.size());
				gl::Context::ReadPixels(0, 0, width, height, gl::PixelDataFormat::RGB, gl::PixelDataType::UnsignedByte, record_data.data());
				
				for(int y = 0; y < height; y++) {
					auto src_row = record_data.data() + y * row_pitch;
					auto dst_row = record_data_flipped.data() + (height - y - 1) * row_pitch;
					memcpy(dst_row, src_row, row_pitch);
				}
				std::string path = "out/frame_" + std::to_string(record_frame_counter) + ".png";
				stbi_write_png(path.c_str(), width, height, 3, record_data_flipped.data(), row_pitch);

				record_frame_counter++;
				last_recorded_frame_time = simulation_time;
			}
			glfwSwapBuffers(window);

			// check if we are done
			if(simulation_time >= simulation_duration)
				break;

			cam_angle += 0.0f;
		}
	}
	catch(gl::ProgramBuildError& e) {
		glfwDestroyWindow(window);
		std::cout << e.what() << std::endl;
		std::cout << e.Log() << std::endl;
		std::getchar();
	}
	catch(gl::Error& e) {
		glfwDestroyWindow(window);
		std::cout << typeid(e).name() << std::endl;
		std::cout << e.what() << std::endl;
		std::getchar();
	}
	catch(cl::Error& e) {
		glfwDestroyWindow(window);
		std::cout << e.what() << ": " << e.err() << std::endl;
		std::getchar();
	}
	catch(std::exception& e) {
		glfwDestroyWindow(window);
		std::cout << e.what() << std::endl;
		std::getchar();
	}

	vis::deinit_renderer();
	vis::reset_shader_cache();

	glfwTerminate();
	return 0;
}