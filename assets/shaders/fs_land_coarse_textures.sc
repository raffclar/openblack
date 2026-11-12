$input v_texcoord0

#include <bgfx_shader.sh>

// The pass reads no snow depth grid
#define SNOW_NO_DEPTH_GRID
#include "snow.sh"
#include "land_paint.sh"

// The cells round the edges of the blocks' coarse textures after the paints of this pass
SAMPLER2D(s1_coarseEdgesAfter, 1);
#define COARSE_EDGES s1_coarseEdgesAfter
#include "coarse_edges.sh"
// The paints of this pass, a texel for each place of the 32 x 32 map, as fs_land_coarse_edges reads them:
// r: the place of the block's paint in the order of the paints (1 for the first), 0 when it gets none
// b: the block's layer of s0_blockTextures plus 1, 0 when the place has no block
SAMPLER2D(s2_coarsePaints, 2);

// The blocks' coarse textures after the paints of a pass.
//
// At each paint of a block, the game samples the block's coarse texture again from its painted texture. Then it mixes
// the edges with the neighbours. A coarse texture does not change between the paints. The pass draws the squares of
// the blocks that paint and of their neighbours. In a block that paints, each cell takes its new sample. In each
// square, the cells round the edge take their value from the edges after the paints. The other cells stay as they are.

void main()
{
	vec2 texel = floor(v_texcoord0.xy);
	vec2 square = floor(texel / COARSE_EDGE_SIDE_TEXELS);
	// The place of the block on the map, x and z, and the cell, z and x
	vec2 block = square.yx;
	vec2 cell = texel - square * COARSE_EDGE_SIDE_TEXELS;
	float samples = CoarseSamples();
	vec4 paints = floor(texture2DLod(s2_coarsePaints, (block.yx + 0.5f) / 32.0f, 0.0f) + 0.5f);
	if (paints.b < 0.5f || any(greaterThan(cell, vec2_splat(samples - 0.5f))))
	{
		discard;
	}
	if (OnCoarseEdge(cell, samples))
	{
		gl_FragColor = CoarseEdgeLevels(block, cell, samples) / 15.0f;
		return;
	}
	if (paints.r < 0.5f)
	{
		discard;
	}
	gl_FragColor = CoarseSampleOf(paints.b - 1.0f, block, cell) / 15.0f;
}
