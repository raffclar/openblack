/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <bitset>
#include <map>
#include <random>
#include <set>
#include <vector>

#include <Visual/CoarseEdges.h>
#include <gtest/gtest.h>

using namespace openblack::graphics::coarse_edges;

namespace
{

/// The game's coarse slots, one paint after the other, texel by texel: a slot of samples + 2 texels a side, its
/// columns along z and its rows along x. At each paint the slot takes new samples, its edge texels copy the texels
/// next to them, and then each shared edge is mixed with each painted neighbour, z - 1, z + 1, x - 1, x + 1.
class GameSlots
{
public:
	explicit GameSlots(int32_t samples)
	    : _samples(samples)
	    , _side(samples + 2)
	{
	}

	/// A paint of a block, with its new sample at each cell (z, x)
	void Paint(glm::ivec2 block, const std::function<Levels(Cell)>& fresh)
	{
		auto& own = SlotOf(block);
		_painted.set(PlaceOf(block));
		const auto last = _side - 1;
		for (int32_t row = 1; row < last; ++row)
		{
			for (int32_t column = 1; column < last; ++column)
			{
				At(own, row, column) = fresh(Cell(column - 1, row - 1));
			}
			At(own, row, 0) = At(own, row, 1);
			At(own, row, last) = At(own, row, last - 1);
		}
		for (int32_t column = 0; column < _side; ++column)
		{
			At(own, 0, column) = At(own, 1, column);
			At(own, last, column) = At(own, last - 1, column);
		}
		// z - 1: the neighbour's last two columns and the block's first two
		if (auto* low = Painted(block + glm::ivec2(0, -1)))
		{
			for (int32_t row = 0; row < _side; ++row)
			{
				const auto mixed = Mix(At(*low, row, last), At(own, row, 0));
				At(*low, row, last - 1) = At(own, row, 1) = At(*low, row, last) = At(own, row, 0) = mixed;
			}
		}
		if (auto* high = Painted(block + glm::ivec2(0, 1)))
		{
			for (int32_t row = 0; row < _side; ++row)
			{
				const auto mixed = Mix(At(own, row, last), At(*high, row, 0));
				At(own, row, last - 1) = At(own, row, last) = At(*high, row, 0) = At(*high, row, 1) = mixed;
			}
		}
		if (auto* low = Painted(block + glm::ivec2(-1, 0)))
		{
			for (int32_t column = 0; column < _side; ++column)
			{
				const auto mixed = Mix(At(*low, last, column), At(own, 0, column));
				At(*low, last - 1, column) = At(own, 1, column) = At(*low, last, column) = At(own, 0, column) = mixed;
			}
		}
		if (auto* high = Painted(block + glm::ivec2(1, 0)))
		{
			for (int32_t column = 0; column < _side; ++column)
			{
				const auto mixed = Mix(At(own, last, column), At(*high, 0, column));
				At(own, last - 1, column) = At(own, last, column) = At(*high, 0, column) = At(*high, 1, column) = mixed;
			}
		}
	}

	[[nodiscard]] bool IsPainted(glm::ivec2 block) const { return OnMap(block) && _painted.test(PlaceOf(block)); }

	/// The texel of a block's slot at a cell (z, x)
	[[nodiscard]] Levels CellOf(glm::ivec2 block, Cell cell) const
	{
		return _slots.at(PlaceOf(block)).at(static_cast<size_t>(((cell.y + 1) * _side) + cell.x + 1));
	}

	/// Whether each edge texel of every painted slot holds the value of the texel next to it inside
	[[nodiscard]] bool EdgesDoubled() const
	{
		const auto last = _side - 1;
		for (const auto& [place, slot] : _slots)
		{
			for (int32_t i = 0; i < _side; ++i)
			{
				const auto& s = slot;
				const auto at = [&](int32_t row, int32_t column) { return s.at(static_cast<size_t>((row * _side) + column)); };
				if (at(i, 0) != at(i, 1) || at(i, last) != at(i, last - 1) || at(0, i) != at(1, i) ||
				    at(last, i) != at(last - 1, i))
				{
					return false;
				}
			}
		}
		return true;
	}

private:
	using Slot = std::vector<Levels>;

	Slot& SlotOf(glm::ivec2 block)
	{
		auto& slot = _slots[PlaceOf(block)];
		slot.resize(static_cast<size_t>(_side) * _side);
		return slot;
	}
	Slot* Painted(glm::ivec2 block) { return IsPainted(block) ? &_slots.at(PlaceOf(block)) : nullptr; }
	Levels& At(Slot& slot, int32_t row, int32_t column) const { return slot.at(static_cast<size_t>((row * _side) + column)); }

	int32_t _samples;
	int32_t _side;
	std::map<size_t, Slot> _slots;
	std::bitset<k_Blocks> _painted;
};

/// The cells round the edges of each block's coarse texture, as the texture of the pass keeps them
class PassCells
{
public:
	explicit PassCells(int32_t samples)
	    : _samples(samples)
	{
	}

	[[nodiscard]] Levels Kept(glm::ivec2 block, Cell cell) const
	{
		const auto texel = EdgeTexel(block, cell, _samples);
		const auto found = _texels.find({texel.x, texel.y});
		return found != _texels.end() ? found->second : Levels {0};
	}

	/// The pass of a frame: every cell round the edges of the blocks takes its value after the frame's paints
	void Frame(const FramePaints& paints, std::span<const glm::ivec2> places, const CellLevels& fresh)
	{
		std::map<std::pair<int, int>, Levels> after = _texels;
		const auto kept = [this](glm::ivec2 block, Cell cell) { return Kept(block, cell); };
		for (const auto place : places)
		{
			for (const auto cell : RingCells())
			{
				const auto texel = EdgeTexel(place, cell, _samples);
				after[{texel.x, texel.y}] = CellAfterFrame(paints, _samples, place, cell, kept, fresh);
			}
		}
		_texels = std::move(after);
	}

	[[nodiscard]] std::vector<Cell> RingCells() const
	{
		std::vector<Cell> cells;
		const auto last = _samples - 1;
		for (int32_t i = 0; i < _samples; ++i)
		{
			cells.emplace_back(0, i);
			cells.emplace_back(last, i);
			if (i > 0 && i < last)
			{
				cells.emplace_back(i, 0);
				cells.emplace_back(i, last);
			}
		}
		return cells;
	}

private:
	int32_t _samples;
	std::map<std::pair<int, int>, Levels> _texels;
};

Levels RandomLevels(std::mt19937& random)
{
	std::uniform_int_distribution<int> level(0, 15);
	return {static_cast<uint8_t>(level(random)), static_cast<uint8_t>(level(random)), static_cast<uint8_t>(level(random)),
	        static_cast<uint8_t>(level(random))};
}

/// Paints random blocks of a small map over many frames, each block with random samples, in the game's way and with
/// the pass, and compares every cell round the edges of every painted block after each frame
void CompareWithTheGame(int32_t samples, uint32_t seed)
{
	std::mt19937 random(seed);
	// A map of 5 x 5 places at its far corner, with some places of sea that have no block
	std::vector<glm::ivec2> places;
	for (int x = 27; x < 32; ++x)
	{
		for (int z = 27; z < 32; ++z)
		{
			if ((x * 7 + z * 3) % 6 != 0)
			{
				places.emplace_back(x, z);
			}
		}
	}
	GameSlots game(samples);
	PassCells pass(samples);
	std::bitset<k_Blocks> painted;
	for (int frame = 0; frame < 30; ++frame)
	{
		// Some blocks get a paint in this frame, in a random order
		std::vector<size_t> order;
		for (size_t block = 0; block < places.size(); ++block)
		{
			if (std::uniform_int_distribution<int>(0, 2)(random) == 0)
			{
				order.push_back(block);
			}
		}
		std::ranges::shuffle(order, random);
		std::map<std::pair<size_t, std::pair<int, int>>, Levels> samplesOfFrame;
		const auto fresh = [&](glm::ivec2 block, Cell cell) {
			const auto key = std::pair {PlaceOf(block), std::pair {cell.x, cell.y}};
			auto found = samplesOfFrame.find(key);
			if (found == samplesOfFrame.end())
			{
				found = samplesOfFrame.emplace(key, RandomLevels(random)).first;
			}
			return found->second;
		};
		const auto paints = MakeFramePaints(places, order, painted);
		for (const auto block : order)
		{
			game.Paint(places[block], [&](Cell cell) { return fresh(places[block], cell); });
		}
		pass.Frame(paints, places, fresh);
		for (const auto block : order)
		{
			painted.set(PlaceOf(places[block]));
		}
		ASSERT_TRUE(game.EdgesDoubled());
		for (const auto place : places)
		{
			if (!game.IsPainted(place))
			{
				continue;
			}
			for (const auto cell : pass.RingCells())
			{
				ASSERT_EQ(pass.Kept(place, cell), game.CellOf(place, cell))
				    << "frame " << frame << " block " << place.x << "," << place.y << " cell " << cell.x << "," << cell.y;
			}
		}
	}
}

} // namespace

TEST(CoarseEdges, MixHalvesEachLevelAndRoundsDown)
{
	EXPECT_EQ(Mix({15, 0, 7, 8}, {0, 0, 8, 9}), Levels(7, 0, 7, 8));
}

TEST(CoarseEdges, ThePassMakesTheGamesEdgesAtFullResolution)
{
	for (const uint32_t seed : {1u, 2u})
	{
		CompareWithTheGame(62, seed);
	}
}

TEST(CoarseEdges, ThePassMakesTheGamesEdgesAtLowResolution)
{
	CompareWithTheGame(30, 4);
}

TEST(CoarseEdges, ABlockPaintedAgainMixesItsNewSampleWithTheOldMix)
{
	const std::vector<glm::ivec2> places {{4, 4}, {4, 5}};
	const auto samples = 62;
	PassCells pass(samples);
	const Cell edge {samples - 1, 10};
	const Cell across {0, 10};
	// Both blocks paint: the second mixes its sample with the first one's
	pass.Frame(MakeFramePaints(places, std::vector<size_t> {0, 1}, {}), places,
	           [](glm::ivec2 block, Cell) { return block.y == 4 ? Levels(12, 12, 12, 15) : Levels(2, 2, 2, 15); });
	EXPECT_EQ(pass.Kept(places[0], edge), Levels(7, 7, 7, 15));
	EXPECT_EQ(pass.Kept(places[1], across), Levels(7, 7, 7, 15));
	// The first block paints again with the same sample: the edge goes three quarters of the way to it
	std::bitset<k_Blocks> painted;
	painted.set(PlaceOf(places[0]));
	painted.set(PlaceOf(places[1]));
	pass.Frame(MakeFramePaints(places, std::vector<size_t> {0}, painted), places,
	           [](glm::ivec2, Cell) { return Levels(12, 12, 12, 15); });
	EXPECT_EQ(pass.Kept(places[0], edge), Levels(9, 9, 9, 15));
	EXPECT_EQ(pass.Kept(places[1], across), Levels(9, 9, 9, 15));
}

TEST(CoarseEdges, ANeighbourWithNoPaintDoesNotMix)
{
	const std::vector<glm::ivec2> places {{4, 4}, {4, 5}};
	PassCells pass(62);
	pass.Frame(MakeFramePaints(places, std::vector<size_t> {0}, {}), places,
	           [](glm::ivec2, Cell) { return Levels(12, 3, 4, 15); });
	EXPECT_EQ(pass.Kept(places[0], Cell(61, 20)), Levels(12, 3, 4, 15));
}

TEST(CoarseEdges, TheOrderOfThePaintsChangesTheCorners)
{
	// Three blocks round a corner: the block, its neighbour across z and its neighbour across x
	const std::vector<glm::ivec2> places {{4, 4}, {4, 5}, {5, 4}};
	const auto samples = 62;
	const auto last = samples - 1;
	const auto fresh = [](glm::ivec2 block, Cell) {
		if (block == glm::ivec2(4, 4))
		{
			return Levels(15, 15, 15, 15);
		}
		return block.y == 5 ? Levels(0, 0, 0, 15) : Levels(4, 4, 4, 15);
	};
	// Painted last, the block mixes along z, (15 + 0) / 2 = 7, then along x, (7 + 4) / 2 = 5
	PassCells first(samples);
	first.Frame(MakeFramePaints(places, std::vector<size_t> {1, 2, 0}, {}), places, fresh);
	EXPECT_EQ(first.Kept(places[0], Cell(last, last)), Levels(5, 5, 5, 15));
	EXPECT_EQ(first.Kept(places[1], Cell(0, last)), Levels(7, 7, 7, 15));
	EXPECT_EQ(first.Kept(places[2], Cell(last, 0)), Levels(5, 5, 5, 15));
	// Painted after the neighbour across x, the block mixes with it, (15 + 4) / 2 = 9. The neighbour across z then
	// mixes with that, (0 + 9) / 2 = 4.
	PassCells second(samples);
	second.Frame(MakeFramePaints(places, std::vector<size_t> {2, 0, 1}, {}), places, fresh);
	EXPECT_EQ(second.Kept(places[0], Cell(last, last)), Levels(4, 4, 4, 15));
	EXPECT_EQ(second.Kept(places[1], Cell(0, last)), Levels(4, 4, 4, 15));
	EXPECT_EQ(second.Kept(places[2], Cell(last, 0)), Levels(9, 9, 9, 15));
	// An edge cell next to the corner mixes only across x: (15 + 4) / 2 = 9 in both orders
	EXPECT_EQ(first.Kept(places[0], Cell(10, last)), Levels(9, 9, 9, 15));
	EXPECT_EQ(second.Kept(places[0], Cell(10, last)), Levels(9, 9, 9, 15));
}

TEST(CoarseEdges, FramePaintsRankTheBlocksInTheirOrder)
{
	const std::vector<glm::ivec2> places {{0, 0}, {0, 1}, {3, 2}};
	std::bitset<k_Blocks> painted;
	painted.set(PlaceOf({3, 2}));
	const auto paints = MakeFramePaints(places, std::vector<size_t> {2, 0, 2}, painted);
	EXPECT_EQ(paints.at(PlaceOf({3, 2})).rank, 1);
	EXPECT_TRUE(paints.at(PlaceOf({3, 2})).paintedBefore);
	EXPECT_EQ(paints.at(PlaceOf({0, 0})).rank, 2);
	EXPECT_EQ(paints.at(PlaceOf({0, 1})).rank, 0);
	EXPECT_EQ(paints.at(PlaceOf({0, 1})).block, std::optional<uint16_t>(1));
	EXPECT_FALSE(paints.at(PlaceOf({5, 5})).block.has_value());
	const auto texels = FramePaintTexels(paints);
	const auto at = PlaceOf({3, 2}) * 4;
	EXPECT_EQ(texels[at], 1.0f);
	EXPECT_EQ(texels[at + 1], 1.0f);
	EXPECT_EQ(texels[at + 2], 3.0f);
}

TEST(CoarseEdges, EachCellHasOneTexel)
{
	for (const auto samples : {62, 30})
	{
		PassCells pass(samples);
		std::set<std::pair<int, int>> texels;
		for (const auto cell : pass.RingCells())
		{
			const auto texel = EdgeTexel({1, 2}, cell, samples);
			EXPECT_TRUE(texels.insert({texel.x, texel.y}).second);
			EXPECT_EQ(texel.y, 34);
			EXPECT_LT(texel.x, k_TextureWidth);
		}
		EXPECT_EQ(texels.size(), static_cast<size_t>(4 * (samples - 1)));
	}
}

TEST(CoarseEdges, PaintOrderPutsTheNearestBlockFirst)
{
	const std::vector<PaintRequest> requests {{.block = 0, .centre = {800.0f, 80.0f}}, {.block = 1, .centre = {80.0f, 80.0f}}};
	EXPECT_EQ(PaintOrder(requests, {0.0f, 100.0f, 0.0f}), (std::vector<size_t> {1, 0}));
}

TEST(CoarseEdges, PaintOrderPutsTheLaterRequestFirstAtTheSameDistance)
{
	const std::vector<PaintRequest> requests {{.block = 0, .centre = {80.0f, 240.0f}},
	                                          {.block = 1, .centre = {240.0f, 80.0f}},
	                                          {.block = 0, .centre = {80.0f, 240.0f}}};
	EXPECT_EQ(PaintOrder(requests, {0.0f, 50.0f, 0.0f}), (std::vector<size_t> {1, 0}));
}

TEST(CoarseEdges, ABlockShowsItsFilePageUntilItsFirstPaint)
{
	EXPECT_EQ(CoarseTextureOf(false, true, false), CoarseTexture::FilePage);
	EXPECT_EQ(CoarseTextureOf(false, true, true), CoarseTexture::Samples);
	// A land without pages samples the block's painted texture
	EXPECT_EQ(CoarseTextureOf(false, false, false), CoarseTexture::Samples);
	// The coarse land draws every block from its page
	EXPECT_EQ(CoarseTextureOf(true, true, false), CoarseTexture::CoarseLand);
	EXPECT_EQ(CoarseTextureOf(true, true, true), CoarseTexture::CoarseLand);
	EXPECT_EQ(CoarseTextureOf(true, false, true), CoarseTexture::Samples);
}

TEST(CoarseEdges, ARepaintWaitsUntilTheLandDrawsTheBlockWithItsTexture)
{
	// Blocks 0 to 3 got a paint before. Blocks 1 and 3 have a request; the land draws block 3 and block 0 with their
	// own textures.
	const std::vector<bool> waiting {false, true, false, true};
	const auto painted = [](size_t) { return true; };
	const auto idle = [](size_t) { return false; };
	EXPECT_EQ(PaintsDue(waiting, std::vector<size_t> {3, 0}, painted, idle), (std::vector<size_t> {3}));
	// A block that the land draws with its coarse texture alone keeps waiting
	EXPECT_TRUE(PaintsDue(waiting, std::vector<size_t> {}, painted, idle).empty());
}

TEST(CoarseEdges, ABlockWithNoPaintPaintsAtOnce)
{
	const std::vector<bool> waiting {true, true, true};
	const auto painted = [](size_t block) { return block == 1; };
	const auto idle = [](size_t) { return false; };
	// The blocks with no paint come first, in the order of their indices, then the drawn blocks in the order of the draws
	EXPECT_EQ(PaintsDue(waiting, std::vector<size_t> {1, 2}, painted, idle), (std::vector<size_t> {0, 2, 1}));
}

TEST(CoarseEdges, ABlockWhoseShadowsBakeWaits)
{
	const std::vector<bool> waiting {true, true};
	const auto painted = [](size_t) { return true; };
	const auto bakes = [](size_t block) { return block == 0; };
	EXPECT_EQ(PaintsDue(waiting, std::vector<size_t> {0, 1, 1}, painted, bakes), (std::vector<size_t> {1}));
}

TEST(CoarseEdges, EachCellOfEachBlockHasItsOwnCoarseTexel)
{
	std::set<std::pair<int32_t, int32_t>> seen;
	for (int32_t x = 0; x < k_BlocksPerSide; x += 31)
	{
		for (int32_t z = 0; z < k_BlocksPerSide; z += 31)
		{
			for (int32_t cz = 0; cz < k_SideTexels; ++cz)
			{
				for (int32_t cx = 0; cx < k_SideTexels; ++cx)
				{
					const auto texel = CoarseTexel({x, z}, {cz, cx});
					EXPECT_GE(texel.x, 0);
					EXPECT_GE(texel.y, 0);
					EXPECT_LT(texel.x, k_CoarseTexturesSide);
					EXPECT_LT(texel.y, k_CoarseTexturesSide);
					EXPECT_TRUE(seen.emplace(texel.x, texel.y).second);
				}
			}
		}
	}
	// A column of squares for each z and a row of squares for each x
	EXPECT_EQ(CoarseTexel({1, 2}, {3, 4}), glm::ivec2(2 * 64 + 3, 1 * 64 + 4));
}
