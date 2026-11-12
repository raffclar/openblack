#ifndef LAND_PAINT_SH
#define LAND_PAINT_SH

// The texels of the land blocks' textures as the game paints them: the footprints, the snow and the baked shadows, and
// the samples that a block's coarse texture takes from them. The land's shader draws them, and the pass that mixes the
// edges of the coarse textures paints them. Include snow.sh first.

// The blocks' painted textures, a layer each: the land's colour, and the coast alpha that fades it into the sea
SAMPLER2DARRAY(s0_blockTextures, 0);
// The shadows baked into the land, of the buildings, rocks and other things and of the trees and big forests: what each
// texel's 4-bit colour is scaled by, over 256, a texel for each texel of the blocks
SAMPLER2D(s14_landShadows, 14);
// The noise that makes the snow's edges ragged, once across each land block of 160 units
SAMPLER2D(s12_snowNoise, 12);
// The snow that each block's texture was last painted with: the depth at the block's 5 x 5 grid points, in a square of
// its own, 32 squares along each side, x along the texture's width and z along its height
SAMPLER2D(s3_paintedSnow, 3);

// How finely the blocks' textures are painted:
// x: the texels along a side of a block's texture (256, or 128 at the low resolution of the lowest detail levels)
// y: the step from one coarse sample to the next, in 65536ths of a texel of the block's texture
// z: the coarse texels a block covers along a side when it is drawn
// w: x divided by 255, the start of the coarse texture on its page
uniform vec4 u_landTexels;
// Texels along a side of a block's texture
#define BLOCK_TEXELS u_landTexels.x
// Texels along a side of the full texture, of a coarse page, and of the snow's noise
#define FULL_TEXELS 256.0f
// Units along a side of a block
#define BLOCK_UNITS 160.0f

// x: darkness of the trees' moving shadows where they fully cover a texel, 0 without them
// yz: size of a texel of s5_objectShadows, w: 1 when the land takes the baked shadows (s14_landShadows)
uniform vec4 u_objectShadows;
// zw: the block's size in units
uniform vec4 u_blockPositionAndSize;
// xy: the island's lowest corner, zw: its highest, in units
uniform vec4 u_islandExtent;

// The depth of the snow at a grid point of a block (0 to 4 along x and z), as the block's texture was last painted with
// it, in 65536ths, cut towards zero. A point off the grid has none. The snow lies at most 255 deep, so each whole number
// of this sum is exact in a float.
float SnowFixedDepth(vec2 block, vec2 corner)
{
	bool onGrid = all(greaterThanEqual(block, vec2_splat(0.0f))) && all(lessThan(block, vec2_splat(32.0f)));
	vec2 texel = block * 5.0f + corner;
	float depth = onGrid ? texture2DLod(s3_paintedSnow, (texel + 0.5f) / 160.0f, 0.0f).r : 0.0f;
	return floor(depth * 65536.0f);
}

// A whole number divided by another, cut towards zero
float CutBy(float value, float divisor)
{
	return SnowWhole(value / divisor);
}

// A texel's 4-bit colour levels with the snow painted over them. The game paints the snow into each texel of a block's
// texture, and then the texture is filtered. blockOrigin is the lowest x and z of the block. blockTexel is the texel
// along the texture's axes (z, then x), of a texture with blockTexels along a side.
vec3 SnowOverTexel(vec3 levels, vec2 blockOrigin, vec2 blockTexel, float blockTexels)
{
	// A side of a block covers four cells of the grid, a quarter of its texels each. Along x first, then along z.
	float cellTexels = blockTexels * 0.25f;
	vec2 texel = blockTexel.yx;
	vec2 inCell = floor(texel / cellTexels);
	vec2 block = floor(blockOrigin / BLOCK_UNITS + 0.5f);
	vec2 step = texel - inCell * cellTexels;
	float low = SnowFixedDepth(block, inCell);
	float high = SnowFixedDepth(block, inCell + vec2(1.0f, 0.0f));
	float lowFar = SnowFixedDepth(block, inCell + vec2(0.0f, 1.0f));
	float highFar = SnowFixedDepth(block, inCell + 1.0f);
	// The cell takes no snow when no corner has more than 5
	if (max(max(low, high), max(lowFar, highFar)) <= 327680.0f)
	{
		return levels;
	}
	// Each step adds the same whole part of the difference, cut towards zero
	float near = low + step.x * CutBy(high - low, cellTexels);
	float far = lowFar + step.x * CutBy(highFar - lowFar, cellTexels);
	float depth = near + step.y * CutBy(far - near, cellTexels);
	// The noise image lies over each block as its texture does. A smaller texture takes every second noise texel.
	vec2 noiseTexel = blockTexel * (FULL_TEXELS / blockTexels);
	float noise = floor(texture2DLod(s12_snowNoise, (noiseTexel + 0.5f) / FULL_TEXELS, 0.0f).r * 255.0f + 0.5f);
	float above = floor(depth / 65536.0f) - noise;
	if (above < 8.0f)
	{
		return levels;
	}
	float level = floor(above / 8.0f);
	float white = 14.0f + mod(floor(noise / 4.0f) + floor(noise / 2.0f), 2.0f);
	if (level >= 16.0f)
	{
		return vec3(white, white, 15.0f);
	}
	// Each level is scaled down and rounded down, and the grey is added in the same way
	return floor(levels * (16.0f - level) / 16.0f) + floor(white * level / 16.0f);
}

// A texel of the block's texture, which holds the footprints, with the snow and the shadows painted into it, in that
// order, as the game paints them. Each 4-bit channel is rounded down again after each step. blockOrigin is the lowest x
// and z of the block. blockTexel is the texel along the axes of the block's texture (z, then x). islandUv is the same
// point on the island's textures.
vec4 PaintedTexelOf(float layer, vec2 blockOrigin, vec2 blockTexel, vec2 islandUv)
{
	vec4 land = texture2DArrayLod(s0_blockTextures, vec3((blockTexel + 0.5f) / BLOCK_TEXELS, layer), 0.0f);
	vec3 levels = floor(land.rgb * 15.0f + 0.5f);
	if (u_snow.x > 0.5f)
	{
		levels = SnowOverTexel(levels, blockOrigin, blockTexel, BLOCK_TEXELS);
	}
	// The shadows are baked into each texel: its 4-bit colour scaled and rounded down. The paint keeps every baked
	// shadow. The moving shadows do not change the paint: the land draws them over the painted texture.
	if (u_objectShadows.w > 0.5f)
	{
		float shade = floor(texture2DLod(s14_landShadows, islandUv, 0.0f).r * 255.0f + 0.5f);
		levels = floor(levels * shade / 256.0f + 0.001f);
	}
	return vec4(levels / 15.0f, land.a);
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
	vec4 texel = PaintedTexelOf(layer, blockOrigin, blockTexel, IslandUvAt(xz));
	return floor(texel * 15.0f + 0.5f);
}

// The coarse texels sampled along a side of a block, between the texels shared with its neighbours
float CoarseSamples()
{
	return BLOCK_TEXELS * 0.25f - 2.0f;
}

// The texel of a texture that a texel of its coarse texture (1 to samples) is sampled from, along one side. The samples
// step evenly across the texture, each on a multiple of four (see land_detail::CoarseSourceTexel).
float CoarseSourceOf(float coarseTexel, float samples, float step)
{
	float index = clamp(coarseTexel, 1.0f, samples) - 1.0f;
	return floor(index * step / 65536.0f) * 4.0f;
}

// The texel of the block's texture that a texel of its coarse texture is sampled from, along one side
float CoarseSource(float coarseTexel)
{
	return CoarseSourceOf(coarseTexel, CoarseSamples(), u_landTexels.y);
}

// Two blocks' texels shared across their edge, each 4-bit level halved and rounded down
vec4 SharedLevels(vec4 a, vec4 b)
{
	return floor((a + b) * 0.5f);
}

// A new sample of a block's coarse texture, in 4-bit levels: the painted texel at a cell of the coarse texture, z and x
// 0 to samples - 1. block is the block's place on the map, x and z.
vec4 CoarseSampleOf(float layer, vec2 block, vec2 cell)
{
	return PaintedLevels(layer, block * BLOCK_UNITS, vec2(CoarseSource(cell.x + 1.0f), CoarseSource(cell.y + 1.0f)));
}

#endif // LAND_PAINT_SH
