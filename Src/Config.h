#pragma once

#include <stdint.h>


/// Custom settings
#define DRAW_SIMPLE_TRIANGLE 0
#define DRAW_SIMPLE_MESH 1

#define DRAW_MODE DRAW_SIMPLE_MESH

#define VERTEX_INPUT_MODE 1
#define FLIP_VIEWPORT 1
#define USE_MESHLETS 1
#define USE_FRAGMENT_SHADING_RATE 1
#define USE_MULTI_DRAW_INDIRECT 1
#define USE_DEVICE_8BIT_16BIT_EXTENSIONS 1
#define USE_PACKED_PRIMITIVE_INDICES_NV 0
// Mesh shader needs the 8bit_16bit_extension

namespace Niagara
{
	/// Global variables

	constexpr uint32_t WIDTH = 800;
	constexpr uint32_t HEIGHT = 600;

	constexpr uint32_t MESHLET_MAX_VERTICES = 64;
	constexpr uint32_t MESHLET_MAX_PRIMITIVES = 84;

	constexpr uint32_t MESH_MAX_LODS = 8;

	constexpr uint32_t TASK_GROUP_SIZE = 32;
	constexpr uint32_t DRAW_COUNT = 1000;
	constexpr float SCENE_RADIUS = 20.0f;
	constexpr float MAX_DRAW_DISTANCE = 20.0f;
}
