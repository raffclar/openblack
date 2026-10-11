#ifndef GROUND_SHADOW_SH
#define GROUND_SHADOW_SH

// The shadows of the villagers a script draws in high detail, laid on the land (src/Graphics/GroundShadow.h). Each
// shadow's coverage samples are a slot of s15_groundShadows, 4 by 2 samples a texel of its 32 by 32 texture.

#define GROUND_SHADOWS 4
#define GROUND_SHADOW_TEXELS 32
#define GROUND_SHADOW_SAMPLES_ACROSS 4
#define GROUND_SHADOW_SAMPLES_DOWN 2
#define GROUND_SHADOW_LEVELS 15.0f
#define GROUND_SHADOW_BLOCK 160.0f

SAMPLER2D(s15_groundShadows, 15);
// A point of the land to where it falls on the shadow's texture, 0 to 1, in xy, and how far it reaches along the sun's
// way across the ground in z
uniform mat4 u_groundShadowMatrix[GROUND_SHADOWS];
// x: the shadow's opacity, 0 to 255, y: how far along the sun's way the land starts taking it, z: its slot
uniform vec4 u_groundShadow[GROUND_SHADOWS];
// The extent of its silhouette across x and z: the lowest in xy, the highest in zw
uniform vec4 u_groundShadowBounds[GROUND_SHADOWS];
// x: how many shadows there are
uniform vec4 u_groundShadowInfo;

// The alpha of a texel of a shadow's texture, 0 to 1: how many of its samples are covered, as levels of a 4 bit alpha,
// scaled down by the shadow's opacity and rounded down. The ring of texels around the texture is always clear.
float GroundShadowTexel(float slot, vec2 texel, float alpha)
{
	float last = float(GROUND_SHADOW_TEXELS - 1);
	if (texel.x < 1.0f || texel.y < 1.0f || texel.x >= last || texel.y >= last)
	{
		return 0.0f;
	}
	ivec2 origin = ivec2(int(slot) * GROUND_SHADOW_TEXELS * GROUND_SHADOW_SAMPLES_ACROSS +
	                         int(texel.x) * GROUND_SHADOW_SAMPLES_ACROSS,
	                     int(texel.y) * GROUND_SHADOW_SAMPLES_DOWN);
	float covered = 0.0f;
	for (int row = 0; row < GROUND_SHADOW_SAMPLES_DOWN; ++row)
	{
		for (int column = 0; column < GROUND_SHADOW_SAMPLES_ACROSS; ++column)
		{
			covered += step(0.5f, texelFetch(s15_groundShadows, origin + ivec2(column, row), 0).r);
		}
	}
	// The level sits in the top four bits of a 16 bit texel, which is scaled by the opacity and cut back to them
	float level = alpha >= 255.0f ? covered : floor(floor(covered * 4096.0f * alpha / 255.0f) / 4096.0f);
	return level / GROUND_SHADOW_LEVELS;
}

// What of the colour one shadow leaves at a point of the land, 0 to 1. aboveWater is the land's alpha, which falls to
// 0 at its corners below altitude 2.
float GroundShadowLight(int i, vec3 world, float aboveWater)
{
	if (float(i) >= u_groundShadowInfo.x)
	{
		return 1.0f;
	}
	// The land is shaded a block at a time, only the blocks the silhouette's extent reaches
	vec4 bounds = u_groundShadowBounds[i];
	vec2 block = floor(world.xz / GROUND_SHADOW_BLOCK) * GROUND_SHADOW_BLOCK;
	if (block.x > bounds.z || bounds.x > block.x + GROUND_SHADOW_BLOCK || block.y > bounds.w ||
	    bounds.y > block.y + GROUND_SHADOW_BLOCK)
	{
		return 1.0f;
	}
	vec3 coord = mul(u_groundShadowMatrix[i], vec4(world, 1.0f)).xyz;
	vec4 shadow = u_groundShadow[i];
	if (coord.z < shadow.y || coord.x <= 0.0f || coord.x >= 1.0f || coord.y <= 0.0f || coord.y >= 1.0f)
	{
		return 1.0f;
	}
	// Filtered between the four nearest texels
	vec2 position = coord.xy * float(GROUND_SHADOW_TEXELS) - 0.5f;
	vec2 low = floor(position);
	vec2 blend = position - low;
	float a = GroundShadowTexel(shadow.z, low, shadow.x);
	float b = GroundShadowTexel(shadow.z, low + vec2(1.0f, 0.0f), shadow.x);
	float c = GroundShadowTexel(shadow.z, low + vec2(0.0f, 1.0f), shadow.x);
	float d = GroundShadowTexel(shadow.z, low + vec2(1.0f, 1.0f), shadow.x);
	float coverage = mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
	return 1.0f - coverage * aboveWater;
}

// What of the colour the high-detail villagers' shadows leave at a point of the land, 0 to 1. Each is read with a
// constant index, as the creatures' shadows are (see creature_shadow.sh).
float GroundShadowsLight(vec3 world, float aboveWater)
{
	return GroundShadowLight(0, world, aboveWater) * GroundShadowLight(1, world, aboveWater) *
	       GroundShadowLight(2, world, aboveWater) * GroundShadowLight(3, world, aboveWater);
}

#endif // GROUND_SHADOW_SH
