#ifndef COARSE_TEXTURES_SH
#define COARSE_TEXTURES_SH

// The blocks' coarse textures as their last paints made them. Define COARSE_TEXTURES as the sampler of the texture
// before the include.
//
// The texture has a square of 64 x 64 texels for each place of the 32 x 32 map: a column of squares for each z and a
// row of squares for each x (see coarse_edges::CoarseTexel). In a square, a texel holds a cell of the block's coarse
// texture, z along the row and x down the column. Each channel is a 4-bit level over 15.

#define COARSE_TEXTURE_SIDE_TEXELS 64.0f
#define COARSE_TEXTURES_SIZE 2048.0f

// The texel of the texture that keeps a cell of a block's coarse texture. block is the place of the block on the map,
// x and z. cell is the cell of its coarse texture, z and x, 0 to samples - 1.
vec2 CoarseTexel(vec2 block, vec2 cell)
{
	return block.yx * COARSE_TEXTURE_SIDE_TEXELS + cell;
}

// A cell of a block's coarse texture, in 4-bit levels
vec4 CoarseTextureLevels(vec2 block, vec2 cell)
{
	vec2 texel = CoarseTexel(block, cell);
	return floor(texture2DLod(COARSE_TEXTURES, (texel + 0.5f) / COARSE_TEXTURES_SIZE, 0.0f) * 15.0f + 0.5f);
}

#endif // COARSE_TEXTURES_SH
