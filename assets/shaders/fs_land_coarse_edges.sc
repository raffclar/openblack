$input v_texcoord0

#include <bgfx_shader.sh>

// The pass reads no snow depth grid
#define SNOW_NO_DEPTH_GRID
#include "snow.sh"
#include "land_paint.sh"

// The cells round the edges of the blocks' coarse textures before the paints of this frame
SAMPLER2D(s1_coarseEdgesBefore, 1);
#define COARSE_EDGES s1_coarseEdgesBefore
#include "coarse_edges.sh"
// The paints of this frame, a texel for each place of the 32 x 32 map, a row for each x and a column for each z:
// r: the place of the block's paint in the order of the paints (1 for the first), 0 when it gets none
// g: 1 when the block got a paint before this frame
// b: the block's layer of s0_blockTextures plus 1, 0 when the place has no block
SAMPLER2D(s2_coarsePaints, 2);

// The cells round the edges of the blocks' coarse textures after the paints of a frame.
//
// At each paint of a block, the game samples the block's coarse texture again from its painted texture. Then it mixes
// each edge that the block shares with a neighbour that got a paint before, in the order z - 1, z + 1, x - 1, x + 1.
// The mix halves the sum of the block's new sample and the neighbour's kept cell, rounded down, and writes it to both.
//
// A cell that is not at a corner changes only at the paints of its two blocks. The four cells at a corner of four
// blocks change only at the paints of these four blocks. Thus each cell finds its value from the paints of its group in
// the order of the paints, with no other cell. The tests of coarse_edges::CellAfterFrame check this against each paint
// done one after the other.

// The paints of a block this frame, as s2_coarsePaints gives them; none off the map
vec4 PaintsOf(vec2 block)
{
	if (any(lessThan(block, vec2_splat(0.0f))) || any(greaterThan(block, vec2_splat(31.0f))))
	{
		return vec4_splat(0.0f);
	}
	return floor(texture2DLod(s2_coarsePaints, (block.yx + 0.5f) / 32.0f, 0.0f) + 0.5f);
}

// The cell of a coarse texture that is not at a corner. The block's own cell and the neighbour's cell across the
// edge take the paints of the two blocks, in their order.
vec4 EdgeCellAfter(vec2 block, vec2 cell, vec2 neighbour, vec2 neighbourCell, float samples, vec4 before)
{
	vec4 own = PaintsOf(block);
	vec4 other = PaintsOf(neighbour);
	if (own.r < 0.5f && other.r < 0.5f)
	{
		return before;
	}
	vec4 ownCell = before;
	vec4 otherCell = other.g > 0.5f ? CoarseEdgeLevels(neighbour, neighbourCell, samples) : vec4_splat(0.0f);
	bool ownFirst = own.r > 0.5f && (other.r < 0.5f || own.r < other.r);
	for (int i = 0; i < 2; ++i)
	{
		bool ownTurn = (i == 0) == ownFirst;
		if (ownTurn && own.r > 0.5f)
		{
			ownCell = CoarseSampleOf(own.b - 1.0f, block, cell);
			if (other.g > 0.5f)
			{
				ownCell = SharedLevels(ownCell, otherCell);
				otherCell = ownCell;
			}
			own.g = 1.0f;
		}
		else if (!ownTurn && other.r > 0.5f)
		{
			otherCell = CoarseSampleOf(other.b - 1.0f, neighbour, neighbourCell);
			if (own.g > 0.5f)
			{
				otherCell = SharedLevels(otherCell, ownCell);
				ownCell = otherCell;
			}
			other.g = 1.0f;
		}
	}
	return ownCell;
}

// The index along x and along z of member m of the four blocks round a corner
vec2 MemberIndex(int m)
{
	return vec2(m >= 2 ? 1.0f : 0.0f, (m == 1 || m == 3) ? 1.0f : 0.0f);
}

// The member of the four blocks round a corner at an index along x and along z
int MemberAt(vec2 index)
{
	return int(index.x) * 2 + int(index.y);
}

// A cell at a corner. The four blocks round the corner are i along x and j along z from the lowest, member 2i + j. A
// block's cell at the corner is at its high edge along an axis where its index is 0. A paint of a member samples its
// cell, then mixes it with the member across z, then with the member across x.
vec4 CornerCellAfter(vec2 block, vec2 cell, float samples, vec4 before)
{
	float last = samples - 1.0f;
	// The index of the block among the four, along x and along z
	vec2 ownIndex = vec2(cell.y > 0.5f ? 0.0f : 1.0f, cell.x > 0.5f ? 0.0f : 1.0f);
	vec2 lowest = block - ownIndex;
	vec4 paints[4];
	vec4 cells[4];
	for (int m = 0; m < 4; ++m)
	{
		vec2 index = MemberIndex(m);
		vec2 member = lowest + index;
		vec2 memberCell = vec2(index.y < 0.5f ? last : 0.0f, index.x < 0.5f ? last : 0.0f);
		paints[m] = PaintsOf(member);
		cells[m] = paints[m].g > 0.5f ? CoarseEdgeLevels(member, memberCell, samples) : vec4_splat(0.0f);
	}
	// The paints in their order: each turn takes the paint with the lowest rank after the turn before
	float done = 0.0f;
	for (int turn = 0; turn < 4; ++turn)
	{
		int next = -1;
		float rank = 4096.0f;
		for (int m = 0; m < 4; ++m)
		{
			if (paints[m].r > done + 0.5f && paints[m].r < rank)
			{
				rank = paints[m].r;
				next = m;
			}
		}
		if (next >= 0)
		{
			done = rank;
			vec2 index = MemberIndex(next);
			vec2 memberCell = vec2(index.y < 0.5f ? last : 0.0f, index.x < 0.5f ? last : 0.0f);
			vec4 mixed = CoarseSampleOf(paints[next].b - 1.0f, lowest + index, memberCell);
			// The member across z, then the member across x
			int acrossZ = MemberAt(vec2(index.x, 1.0f - index.y));
			int acrossX = MemberAt(vec2(1.0f - index.x, index.y));
			if (paints[acrossZ].g > 0.5f)
			{
				mixed = SharedLevels(mixed, cells[acrossZ]);
				cells[acrossZ] = mixed;
			}
			if (paints[acrossX].g > 0.5f)
			{
				mixed = SharedLevels(mixed, cells[acrossX]);
				cells[acrossX] = mixed;
			}
			cells[next] = mixed;
			paints[next].g = 1.0f;
		}
	}
	int own = MemberAt(ownIndex);
	return paints[own].g > 0.5f ? cells[own] : before;
}

void main()
{
	vec2 texel = floor(v_texcoord0.xy);
	vec4 kept = texture2DLod(s1_coarseEdgesBefore, (texel + 0.5f) / COARSE_EDGE_TEXTURE_SIZE, 0.0f);
	float samples = CoarseSamples();
	float last = samples - 1.0f;
	float side = floor(texel.x / COARSE_EDGE_SIDE_TEXELS);
	float k = texel.x - side * COARSE_EDGE_SIDE_TEXELS;
	vec2 block = vec2(floor(texel.y / 32.0f), texel.y - floor(texel.y / 32.0f) * 32.0f);
	// A side along z keeps its corners; a side along x keeps only the cells between them
	bool alongZ = side < 1.5f;
	bool stored = alongZ ? k < samples - 0.5f : k > 0.5f && k < last - 0.5f;
	if (!stored || PaintsOf(block).b < 0.5f)
	{
		gl_FragColor = kept;
		return;
	}
	vec4 before = floor(kept * 15.0f + 0.5f);
	vec2 cell;
	vec2 neighbour;
	vec2 neighbourCell;
	if (side < 0.5f)
	{
		cell = vec2(0.0f, k);
		neighbour = block - vec2(0.0f, 1.0f);
		neighbourCell = vec2(last, k);
	}
	else if (side < 1.5f)
	{
		cell = vec2(last, k);
		neighbour = block + vec2(0.0f, 1.0f);
		neighbourCell = vec2(0.0f, k);
	}
	else if (side < 2.5f)
	{
		cell = vec2(k, 0.0f);
		neighbour = block - vec2(1.0f, 0.0f);
		neighbourCell = vec2(k, last);
	}
	else
	{
		cell = vec2(k, last);
		neighbour = block + vec2(1.0f, 0.0f);
		neighbourCell = vec2(k, 0.0f);
	}
	bool corner = alongZ && (k < 0.5f || k > last - 0.5f);
	vec4 levels = corner ? CornerCellAfter(block, cell, samples, before)
	                     : EdgeCellAfter(block, cell, neighbour, neighbourCell, samples, before);
	gl_FragColor = levels / 15.0f;
}
