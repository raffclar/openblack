$input v_texcoord0, v_texcoord1, v_lightColour, v_smallBumpFade, v_shadowCoord, v_haze, v_world

#include <bgfx_shader.sh>

// The land's shader reads no snow depth grid, and takes its stage for the coarse textures
#define SNOW_NO_DEPTH_GRID
#include "snow.sh"
#include "land_paint.sh"
#include "vortex_ground.sh"

#define M_PI 3.1415926535897932384626433832795

// The small bump detail's colour and alpha, 12 times across a block
SAMPLER2D(s1_smallBumpAlpha, 1);
SAMPLER2D(s2_smallBump, 2);
// The moving shadows' data (see ground_shadow.sh)
SAMPLER2D(s4_groundShadowData, 4);
#define GROUND_SHADOW_DATA s4_groundShadowData
#include "ground_shadow.sh"
// The ring an open vortex lays over the land around its hole
SAMPLER2D(s13_vortexRing, 13);
// Where the trees' shadows cover the island when they are drawn moving, a texel for each texel of the blocks
SAMPLER2D(s5_objectShadows, 5);
// The blocks' coarse textures, as their last paints made them
SAMPLER2D(s11_coarseTextures, 11);
#define COARSE_TEXTURES s11_coarseTextures
#include "coarse_textures.sh"
// The coarse texture pages the landscape file keeps, a layer each: the coarse land (detail level 0) draws from them
SAMPLER2DARRAY(s15_coarsePages, 15);

// x: the block's layer of s0_blockTextures, z: its texture (0 full, 1 the coarse one faded in over the full one, 2
// coarse), w: 1 when its coarse texture comes from its last paint (s11_coarseTextures)
uniform vec4 u_block;
// The layers of the neighbouring blocks: x the block at x - 1, y at z - 1, z at z + 1, w at x + 1; -1 where there is
// none
uniform vec4 u_blockNeighbours;
// x: the layer of the block's coarse page, yz: where its square starts on it (0 to 1), w: where the block's coarse
// texture comes from (see coarse_edges::CoarseTexture). 0: the samples of its own painted texture. 1: the page (the
// coarse land). 2: the page as the landscape file keeps it, before the first paint of the block.
uniform vec4 u_coarsePage;
// y: how bright the land's light is, a half for the land mirrored under the sea
// z: the small bump detail's strength
// w: 1 to draw the land's textures alone, unlit and with no sea, as the temple's map is textured with
uniform vec4 u_skyAndBump;

vec4 PaintedTexel(vec2 blockTexel, vec2 islandUv)
{
	return PaintedTexelOf(u_block.x, u_blockPositionAndSize.xy, blockTexel, islandUv);
}

// The block's painted texture filtered across its texels: the four nearest texels, each painted, blended by distance
// as the block's texture is filtered, each block on its own. blockUv is where the fragment is on the block's texture,
// islandUv where it is on the textures over the whole island.
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

// A texel of the block's coarse texture, in 4-bit levels, coarse.x along z and coarse.y along x (0 to 63). The two
// texels at each edge are shared with the neighbouring block's. After the block's first paint, the coarse texture is as
// the last paint of the block made it, and the paints of the blocks mix each shared edge in the order of the paints.
// Before it, the cells are the samples of the block's texture now, and the edges are the two blocks' samples averaged:
// along z first, then along x with the neighbour's own sample.
vec4 CoarseLevels(vec2 coarse)
{
	vec2 origin = u_blockPositionAndSize.xy;
	float samples = CoarseSamples();
	vec2 cell = clamp(coarse, 1.0f, samples) - 1.0f;
	vec4 levels = vec4_splat(0.0f);
	if (u_block.w > 0.5f)
	{
		levels = CoarseTextureLevels(floor(origin / BLOCK_UNITS + 0.5f), cell);
	}
	else
	{
		vec2 source = vec2(CoarseSource(coarse.x), CoarseSource(coarse.y));
		levels = PaintedLevels(u_block.x, origin, source);
		if (coarse.x <= 1.0f && u_blockNeighbours.y >= 0.0f)
		{
			levels = SharedLevels(levels, PaintedLevels(u_blockNeighbours.y, origin - vec2(0.0f, BLOCK_UNITS),
			                                            vec2(CoarseSource(samples), source.y)));
		}
		else if (coarse.x >= samples && u_blockNeighbours.z >= 0.0f)
		{
			levels = SharedLevels(levels, PaintedLevels(u_blockNeighbours.z, origin + vec2(0.0f, BLOCK_UNITS),
			                                            vec2(CoarseSource(1.0f), source.y)));
		}
		if (coarse.y <= 1.0f && u_blockNeighbours.x >= 0.0f)
		{
			levels = SharedLevels(levels, PaintedLevels(u_blockNeighbours.x, origin - vec2(BLOCK_UNITS, 0.0f),
			                                            vec2(source.x, CoarseSource(samples))));
		}
		else if (coarse.y >= samples && u_blockNeighbours.w >= 0.0f)
		{
			levels = SharedLevels(levels, PaintedLevels(u_blockNeighbours.w, origin + vec2(BLOCK_UNITS, 0.0f),
			                                            vec2(source.x, CoarseSource(1.0f))));
		}
	}
	return levels;
}

// The block's coarse texture, a quarter of its texture along each side, filtered across its texels. It lies a 255th of
// its page in from its edge (see land_detail::CoarseTexelPosition).
vec4 CoarseBlock(vec2 blockUv)
{
	vec2 position = u_landTexels.w + blockUv * u_landTexels.z - 0.5f;
	vec2 first = clamp(floor(position), vec2_splat(0.0f), vec2_splat(CoarseSamples()));
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
	vec4 levels = floor(floor(colour * 255.0f + 0.5f) / 16.0f);
	// On the coarse land, the snow shows as it does in the texel of the block's own texture that the coarse texel takes
	// its colour from. Before its first paint, the square keeps the colours of the file.
	if (u_snow.x > 0.5f && u_coarsePage.w < 1.5f)
	{
		vec2 coarse = texel - u_coarsePage.yz * FULL_TEXELS;
		vec2 source = vec2(CoarseSourceOf(coarse.x, 62.0f, 67650.0f), CoarseSourceOf(coarse.y, 62.0f, 67650.0f));
		levels.rgb = SnowOverTexel(levels.rgb, u_blockPositionAndSize.xy, source, FULL_TEXELS);
	}
	return levels;
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
		// A block drawn with its coarse texture alone finds its hole from its place on its coarse page. The ring always
		// takes the point's own place.
		vec2 holeGround = u_block.z > 1.5f
		                      ? VortexCoarseHoleGround(v_texcoord1.zw, u_blockPositionAndSize.xy, u_coarsePage.yz)
		                      : v_texcoord1.zw;
		if (InVortexHole(texture2D(s6_vortexHole, VortexGroundUv(holeGround))))
		{
			discard;
		}
		ring = texture2D(s13_vortexRing, VortexGroundUv(v_texcoord1.zw));
	}

	col.rgb = col.rgb * v_lightColour;

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
	// The coast alpha, which the rivers' channels lower where they run, so the sea shows through as their water
	float landAlpha = block.a;
	vec4 premultiplied = vec4(land * landAlpha * (1.0f - bumpAlpha) + detail * bumpAlpha,
	                          1.0f - (1.0f - landAlpha) * (1.0f - bumpAlpha));
	if (vortex)
	{
		// The ring, lit as the land, is blended over all of it by its alpha. The hole texture adds no colour of its own:
		// outside the hole the land and what shows through its coast alpha are drawn as on any other block.
		vec3 ringColour = min(ring.rgb * v_lightColour + v_haze.rgb, vec3_splat(1.0f));
		premultiplied = vec4(ringColour * ring.a + premultiplied.rgb * (1.0f - ring.a),
		                     1.0f - (1.0f - premultiplied.a) * (1.0f - ring.a));
	}
	// The moving shadows of the creatures, the hand and the high-detail villagers are laid over all of it in black,
	// after the land is drawn
	// The coarse land, drawn from its pages, takes none
	if (u_groundShadowBlock.y > 0.0f && v_shadowCoord.w > 0.0f && abs(u_coarsePage.w - 1.0f) > 0.5f)
	{
		premultiplied.rgb = premultiplied.rgb * GroundShadowsOnLand(v_world, v_shadowCoord.w);
	}
	gl_FragColor = premultiplied;
}
