#ifndef VORTEX_GROUND_SH
#define VORTEX_GROUND_SH

// An open vortex's marks on the land block under its middle: the texture whose alpha opens its hole in the land (its
// colour is added under the land), and the ring laid over the land around it
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

// Whether the hole's texture is clearer there than its threshold, where no land is drawn
bool InVortexHole(vec4 hole)
{
	return floor(hole.a * 255.0f + 0.5f) < u_vortexGround.w;
}

// The hole texture's colour lit as the land: the land's light and its haze
vec3 VortexHoleColour(vec4 hole, vec3 lightColour, vec4 haze)
{
	return min(hole.rgb * lightColour * haze.a + haze.rgb, vec3_splat(1.0f));
}

#endif // VORTEX_GROUND_SH
