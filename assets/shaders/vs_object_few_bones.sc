// For meshes with a small skeleton of their own: a draw uploads every bone that the shader declares, so this shader
// declares only as many bones as such a skeleton has (bone_budget::k_Few)
#define BGFX_CONFIG_MAX_BONES 32

#include "vs_object.sc"
