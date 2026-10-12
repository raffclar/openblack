$input v_texcoord0, v_texcoord1, v_lightColour, v_smallBumpFade, v_shadowCoord, v_haze, v_world

#include <bgfx_shader.sh>

#include "snow.sh"
#include "vortex_ground.sh"

#define M_PI 3.1415926535897932384626433832795

// The blocks' painted textures, a layer each: the land's colour, and the coast alpha that fades it into the sea
SAMPLER2DARRAY(s0_blockTextures, 0);
// The small bump detail's colour and alpha, 12 times across a block
SAMPLER2D(s1_smallBumpAlpha, 1);
SAMPLER2D(s2_smallBump, 2);
SAMPLER2D(s3_footprints, 3);
// The moving shadows' data (see ground_shadow.sh)
SAMPLER2D(s4_groundShadowData, 4);
#define GROUND_SHADOW_DATA s4_groundShadowData
#include "ground_shadow.sh"
// Where the trees' shadows cover the island when they are drawn moving, laid out as the footprints are
SAMPLER2D(s5_objectShadows, 5);
// The shadows baked into the land, of the buildings, rocks and other things and of the trees and big forests: what each
// texel's 4-bit colour is scaled by, over 256, laid out as the footprints are
SAMPLER2D(s14_landShadows, 14);
// What of the land's alpha the rivers' channels leave, laid out as the footprints are
SAMPLER2D(s8_landAlpha, 8);

// The noise that makes the snow's edges ragged, once across each land block of 160 units
SAMPLER2D(s12_snowNoise, 12);
// The ring an open vortex lays over the land around its hole
SAMPLER2D(s13_vortexRing, 13);
// The coarse texture pages the landscape file keeps, a layer each: the coarse land (detail level 0) draws from them
SAMPLER2DARRAY(s15_coarsePages, 15);

// The land under the snow lying on it: whiter the deeper the snow over each texel of the noise, from its own colour to
// a grey that is blue-white where it lies thickest. Where no corner of its cell has more than a little snow, none shows.
vec3 LandUnderSnow(vec3 colour, vec2 xz)
{
	vec2 cell = floor(xz * 0.025f);
	float most = max(max(SnowDepthCell(cell), SnowDepthCell(cell + vec2(1.0f, 0.0f))),
	                 max(SnowDepthCell(cell + vec2(0.0f, 1.0f)), SnowDepthCell(cell + 1.0f)));
	if (most <= 5.0f)
	{
		return colour;
	}
	vec2 texel = floor(fract(xz / 160.0f) * 256.0f);
	float noise = floor(texture2DLod(s12_snowNoise, (texel + 0.5f) / 256.0f, 0.0f).r * 255.0f + 0.5f);
	float level = clamp(floor((floor(SnowDepthAt(xz)) - noise) / 8.0f), 0.0f, 16.0f);
	if (level <= 0.0f)
	{
		return colour;
	}
	float white = 14.0f + mod(floor(noise / 4.0f) + floor(noise / 2.0f), 2.0f);
	if (level >= 16.0f)
	{
		return vec3(white, white, 15.0f) / 15.0f;
	}
	return colour * (16.0f - level) / 16.0f + vec3_splat(floor(white * level / 16.0f) / 15.0f);
}

// x: the block's layer of s0_blockTextures, z: its texture (0 full, 1 the coarse one faded in over the full one, 2
// coarse)
uniform vec4 u_block;
// The layers of the neighbouring blocks: x the block at x - 1, y at z - 1, z at z + 1, w at x + 1; -1 where there is
// none
uniform vec4 u_blockNeighbours;
// x: the layer of the block's coarse page, yz: where its square starts on it (0 to 1), w: 1 when the block is drawn from
// the page (the coarse land), 0 when its coarse texture is sampled from its own painted one
uniform vec4 u_coarsePage;
// y: how bright the land's light is, a half for the land mirrored under the sea
// z: the small bump detail's strength
// w: 1 to draw the land's textures alone, unlit and with no sea, as the temple's map is textured with
uniform vec4 u_skyAndBump;
// x: darkness of the trees' moving shadows where they fully cover a texel, 0 without them
// yz: size of a texel of s5_objectShadows, w: 1 when the land takes the baked shadows (s14_landShadows)
uniform vec4 u_objectShadows;
// zw: the block's size in units
uniform vec4 u_blockPositionAndSize;
// xy: the island's lowest corner, zw: its highest, in units
uniform vec4 u_islandExtent;

// Texels along a side of a block's texture
#define BLOCK_TEXELS 256.0f
// Units along a side of a block
#define BLOCK_UNITS 160.0f

// A texel of the block's texture with the footprints painted into it, as the game paints them: the footprints' colour
// mixed into the block's 4-bit colour by how much they cover it, each channel rounded down to 4 bits again. The block
// keeps its coast alpha. blockTexel is the texel along the block texture's axes (z, then x); islandUv is where the
// footprints have the same point.
vec4 PaintedTexelOf(float layer, vec2 blockTexel, vec2 islandUv)
{
	vec4 land = texture2DArrayLod(s0_blockTextures, vec3((blockTexel + 0.5f) / BLOCK_TEXELS, layer), 0.0f);
	vec4 footprints = texture2DLod(s3_footprints, islandUv, 0.0f);
	// The mix's exact result is a whole number of 255ths of a level: a thousandth of a level absorbs the rounding of
	// what is stored without lifting any result to the next level
	vec3 levels = floor((land.rgb * (1.0f - footprints.a) + footprints.rgb) * 15.0f + 0.001f);
	// The shadows are baked into each texel: its 4-bit colour scaled and rounded down. Where a tree's moving shadow
	// drawn this frame covers the texel, that shadow (as dark as any baked one) stands for both.
	bool moving = u_objectShadows.x > 0.0f && texture2DLod(s5_objectShadows, islandUv, 0.0f).r >= 0.5f;
	if (u_objectShadows.w > 0.5f && !moving)
	{
		float shade = floor(texture2DLod(s14_landShadows, islandUv, 0.0f).r * 255.0f + 0.5f);
		levels = floor(levels * shade / 256.0f + 0.001f);
	}
	return vec4(levels / 15.0f, land.a);
}

vec4 PaintedTexel(vec2 blockTexel, vec2 islandUv)
{
	return PaintedTexelOf(u_block.x, blockTexel, islandUv);
}

// The block's painted texture filtered across its texels: the four nearest texels, each painted, blended by distance
// as the block's texture is filtered, each block on its own. blockUv is where the fragment is on the block's texture,
// islandUv where it is on the footprints.
vec4 PaintedBlock(vec2 blockUv, vec2 islandUv)
{
	vec2 position = blockUv * BLOCK_TEXELS - 0.5f;
	vec2 first = floor(position);
	vec2 blend = position - first;
	// Island texture coordinates a texel of the block's texture moves by. The block's texture runs along z then x, the
	// island's along x then z, upside down where the island's textures are.
	vec2 unitsPerTexel = u_blockPositionAndSize.wz / BLOCK_TEXELS;
	vec2 islandPerUnit = 1.0f / (u_islandExtent.zw - u_islandExtent.xy);
	#if BGFX_SHADER_LANGUAGE_GLSL
		vec2 islandPerTexel = unitsPerTexel.yx * islandPerUnit;
	#else
		vec2 islandPerTexel = unitsPerTexel.yx * islandPerUnit * vec2(1.0f, -1.0f);
	#endif
	vec2 last = vec2_splat(BLOCK_TEXELS - 1.0f);
	vec2 low = clamp(first, vec2_splat(0.0f), last);
	vec2 high = clamp(first + 1.0f, vec2_splat(0.0f), last);
	vec4 texels[4];
	texels[0] = PaintedTexel(low, islandUv + (low - position).yx * islandPerTexel);
	texels[1] = PaintedTexel(vec2(high.x, low.y), islandUv + (vec2(high.x, low.y) - position).yx * islandPerTexel);
	texels[2] = PaintedTexel(vec2(low.x, high.y), islandUv + (vec2(low.x, high.y) - position).yx * islandPerTexel);
	texels[3] = PaintedTexel(high, islandUv + (high - position).yx * islandPerTexel);
	return mix(mix(texels[0], texels[1], blend.x), mix(texels[2], texels[3], blend.x), blend.y);
}

// Where a point of the island is on the island's textures, as the vertices give it
vec2 IslandUvAt(vec2 xz)
{
	vec2 uv = (xz - u_islandExtent.xy) / (u_islandExtent.zw - u_islandExtent.xy);
	#if !BGFX_SHADER_LANGUAGE_GLSL
		uv.y = 1.0f - uv.y;
	#endif
	return uv;
}

// A texel of a block's painted texture, its colour and alpha as whole 4-bit levels. blockOrigin is the block's lowest x
// and z, blockTexel the texel along the texture's axes (z, then x).
vec4 PaintedLevels(float layer, vec2 blockOrigin, vec2 blockTexel)
{
	vec2 xz = blockOrigin + (blockTexel.yx + 0.5f) * (u_blockPositionAndSize.wz / BLOCK_TEXELS);
	vec4 texel = PaintedTexelOf(layer, blockTexel, IslandUvAt(xz));
	return floor(texel * 15.0f + 0.5f);
}

// The texel of the block's texture a texel of its coarse texture (1 to 62) is sampled from, along one side: 62 samples
// across 256 texels, each on a multiple of four (see land_detail::CoarseSourceTexel)
float CoarseSource(float coarseTexel)
{
	float index = clamp(coarseTexel, 1.0f, 62.0f) - 1.0f;
	return floor(index * 67650.0f / 65536.0f) * 4.0f;
}

// Two blocks' texels shared across their edge, each 4-bit level halved and rounded down
vec4 SharedLevels(vec4 a, vec4 b)
{
	return floor((a + b) * 0.5f);
}

// A texel of the block's coarse texture, in 4-bit levels, coarse.x along z and coarse.y along x (0 to 63). The two
// texels at each edge are shared with the neighbouring block's: the two blocks' edge samples, averaged. Along z first,
// then along x with the neighbour's own sample.
vec4 CoarseLevels(vec2 coarse)
{
	vec2 origin = u_blockPositionAndSize.xy;
	vec2 source = vec2(CoarseSource(coarse.x), CoarseSource(coarse.y));
	vec4 levels = PaintedLevels(u_block.x, origin, source);
	if (coarse.x <= 1.0f && u_blockNeighbours.y >= 0.0f)
	{
		levels = SharedLevels(levels, PaintedLevels(u_blockNeighbours.y, origin - vec2(0.0f, BLOCK_UNITS),
		                                            vec2(CoarseSource(62.0f), source.y)));
	}
	else if (coarse.x >= 62.0f && u_blockNeighbours.z >= 0.0f)
	{
		levels = SharedLevels(levels, PaintedLevels(u_blockNeighbours.z, origin + vec2(0.0f, BLOCK_UNITS),
		                                            vec2(CoarseSource(1.0f), source.y)));
	}
	if (coarse.y <= 1.0f && u_blockNeighbours.x >= 0.0f)
	{
		levels = SharedLevels(levels, PaintedLevels(u_blockNeighbours.x, origin - vec2(BLOCK_UNITS, 0.0f),
		                                            vec2(source.x, CoarseSource(62.0f))));
	}
	else if (coarse.y >= 62.0f && u_blockNeighbours.w >= 0.0f)
	{
		levels = SharedLevels(levels, PaintedLevels(u_blockNeighbours.w, origin + vec2(BLOCK_UNITS, 0.0f),
		                                            vec2(source.x, CoarseSource(1.0f))));
	}
	return levels;
}

// The block's coarse texture, 64 texels square, filtered across its texels. It lies a texel in from its edge, 62
// texels across the block (see land_detail::CoarseTexelPosition).
vec4 CoarseBlock(vec2 blockUv)
{
	vec2 position = 256.0f / 255.0f + blockUv * 62.0f - 0.5f;
	vec2 first = clamp(floor(position), vec2_splat(0.0f), vec2_splat(62.0f));
	vec2 blend = clamp(position - first, vec2_splat(0.0f), vec2_splat(1.0f));
	vec4 low = mix(CoarseLevels(first), CoarseLevels(first + vec2(1.0f, 0.0f)), blend.x);
	vec4 high = mix(CoarseLevels(first + vec2(0.0f, 1.0f)), CoarseLevels(first + 1.0f), blend.x);
	return mix(low, high, blend.y) / 15.0f;
}

// A texel of the block's coarse page, as whole 4-bit levels: the page is held at 16 bits a texel, each channel cut to
// its top 4 bits
vec4 PageLevels(vec2 texel)
{
	vec4 colour = texture2DArrayLod(s15_coarsePages, vec3((texel + 0.5f) / 256.0f, u_coarsePage.x), 0.0f);
	return floor(floor(colour * 255.0f + 0.5f) / 16.0f);
}

// The block's coarse texture from its square of its page, filtered across the page's texels
vec4 CoarsePage(vec2 blockUv)
{
	vec2 position = u_coarsePage.yz * 256.0f + 256.0f / 255.0f + blockUv * 62.0f - 0.5f;
	vec2 first = floor(position);
	vec2 blend = position - first;
	vec4 low = mix(PageLevels(first), PageLevels(first + vec2(1.0f, 0.0f)), blend.x);
	vec4 high = mix(PageLevels(first + vec2(0.0f, 1.0f)), PageLevels(first + 1.0f), blend.x);
	return mix(low, high, blend.y) / 15.0f;
}

void main()
{
	// unpack uniforms
	float skyType = u_skyAndBump.x;
	float smallBumpMapStrength = u_skyAndBump.z;

	// The block's texture with the footprints painted into it, filtered across its texels. Beyond the texture line the
	// block's coarse texture is drawn over it, faded in across the line's band, and alone further out. Each layer is
	// blended by its alpha.
	vec4 block = vec4_splat(0.0f);
	if (u_block.z < 1.5f)
	{
		block = PaintedBlock(v_texcoord0.xy, v_texcoord1.xy);
	}
	if (u_block.z > 0.5f)
	{
		vec4 coarse = u_coarsePage.w > 0.5f ? CoarsePage(v_texcoord0.xy) : CoarseBlock(v_texcoord0.xy);
		float coarseAlpha = (u_block.z > 1.5f ? 1.0f : v_texcoord0.z) * coarse.a;
		float alpha = 1.0f - (1.0f - block.a) * (1.0f - coarseAlpha);
		vec3 colour = coarse.rgb * coarseAlpha + block.rgb * block.a * (1.0f - coarseAlpha);
		block = vec4(alpha > 0.0f ? colour / alpha : coarse.rgb, alpha);
	}
	vec4 col = vec4(block.rgb, 1.0f);

	// The trees' moving shadows, when they are asked for, darken the land under the light. Four filtered taps across a
	// texel soften their edges as the game's eight samples a texel would.
	if (u_objectShadows.x > 0.0f)
	{
		vec2 texel = u_objectShadows.yz * 0.5f;
		float objectShadow = 0.25f * (
			texture2D(s5_objectShadows, v_texcoord1.xy + vec2(-texel.x, -texel.y)).r +
			texture2D(s5_objectShadows, v_texcoord1.xy + vec2(texel.x, -texel.y)).r +
			texture2D(s5_objectShadows, v_texcoord1.xy + vec2(-texel.x, texel.y)).r +
			texture2D(s5_objectShadows, v_texcoord1.xy + vec2(texel.x, texel.y)).r);
		col.rgb = col.rgb * (1.0f - objectShadow * u_objectShadows.x);
	}

	// The snow lies over all of it, under the light
	if (u_snow.x > 0.5f)
	{
		col.rgb = LandUnderSnow(col.rgb, v_texcoord1.zw);
	}

	if (u_skyAndBump.w > 0.0f)
	{
		gl_FragColor = vec4(col.rgb, 1.0f);
		return;
	}

	// Where the vortex's hole texture is clearer than its threshold no land is drawn, not even its depth, and what was
	// drawn before the land shows through
	bool vortex = u_vortexGround.w >= 0.0f;
	vec4 ring = vec4_splat(0.0f);
	if (vortex)
	{
		vec2 vortexUv = VortexGroundUv(v_texcoord1.zw);
		if (InVortexHole(texture2D(s6_vortexHole, vortexUv)))
		{
			discard;
		}
		ring = texture2D(s13_vortexRing, vortexUv);
	}

	col.rgb = col.rgb * v_lightColour * v_haze.a;

	// The small bump detail is a second layer over the lit land, its colour unlit, blended by its alpha whatever the
	// coast alpha. The distance haze is added to both. The land is blended over the sea by its coast alpha, and writes
	// its depth even where it is clear. Both layers are drawn at once, premultiplied: what of the sea shows through is
	// what neither layer covers.
	vec2 smallBumpUv = v_texcoord0.xy * 12.0f;
	vec3 smallBump = texture2D(s2_smallBump, smallBumpUv).rgb * v_smallBumpFade.x;
	float bumpAlpha = texture2D(s1_smallBumpAlpha, smallBumpUv).r * v_smallBumpFade.y * smallBumpMapStrength;
	// A block drawn with its coarse texture has no small bump detail
	if (u_block.z > 1.5f)
	{
		bumpAlpha = 0.0f;
	}
	vec3 land = min(col.rgb + v_haze.rgb, vec3_splat(1.0f));
	vec3 detail = min(smallBump + v_haze.rgb, vec3_splat(1.0f));
	// The rivers' channels lower the coast alpha where they run, so the sea shows through as their water
	float landAlpha = min(block.a, texture2D(s8_landAlpha, v_texcoord1.xy).r);
	vec4 premultiplied = vec4(land * landAlpha * (1.0f - bumpAlpha) + detail * bumpAlpha,
	                          1.0f - (1.0f - landAlpha) * (1.0f - bumpAlpha));
	if (vortex)
	{
		// The ring, lit as the land, is blended over all of it by its alpha. The hole texture adds no colour of its own:
		// outside the hole the land and what shows through its coast alpha are drawn as on any other block.
		vec3 ringColour = min(ring.rgb * v_lightColour * v_haze.a + v_haze.rgb, vec3_splat(1.0f));
		premultiplied = vec4(ringColour * ring.a + premultiplied.rgb * (1.0f - ring.a),
		                     1.0f - (1.0f - premultiplied.a) * (1.0f - ring.a));
	}
	// The moving shadows of the creatures, the hand and the high-detail villagers are laid over all of it in black,
	// after the land is drawn
	// The coarse land, drawn from its pages, takes none
	if (u_groundShadowBlock.y > 0.0f && v_shadowCoord.w > 0.0f && u_coarsePage.w < 0.5f)
	{
		premultiplied.rgb = premultiplied.rgb * GroundShadowsOnLand(v_world, v_shadowCoord.w);
	}
	gl_FragColor = premultiplied;
}
