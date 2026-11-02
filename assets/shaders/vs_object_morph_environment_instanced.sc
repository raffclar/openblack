// A creature's body, blended from its base mesh and the meshes it is pulled towards, with the environment map added
// over every part. Each bone's axes, turned by the instance, give the coordinates of the map.
#define USE_INSTANCING 1
#define USE_MORPH 1
#define USE_ENVIRONMENT 1
#define USE_ENVIRONMENT_INSTANCE_AXES 1

#include "vs_object.sc"
