#ifndef COARSE_EDGES_SH
#define COARSE_EDGES_SH

// The cells round the edges of the blocks' coarse textures, as the paints of the blocks mixed them. Define
// COARSE_EDGES as the sampler of the texture before the include.
//
// The texture has a row for each place of the 32 x 32 map, x * 32 + z, and in each row a run of 64 texels for each
// side of the block: z - 1, z + 1, x - 1 and x + 1. A cell is a sample of the coarse texture, and holds the texel on the
// edge and the texel next to it inside. The texture keeps a corner cell only in the run of its side along z. Each
// channel is a 4-bit level over 15.

#define COARSE_EDGE_SIDE_TEXELS 64.0f
#define COARSE_EDGE_TEXTURE_SIZE vec2(256.0f, 1024.0f)

// The texel of the texture that keeps a cell round the edge of a block. block is the place of the block on the map, x
// and z. cell is the cell of its coarse texture, z and x, 0 to samples - 1, on an edge.
vec2 CoarseEdgeTexel(vec2 block, vec2 cell, float samples)
{
	float last = samples - 1.0f;
	float row = block.x * 32.0f + block.y;
	if (cell.x < 0.5f)
	{
		return vec2(cell.y, row);
	}
	if (cell.x > last - 0.5f)
	{
		return vec2(COARSE_EDGE_SIDE_TEXELS + cell.y, row);
	}
	if (cell.y < 0.5f)
	{
		return vec2(2.0f * COARSE_EDGE_SIDE_TEXELS + cell.x, row);
	}
	return vec2(3.0f * COARSE_EDGE_SIDE_TEXELS + cell.x, row);
}

// A cell round the edge of a block, in 4-bit levels
vec4 CoarseEdgeLevels(vec2 block, vec2 cell, float samples)
{
	vec2 texel = CoarseEdgeTexel(block, cell, samples);
	return floor(texture2DLod(COARSE_EDGES, (texel + 0.5f) / COARSE_EDGE_TEXTURE_SIZE, 0.0f) * 15.0f + 0.5f);
}

// Whether a cell of a coarse texture (z and x, 0 to samples - 1) is round its edge
bool OnCoarseEdge(vec2 cell, float samples)
{
	return any(lessThan(cell, vec2_splat(0.5f))) || any(greaterThan(cell, vec2_splat(samples - 1.5f)));
}

#endif // COARSE_EDGES_SH
