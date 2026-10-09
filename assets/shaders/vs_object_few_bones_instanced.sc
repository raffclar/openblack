// For meshes with a skeleton of their own, such as the villagers: a draw uploads every bone the shader declares, so
// these declare only as many as such a skeleton has (bone_budget::k_Few)
#define BGFX_CONFIG_MAX_BONES 32

#include "vs_object_instanced.sc"
