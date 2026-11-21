// A creature's body, blended from its base mesh and the meshes it is pulled towards, with its bones read from a texture:
// only the model matrix is uploaded with each draw. The bones use the height map's stage, which this shader does not
// sample.
#define BGFX_CONFIG_MAX_BONES 1
#define USE_INSTANCING 1
#define USE_MORPH 1
#define USE_BONE_TEXTURE 1
#define BONE_TEXTURE_STAGE 1

#include "vs_object.sc"
