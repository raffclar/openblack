/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The worship site's mesh has no skin of its own: it wears the first skin of its temple's mesh.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Common/Zip.h"
#include "Graphics/WorldTriangles.h"
#include "support/BgfxShutdown.h"

using namespace openblack;

namespace
{
/// A compressed mesh of Data/Citadel/OutsideMeshes inflated (a u32 of its inflated size, then the deflated bytes);
/// empty when it cannot be read
std::vector<uint8_t> ReadOutsideMesh(const std::filesystem::path& game, const char* file)
{
	std::ifstream stream(game / "Data" / "Citadel" / "OutsideMeshes" / file, std::ios::binary);
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	uint32_t size = 0;
	if (bytes.size() < sizeof(size))
	{
		return {};
	}
	std::memcpy(&size, bytes.data(), sizeof(size));
	return zip::Inflate(std::vector<uint8_t>(bytes.begin() + sizeof(size), bytes.end()), size);
}
} // namespace

/// With OPENBLACK_GAME_PATH set to the install: B_WORSHIP's materials name a skin it does not carry; once its skin
/// source is the first temple's mesh and every primitive takes that mesh's first skin, each primitive draws with the
/// temple's own texture, the one the alignment blend rewrites
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(WorshipSiteSkin, everyPrimitiveWearsTheTemplesFirstSkin)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto* name : {"game", "graphics"})
	{
		if (spdlog::get(name) == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>(name);
		}
	}
	const auto templeBytes = ReadOutsideMesh(game, "b_first_temple_l3d.zzz");
	const auto siteBytes = ReadOutsideMesh(game, "B_WORSHIP_l3d.zzz");
	ASSERT_FALSE(templeBytes.empty());
	ASSERT_FALSE(siteBytes.empty());
	// the sub-meshes and skins build bgfx buffers and textures
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		auto temple = std::make_shared<graphics::L3DMesh>("b_first_temple");
		ASSERT_TRUE(temple->LoadFromBuffer(templeBytes));
		graphics::L3DMesh site("B_WORSHIP");
		ASSERT_TRUE(site.LoadFromBuffer(siteBytes));

		// the temple carries one skin; the site none, and its materials ask for skin 1
		const auto skin = temple->GetFirstSkin();
		ASSERT_TRUE(skin.has_value());
		EXPECT_EQ(*skin, 0xFC0B7D34u);
		EXPECT_FALSE(site.GetFirstSkin().has_value());
		EXPECT_TRUE(site.GetSkins().empty());
		ASSERT_EQ(site.GetNumSubMeshes(), 2);
		EXPECT_EQ(site.GetSubMeshes()[0]->GetPrimitives().at(0).skinID, 1u);

		site.SetSkinSource(temple);
		site.SetSkin(*skin);

		EXPECT_EQ(site.GetFirstSkin(), skin);
		const auto* templeTexture = temple->GetSkins().at(*skin).get();
		for (uint8_t i = 0; i < site.GetNumSubMeshes(); ++i)
		{
			// every sub-mesh, the physics one included, as the original sets every material's texture
			for (const auto& primitive : site.GetSubMeshes()[i]->GetPrimitives())
			{
				EXPECT_EQ(primitive.skinID, *skin) << "sub-mesh " << static_cast<int>(i);
				EXPECT_EQ(graphics::world_triangles::PrimitiveTexture(site, primitive.skinID), templeTexture)
				    << "sub-mesh " << static_cast<int>(i);
			}
		}
	}
}
