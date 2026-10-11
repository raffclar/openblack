#ifndef GROUND_SHADOW_SH
#define GROUND_SHADOW_SH

// The moving shadows laid on the land and the objects on it: the creatures', the hand's and those of the villagers a
// script draws in high detail (src/Graphics/GroundShadow.h). Before including this, GROUND_SHADOW_DATA names the
// sampler of their data, in rows of GROUND_SHADOW_DATA_WIDTH texels: GROUND_SHADOW_TEXELS_EACH texels a shadow, then
// each land block's list of the shadows reaching it.
//   0: where the land falls on the texture: u = x * r + g, v = z * b + a
//   1: the light's way across the ground (rg), how far along it the land starts taking the shadow (b)
//   2: the extent of its silhouette across x and z: the lowest (rg) and the highest (ba)
//   4 on: the levels of its 32 by 32 texture, 0 to 15, row by row, four a texel

#define GROUND_SHADOW_TEXELS 32
#define GROUND_SHADOW_TEXELS_EACH 260
#define GROUND_SHADOW_LEVELS 15.0f
#define GROUND_SHADOW_DATA_WIDTH 64

// x: how many of the shadows fall on objects too, which come first, y: how many there are
uniform vec4 u_groundShadowInfo;
// The land block being drawn: x: its first entry in the lists, y: how many shadows reach it
uniform vec4 u_groundShadowBlock;

vec4 GroundShadowData(float texel)
{
	// Worked out in floats, which hold these whole numbers exactly, as integer division is slow on some renderers
	float row = floor(texel / float(GROUND_SHADOW_DATA_WIDTH));
	return texelFetch(GROUND_SHADOW_DATA, ivec2(int(texel - row * float(GROUND_SHADOW_DATA_WIDTH)), int(row)), 0);
}

// The alpha of a texel of a shadow's texture, 0 to 1. The ring of texels around the texture is always clear.
float GroundShadowTexel(float first, vec2 texel)
{
	float last = float(GROUND_SHADOW_TEXELS - 1);
	if (texel.x < 1.0f || texel.y < 1.0f || texel.x >= last || texel.y >= last)
	{
		return 0.0f;
	}
	float quarter = floor(texel.x / 4.0f);
	vec4 levels = GroundShadowData(first + 4.0f + texel.y * float(GROUND_SHADOW_TEXELS / 4) + quarter);
	vec4 pick = step(abs(vec4(0.0f, 1.0f, 2.0f, 3.0f) - (texel.x - quarter * 4.0f)), vec4_splat(0.5f));
	return dot(levels, pick) / GROUND_SHADOW_LEVELS;
}

// How much of a shadow covers a point across the ground, 0 to 1, filtered between the four nearest texels. Outside its
// silhouette's extent (when asked to look) there is none.
float GroundShadowCoverage(float shadow, vec2 xz, bool withinExtent)
{
	float first = shadow * float(GROUND_SHADOW_TEXELS_EACH);
	vec4 bounds = GroundShadowData(first + 2.0f);
	if (withinExtent && (xz.x < bounds.x || xz.x > bounds.z || xz.y < bounds.y || xz.y > bounds.w))
	{
		return 0.0f;
	}
	vec4 receiver = GroundShadowData(first);
	vec4 shape = GroundShadowData(first + 1.0f);
	vec2 uv = vec2(xz.x * receiver.x + receiver.y, xz.y * receiver.z + receiver.w);
	if (dot(xz, shape.xy) < shape.z || uv.x <= 0.0f || uv.x >= 1.0f || uv.y <= 0.0f || uv.y >= 1.0f)
	{
		return 0.0f;
	}
	vec2 position = uv * float(GROUND_SHADOW_TEXELS) - 0.5f;
	vec2 low = floor(position);
	vec2 blend = position - low;
	float a = GroundShadowTexel(first, low);
	float b = GroundShadowTexel(first, low + vec2(1.0f, 0.0f));
	float c = GroundShadowTexel(first, low + vec2(0.0f, 1.0f));
	float d = GroundShadowTexel(first, low + vec2(1.0f, 1.0f));
	return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

// What of the colour the moving shadows leave at a point of the land block being drawn, 0 to 1: those reaching the
// block, black by their coverage, each cut by the land's alpha, which falls to 0 at its corners below altitude 2
float GroundShadowsOnLand(vec3 world, float aboveWater)
{
	float light = 1.0f;
	int count = int(u_groundShadowBlock.y);
	for (int entry = 0; entry < count; ++entry)
	{
		float shadow = GroundShadowData(u_groundShadowBlock.x + float(entry)).x;
		light *= 1.0f - GroundShadowCoverage(shadow, world.xz, false) * aboveWater;
	}
	return light;
}

// What of the colour the creatures' and the hand's shadows leave at a point of an object, 0 to 1
float GroundShadowsOnObject(vec3 world)
{
	float light = 1.0f;
	int count = int(u_groundShadowInfo.x);
	for (int shadow = 0; shadow < count; ++shadow)
	{
		light *= 1.0f - GroundShadowCoverage(float(shadow), world.xz, true);
	}
	return light;
}

#endif // GROUND_SHADOW_SH
