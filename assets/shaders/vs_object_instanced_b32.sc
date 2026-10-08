// vs_object_instanced.sc for meshes with up to 32 bones (villagers, animals): each posed one is its own draw, and a
// smaller u_model keeps many of them inside the per-frame uniform scratch buffer (see vs_object.sc)
#define BGFX_CONFIG_MAX_BONES 32
#include "vs_object_instanced.sc"
