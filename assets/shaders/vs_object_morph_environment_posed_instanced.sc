// A creature's body with the environment map added over every part, as vs_object_morph_environment_instanced, with its
// bones read from a texture: only the model matrix is uploaded with each draw
#define BGFX_CONFIG_MAX_BONES 1
#define USE_INSTANCING 1
#define USE_MORPH 1
#define USE_ENVIRONMENT 1
#define USE_BONE_TEXTURE 1
#define BONE_TEXTURE_STAGE 1

#include "vs_object.sc"
