#ifndef VORTEX_GROUND_SH
#define VORTEX_GROUND_SH

// An open vortex's marks on the land block under its middle: the texture whose alpha opens its hole in the land (only
// its alpha is used: the game draws it into the depth alone, never its colour), and the ring laid over the land around it
SAMPLER2D(s6_vortexHole, 6);
// xy: the vortex's middle on the ground, x and z; z: one over its textures' span; w: how opaque its hole's texture must
// be, 0 to 255, for the land to be drawn, below 0 on a block without a vortex
uniform vec4 u_vortexGround;

// Where a point on the ground is on the vortex's textures: they run across with z and down with x, clamped at their
// edges
vec2 VortexGroundUv(vec2 groundXz)
{
	vec2 offset = (groundXz - u_vortexGround.xy) * u_vortexGround.z;
	return clamp(offset.yx + 0.5f, vec2_splat(0.5f / 256.0f), vec2_splat(1.0f - 0.5f / 256.0f));
}

// The point of the ground whose place on the hole's texture a block drawn with its coarse texture alone takes. The game
// finds the hole's place from the coordinates that the block's texture uses. On such a block, these are its coordinates
// on its coarse page: they start a 255th of the page in from the corner of its square and span 0.2421875 of the page.
// Thus far from the camera, the hole moves away from the vortex and becomes larger, or goes off the block (see
// vortex::HoleTexturePoint). xy of the slot: where the square starts on the page, along z and along x.
vec2 VortexCoarseHoleGround(vec2 groundXz, vec2 blockCorner, vec2 coarseSlot)
{
	vec2 fraction = (groundXz - blockCorner) / 160.0f;
	return blockCorner + (coarseSlot.yx + 1.0f / 255.0f + fraction * 0.2421875f) * 160.0f;
}

// Whether the hole's texture is clearer there than its threshold, where no land is drawn
bool InVortexHole(vec4 hole)
{
	return floor(hole.a * 255.0f + 0.5f) < u_vortexGround.w;
}

#endif // VORTEX_GROUND_SH
