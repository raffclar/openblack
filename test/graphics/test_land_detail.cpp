/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <set>
#include <vector>

#include <Graphics/LandDetail.h>
#include <LNDFile.h>
#include <gtest/gtest.h>

using namespace openblack::graphics::land_detail;
namespace lnd = openblack::lnd;

namespace
{

using Triangle = std::array<uint16_t, 3>;

std::set<Triangle> TriangleSet(const std::vector<uint16_t>& indices)
{
	std::set<Triangle> triangles;
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		Triangle triangle = {indices[i], indices[i + 1], indices[i + 2]};
		std::ranges::sort(triangle);
		triangles.insert(triangle);
	}
	return triangles;
}

std::set<Triangle> TriangleSet(std::initializer_list<Triangle> list)
{
	std::vector<uint16_t> indices;
	for (const auto& triangle : list)
	{
		indices.insert(indices.end(), triangle.begin(), triangle.end());
	}
	return TriangleSet(indices);
}

std::array<CellFlags, k_CornerCount> PlainCells()
{
	return {};
}

/// Each triangle's corners turn the same way in x and z
bool AllTurnTheSameWay(const std::vector<uint16_t>& indices)
{
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		const auto xz = [](uint16_t corner) { return std::array<int, 2> {corner / 17, corner % 17}; };
		const auto a = xz(indices[i]);
		const auto b = xz(indices[i + 1]);
		const auto c = xz(indices[i + 2]);
		if ((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) <= 0)
		{
			return false;
		}
	}
	return true;
}

CornerOffsets Uniform(float meshNear, float meshFar, float texture)
{
	CornerOffsets offsets;
	offsets.meshNear.fill(meshNear);
	offsets.meshFar.fill(meshFar);
	offsets.texture.fill(texture);
	return offsets;
}

} // namespace

TEST(LandDetail, MaximumUsesTheFurthestLinesAndLowerLevelsStepDown)
{
	EXPECT_EQ(For(4),
	          (Lines {.meshNear = 1600.0f, .meshFar = 2500.0f, .meshBand = 275.0f, .texture = 800.0f, .textureBand = 85.0f}));
	EXPECT_EQ(For(5), For(4));
	EXPECT_EQ(For(6), For(4));
	EXPECT_EQ(For(200), For(6));
	EXPECT_FLOAT_EQ(For(3).texture, 700.0f);
	EXPECT_FLOAT_EQ(For(2).texture, 600.0f);
	EXPECT_EQ(For(1),
	          (Lines {.meshNear = 300.0f, .meshFar = 600.0f, .meshBand = 25.0f, .texture = 400.0f, .textureBand = 35.0f}));
	EXPECT_EQ(For(0),
	          (Lines {.meshNear = 300.0f, .meshFar = 600.0f, .meshBand = 25.0f, .texture = 300.0f, .textureBand = 20.0f}));
}

TEST(LandDetail, LineLiesAtItsDepthAlongTheViewOnTheGround)
{
	// Looking level, the line is the depth ahead
	const auto level = LineAcross({0.0f, 100.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 300.0f, 0.0f);
	ASSERT_TRUE(level.valid);
	EXPECT_FLOAT_EQ(level.ahead, 300.0f);
	EXPECT_FLOAT_EQ(Offset(level, {100.0f, 50.0f}), 200.0f);
	EXPECT_FLOAT_EQ(Offset(level, {400.0f, -20.0f}), -100.0f);

	// Looking down at 45 degrees from 100 up, the ground is already 70.7 deep below the camera
	const float diagonal = std::sqrt(0.5f);
	const auto down = LineAcross({0.0f, 100.0f, 0.0f}, {0.0f, -diagonal, diagonal}, 300.0f, 0.0f);
	EXPECT_NEAR(down.ahead, 300.0f / diagonal - 100.0f, 1e-3f);
	// Measured at the camera's own height, nothing is gained
	const auto atHeight = LineAcross({0.0f, 100.0f, 0.0f}, {0.0f, -diagonal, diagonal}, 300.0f, 100.0f);
	EXPECT_NEAR(atHeight.ahead, 300.0f / diagonal, 1e-3f);

	// Looking straight down there is no line: everywhere counts as on it
	const auto straightDown = LineAcross({0.0f, 100.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 300.0f, 0.0f);
	EXPECT_FALSE(straightDown.valid);
	EXPECT_FLOAT_EQ(Offset(straightDown, {1000.0f, 1000.0f}), 0.0f);
}

TEST(LandDetail, BlocksTakeTheirMeshAndTextureFromTheLines)
{
	const Lines lines = For(0);
	// All before every line
	EXPECT_EQ(Classify(Uniform(100.0f, 400.0f, 100.0f), lines, false), (BlockDetail {}));
	// The first line's band
	EXPECT_EQ(Classify(Uniform(10.0f, 400.0f, 100.0f), lines, false).morph, Morph::ToHalf);
	// Corners either side of the first line, none in its band, still morph
	auto across = Uniform(100.0f, 400.0f, 100.0f);
	across.meshNear = {100.0f, 100.0f, -100.0f, -100.0f};
	EXPECT_EQ(Classify(across, lines, false), (BlockDetail {.mesh = Mesh::Full, .morph = Morph::ToHalf}));
	// Beyond the first line, before the second
	EXPECT_EQ(Classify(Uniform(-100.0f, 200.0f, -100.0f), lines, false),
	          (BlockDetail {.mesh = Mesh::Half, .morph = Morph::None, .texture = Texture::Coarse}));
	// In the second line's band
	EXPECT_EQ(Classify(Uniform(-100.0f, -25.0f, 0.0f), lines, false),
	          (BlockDetail {.mesh = Mesh::Half, .morph = Morph::ToQuarter, .texture = Texture::CrossFade}));
	// Beyond both
	EXPECT_EQ(Classify(Uniform(-400.0f, -100.0f, -100.0f), lines, false).mesh, Mesh::Quarter);
	// The band's edges belong to the band
	EXPECT_EQ(Classify(Uniform(25.0f, 400.0f, 20.0f), lines, false),
	          (BlockDetail {.mesh = Mesh::Full, .morph = Morph::ToHalf, .texture = Texture::CrossFade}));
	// The coarse land draws every block with its coarse texture
	EXPECT_EQ(Classify(Uniform(100.0f, 400.0f, 100.0f), lines, true).texture, Texture::Coarse);
}

TEST(LandDetail, OnlyCoarserNeighboursAreStitched)
{
	const std::array<std::optional<Mesh>, 4> neighbours = {Mesh::Half, Mesh::Full, std::nullopt, Mesh::Quarter};
	EXPECT_EQ(CoarserSides(Mesh::Full, neighbours), CoarserLowX | CoarserHighX);
	EXPECT_EQ(CoarserSides(Mesh::Half, neighbours), CoarserHighX);
	EXPECT_EQ(CoarserSides(Mesh::Quarter, neighbours), 0);
}

TEST(LandDetail, FullMeshSplitsEachCellAlongItsDiagonal)
{
	auto cells = PlainCells();
	const auto plain = BlockTriangles(Mesh::Full, 0, true, cells);
	EXPECT_EQ(plain.size(), 16u * 16u * 6u);
	EXPECT_TRUE(AllTurnTheSameWay(plain));
	EXPECT_TRUE(TriangleSet(plain).contains(Triangle {0, 1, 18}));

	cells[0].split = true;
	const auto split = TriangleSet(BlockTriangles(Mesh::Full, 0, true, cells));
	EXPECT_TRUE(split.contains(Triangle {0, 1, 17}));
	EXPECT_TRUE(split.contains(Triangle {1, 17, 18}));

	// Open sea is left out only when asked
	cells[0].openSea = true;
	EXPECT_EQ(BlockTriangles(Mesh::Full, 0, true, cells).size(), plain.size() - 6);
	EXPECT_EQ(BlockTriangles(Mesh::Full, 0, false, cells).size(), plain.size());
}

TEST(LandDetail, CoarserMeshesStepOverCorners)
{
	const auto cells = PlainCells();
	const auto half = BlockTriangles(Mesh::Half, 0, false, cells);
	EXPECT_EQ(half.size(), 8u * 8u * 6u);
	EXPECT_TRUE(TriangleSet(half).contains(Triangle {0, 2, 36}));
	const auto quarter = BlockTriangles(Mesh::Quarter, CoarserLowX, false, cells);
	EXPECT_EQ(quarter.size(), 4u * 4u * 6u);
	EXPECT_TRUE(AllTurnTheSameWay(quarter));
}

TEST(LandDetail, CoarseQuadsTakeTheFlagsOfTheCellCountedFromTheBlocksFirst)
{
	auto cells = PlainCells();
	// The half mesh's quad at x 2, z 0 is the ninth: it reads the ninth cell, not its own at [2 * 17]
	cells[9].split = true;
	const auto half = TriangleSet(BlockTriangles(Mesh::Half, 0, false, cells));
	EXPECT_TRUE(half.contains(Triangle {34, 36, 68}));
	EXPECT_FALSE(half.contains(Triangle {34, 36, 70}));
}

TEST(LandDetail, StitchedEdgesMatchTheGamesStrips)
{
	const auto cells = PlainCells();
	// The full mesh's strip along x = 0 against a half mesh
	const auto lowX = TriangleSet(BlockTriangles(Mesh::Full, CoarserLowX, false, cells));
	const auto expectedLowX = TriangleSet({
	    {0, 2, 18},   {0, 18, 17},  {2, 19, 18},  {2, 4, 20},   {2, 20, 19},  {4, 21, 20},  {4, 6, 22},   {4, 22, 21},
	    {6, 23, 22},  {6, 8, 24},   {6, 24, 23},  {8, 25, 24},  {8, 10, 26},  {8, 26, 25},  {10, 27, 26}, {10, 12, 28},
	    {10, 28, 27}, {12, 29, 28}, {12, 14, 30}, {12, 30, 29}, {14, 31, 30}, {14, 16, 32}, {14, 32, 31}, {16, 33, 32},
	});
	EXPECT_TRUE(std::ranges::includes(lowX, expectedLowX));
	EXPECT_EQ(lowX.size(), 15u * 16u * 2u + 24u);

	// Two stitched sides meeting at a corner
	const auto corner = BlockTriangles(Mesh::Full, CoarserLowX | CoarserLowZ, false, cells);
	EXPECT_EQ(corner.size() / 3, 15u * 15u * 2u + 46u);
	EXPECT_TRUE(AllTurnTheSameWay(corner));
	const auto cornerSet = TriangleSet(corner);
	EXPECT_FALSE(cornerSet.contains(Triangle {0, 17, 18}));
	EXPECT_FALSE(cornerSet.contains(Triangle {0, 1, 18}));
	EXPECT_TRUE(cornerSet.contains(Triangle {0, 18, 34}));

	// The half mesh against a quarter mesh on its high x and high z sides
	const auto half = TriangleSet(BlockTriangles(Mesh::Half, CoarserHighX | CoarserHighZ, false, cells));
	const auto expectedHalf = TriangleSet({
	    {240, 276, 272}, {238, 240, 272}, {240, 242, 276}, {244, 280, 276}, {242, 244, 276}, {244, 246, 280},
	    {248, 284, 280}, {246, 248, 280}, {248, 250, 284}, {252, 288, 284}, {250, 252, 284}, {16, 84, 48},
	    {14, 16, 48},    {48, 84, 82},    {84, 152, 116},  {82, 84, 116},   {116, 152, 150}, {152, 220, 184},
	    {150, 152, 184}, {184, 220, 218}, {220, 288, 252}, {218, 220, 252},
	});
	EXPECT_TRUE(std::ranges::includes(half, expectedHalf));
	EXPECT_EQ(half.size(), 7u * 7u * 2u + 22u);
}

TEST(LandDetail, CornersMorphTowardsTheirCoarserNeighbours)
{
	std::array<uint8_t, k_CornerCount> altitudes {};
	const auto set = [&](size_t x, size_t z, uint8_t altitude) { altitudes.at(x * 17 + z) = altitude; };
	set(0, 0, 10);
	set(0, 2, 21);
	set(2, 0, 30);
	set(2, 2, 41);
	set(0, 4, 50);
	set(4, 0, 60);
	set(4, 4, 70);
	EXPECT_EQ(TargetsAt(altitudes, 0, 1).half, 15); // along z, rounded down
	EXPECT_EQ(TargetsAt(altitudes, 1, 0).half, 20); // along x
	EXPECT_EQ(TargetsAt(altitudes, 1, 1).half, 25); // along the diagonal
	EXPECT_EQ(TargetsAt(altitudes, 2, 2).half, 41); // a half mesh corner keeps its own
	EXPECT_EQ(TargetsAt(altitudes, 0, 2).quarter, 30);
	EXPECT_EQ(TargetsAt(altitudes, 2, 0).quarter, 35);
	EXPECT_EQ(TargetsAt(altitudes, 2, 2).quarter, 40);
	EXPECT_EQ(TargetsAt(altitudes, 4, 4).quarter, 70);
	EXPECT_EQ(TargetsAt(altitudes, 1, 1).quarter, 0); // only the half mesh's corners morph further
}

TEST(LandDetail, MorphBlendsAcrossTheBandIn256ths)
{
	EXPECT_EQ(MorphedAltitude(100, 200, 25.0f, 25.0f), 100);
	EXPECT_EQ(MorphedAltitude(100, 200, -25.0f, 25.0f), 200);
	EXPECT_EQ(MorphedAltitude(100, 200, 0.0f, 25.0f), 150);
	// A quarter of the way: 64/256
	EXPECT_EQ(MorphedAltitude(100, 201, 12.5f, 25.0f), (192 * 100 + 64 * 201) >> 8);
}

TEST(LandDetail, CoarseTextureFadesInAcrossTheBand)
{
	EXPECT_EQ(CrossFadeAlpha(36.0f, 35.0f), 0);
	EXPECT_EQ(CrossFadeAlpha(35.0f, 35.0f), 0);
	EXPECT_EQ(CrossFadeAlpha(0.0f, 35.0f), 128);
	EXPECT_EQ(CrossFadeAlpha(-34.0f, 35.0f), 251);
	EXPECT_EQ(CrossFadeAlpha(-35.0f, 35.0f), 255);
}

TEST(LandDetail, CoarseTextureSamplesTheBlockTexture)
{
	EXPECT_EQ(CoarseSourceTexel(1), 0);
	EXPECT_EQ(CoarseSourceTexel(2), 4);
	EXPECT_EQ(CoarseSourceTexel(32), 124);
	EXPECT_EQ(CoarseSourceTexel(62), 248);
	EXPECT_NEAR(CoarseTexelPosition(0.0f), 1.0039f, 1e-4f);
	EXPECT_NEAR(CoarseTexelPosition(1.0f), 63.0039f, 1e-4f);
}

namespace
{

/// A page as the landscape file keeps it: its surface description, then its DXT3 blocks
lnd::LNDLowResolutionTexture FakePage(uint32_t side, const char* fourCc)
{
	constexpr size_t k_Description = 124;
	lnd::LNDLowResolutionTexture page {};
	page.texels.assign(k_Description + static_cast<size_t>(side / 4) * (side / 4) * 16, 0);
	const auto write32 = [&](size_t at, uint32_t value) {
		for (size_t i = 0; i < 4; ++i)
		{
			page.texels[at + i] = static_cast<uint8_t>(value >> (8 * i));
		}
	};
	write32(0, k_Description);
	write32(8, side);
	write32(12, side);
	std::copy_n(fourCc, 4, page.texels.begin() + 80);
	page.texels[k_Description] = 0xAB;
	return page;
}

} // namespace

TEST(LandDetail, CoarsePagesAreReadFromTheirSurfaceDescriptions)
{
	const auto page = FakePage(256, "DXT3");
	const auto texels = lnd::GetTexels(page);
	ASSERT_TRUE(texels.has_value());
	EXPECT_EQ(texels->width, 256);
	EXPECT_EQ(texels->height, 256);
	EXPECT_EQ(texels->blocks.size(), 64u * 64u * 16u);
	EXPECT_EQ(texels->blocks.front(), 0xAB);

	EXPECT_FALSE(lnd::GetTexels(FakePage(256, "DXT1")).has_value());
	auto cut = page;
	cut.texels.resize(cut.texels.size() - 1);
	EXPECT_FALSE(lnd::GetTexels(cut).has_value());
}
