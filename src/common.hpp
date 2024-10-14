#pragma once

// standard libs
#include <string>
#include <tuple>
#include <vector>
#include <mach-o/dyld.h>  // for _NSGetExecutablePath
#include <limits.h>       // for PATH_MAX
#include <stdexcept>

// glfw (OpenGL)
#define NOMINMAX
#include <gl3w.h>
#include <GLFW/glfw3.h>

// The glm library provides vector and matrix operations as in GLSL
#include <glm/vec2.hpp>				// vec2
#include <glm/ext/vector_int2.hpp>  // ivec2
#include <glm/vec3.hpp>             // vec3
#include <glm/mat3x3.hpp>           // mat3
using namespace glm;

#include "tiny_ecs.hpp"

// Simple utility functions to avoid mistyping directory name
// audio_path("audio.ogg") -> data/audio/audio.ogg
// Get defintion of PROJECT_SOURCE_DIR from:
#include "../ext/project_path.hpp"

inline std::string get_executable_path() {
	char path[PATH_MAX];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0) {
		throw std::runtime_error("Executable path is too long");
	}
	return {path};
}

inline std::string data_path() {
	std::string execPath = get_executable_path();
	std::string bundlePath = execPath.substr(0, execPath.find_last_of("/"));
	return bundlePath + "/../Resources/data";
}

inline std::string shader_path(const std::string& name) {
	std::string execPath = get_executable_path();
	std::string bundlePath = execPath.substr(0, execPath.find_last_of("/"));
	return bundlePath + "/../Resources/shaders/" + name;
}
inline std::string textures_path(const std::string& name) {return data_path() + "/textures/" + std::string(name);};
inline std::string audio_path(const std::string& name) {return data_path() + "/audio/" + std::string(name);};
inline std::string mesh_path(const std::string& name) {return data_path() + "/meshes/" + std::string(name);};

const int window_width_px = 1280;
const int window_height_px = 720;

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// The 'Transform' component handles transformations passed to the Vertex shader
// (similar to the gl Immediate mode equivalent, e.g., glTranslate()...)
// We recomment making all components non-copyable by derving from ComponentNonCopyable
struct Transform {
	mat3 mat = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f}, { 0.f, 0.f, 1.f} }; // start with the identity
	void scale(vec2 scale);
	void rotate(float radians);
	void translate(vec2 offset);
};

bool gl_has_errors();
