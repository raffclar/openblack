$input v_texcoord0, v_texcoord1

// EXPERIMENT (not for merge): the rays through the clouds when no thunderstorm cell is there. The shader has none of
// the cells' code, so that it runs as fast as it can.
#define CLOUD_CELLS 0
#include "clouds_march.sh"
