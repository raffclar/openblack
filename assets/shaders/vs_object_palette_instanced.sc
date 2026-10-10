// For the villagers, drawn a mesh at a time with every instance's bones read from the bone palette texture: only the
// model matrix is uploaded with each draw
#define BGFX_CONFIG_MAX_BONES 1
#define USE_BONE_PALETTE 1

#include "vs_object_instanced.sc"
