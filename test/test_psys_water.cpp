/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UpdateRuleGravityWithFloor (Particles/Rules/Fireball.cpp, its update and impact) on a flat
// hand-made island: an atom that falls on a water cell bounces and leaves the particle ripple (growth 4 x radius, cell
// 0x30) only when the rule has an ImpactSound; on dry land it bounces without a ring.
// The rule reads the ImpactSound by its SoundAction.h value (psys::ReadSoundAction: an unknown name is -1, like
// NO_SOUND), so the tests that need a real sound read Data\SoundAction.h from OPENBLACK_GAME_PATH (or
// OPENBLACK_TEST_GAME_PATH) and are skipped without it. Their Synthetic twins put a small hand-written SoundAction.h
// into the resource caches instead, so the same checks also run without the game.

#include <cstdint>
#include <cstdlib>

#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "ECS/WaterRings.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/SoundAction.h"

// a custom locator: the filesystem for Data\SoundAction.h (as test_camera does)
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// One block of 16 x 16 cells, flat at height 0
class FlatIsland final: public LandIslandInterface
{
public:
	explicit FlatIsland(bool water)
	    : _cells(16 * 16)
	{
		for (auto& cell : _cells)
		{
			cell.altitude = water ? 0 : 40;
			cell.properties.hasWater = water ? 1 : 0;
		}
	}
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& cell) const final
	{
		static const lnd::LNDCell k_Empty {};
		return HasBlockAt(cell) ? _cells[static_cast<size_t>(cell.x) * 16 + cell.y] : k_Empty;
	}
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2& cell) const final { return cell.x < 16 && cell.y < 16; }
	[[nodiscard]] uint16_t GetCellsPerSide() const final { return 16; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::FrameBuffer& GetStaticShadowFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const final { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const final { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	std::vector<lnd::LNDCell> _cells;
	std::vector<lnd::LNDCountry> _countries;
};

std::string SpellFile(const char* impactSound)
{
	std::string zeros;
	for (int i = 1; i < 25; ++i)
	{
		zeros += " 0";
	}
	std::string text = "BEGINPROPERTIES\n"
	                   "PROPERTY DeleteOnCloseDown BOOL 1\n"
	                   "PROPERTY Hierarchies ARRAY SIZE 25 0" +
	                   zeros +
	                   "\n"
	                   "PROPERTY InitiallyCreated ARRAY SIZE 25 1" +
	                   zeros +
	                   "\n"
	                   "PROPERTY MaxSpellAge FLOAT 25\n"
	                   "ENDPROPERTIES\n"
	                   "BEGINCLASS ParticlePointCreator Point\nBEGINPROPERTIES\n"
	                   "PROPERTY InitialScale FLOAT 1.5\n"
	                   "ENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS CreateRuleAnAtom Create\nBEGINPROPERTIES\n"
	                   "PROPERTY Group INTEGER 0\n"
	                   "PROPERTY NextGroups ARRAY SIZE 0\n"
	                   "PROPERTY OffsetY FLOAT 3\n"
	                   "PROPERTY PCreator PERSIS_PNTR Point\n"
	                   "ENDPROPERTIES\nENDCLASS\n"
	                   "BEGINCLASS UpdateRuleGravityWithFloor Floor\nBEGINPROPERTIES\n"
	                   "PROPERTY Condition PERSIS_PNTR NULL_STRING\n"
	                   "PROPERTY DampingHorozontalBounce FLOAT 0.9\n"
	                   "PROPERTY DampingVerticalBounce FLOAT 0.5\n"
	                   "PROPERTY Gravity FLOAT 30\n"
	                   "PROPERTY Group INTEGER 0\n"
	                   "PROPERTY ImpactSound SOUND_ACTION ";
	text += impactSound;
	text += " LOOPING 0 ONLYONE 0 SOFTRELEASE 0 USESURFACE 0\n"
	        "PROPERTY ImpactSoundCondition PERSIS_PNTR NULL_STRING\n"
	        "PROPERTY ImpactSpeedSmall FLOAT 0\n"
	        "PROPERTY MaxSpeed FLOAT 100\n"
	        "PROPERTY UseWind BOOL 0\n"
	        "ENDPROPERTIES\nENDCLASS\n";
	return text;
}

struct Result
{
	std::vector<ecs::WaterRing> rings;
};

/// The sound action names (psys::SoundActionValue reads Data\SoundAction.h once, through Locator::filesystem)
bool HaveSoundActions()
{
	if (!Locator::filesystem::has_value())
	{
		const char* game = std::getenv("OPENBLACK_GAME_PATH");
		if (game == nullptr)
		{
			game = std::getenv("OPENBLACK_TEST_GAME_PATH");
		}
		if (game == nullptr)
		{
			return false;
		}
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::filesystem::value().SetGamePath(game);
	}
	return psys::SoundActionValue("SOUND_SPELL_FIREBALL_HIT") != -1;
}

/// A tiny SoundAction.h with one impact sound, and the value it gives that sound
constexpr std::string_view k_SyntheticSoundActions = "enum LHSoundAction\n{\n\tNO_SOUND_ACTION = 0,\n"
                                                     "\tSOUND_SPELL_FIREBALL_HIT = 42,\n};\n";
constexpr int32_t k_SyntheticImpactSound = 42;

/// Puts the synthetic SoundAction.h into a fresh resource cache, under the path the sound actions are read from, so
/// no file is opened. The previous file system and resources come back when it goes
class SyntheticSoundActions
{
public:
	SyntheticSoundActions()
	{
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::resources::emplace<resources::Resources>();
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "SoundAction.h";
		Locator::resources::value().GetBlobs().Load(
		    resources::BlobId(path), resources::BlobLoader::FromBufferTag {},
		    std::vector<uint8_t>(k_SyntheticSoundActions.begin(), k_SyntheticSoundActions.end()));
	}

private:
	// built before the constructor's body, so they keep the services that were there before
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;
	const test::RestoreService<Locator::resources> _restoreResources;
};

Result Drop(bool water, const char* impactSound)
{
	Locator::terrainSystem::emplace<FlatIsland>(water);
	ecs::UpdateWaterRings(100000.0f); // empty the pool
	auto file = psys::File::Parse(SpellFile(impactSound), "test");
	EXPECT_TRUE(file.has_value());
	psys::Effect effect(std::make_shared<const psys::File>(std::move(*file)), glm::vec3(85.0f, 0.0f, 85.0f), 1.0f);
	Result result;
	for (int i = 0; i < 20; ++i)
	{
		effect.Step(0.1f);
		for (const auto& ring : ecs::GetWaterRings())
		{
			result.rings.push_back(ring);
		}
		ecs::UpdateWaterRings(100000.0f);
	}
	Locator::terrainSystem::reset();
	return result;
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_GAME_PATH or OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(PSysWater, FloorRippleOnWater)
{
	if (!HaveSoundActions())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set (Data\\SoundAction.h)";
	}
	const auto result = Drop(true, "SOUND_SPELL_FIREBALL_HIT");
	ASSERT_FALSE(result.rings.empty());
	const auto& ring = result.rings.front();
	EXPECT_FLOAT_EQ(ring.growth, 4.0f * 1.5f); // 4 x the atom's radius (InitialScale 1.5)
	EXPECT_EQ(ring.cell, 0x30);
	EXPECT_EQ(ring.argb, 0xFFFFFFFFu);
	EXPECT_FLOAT_EQ(ring.rate, 1.0f);
	EXPECT_FLOAT_EQ(ring.aspect, 1.0f);
	EXPECT_FLOAT_EQ(ring.position.x, 85.0f);
	EXPECT_FLOAT_EQ(ring.position.y, 0.0f); // put back on the ground
	EXPECT_FLOAT_EQ(ring.position.z, 85.0f);
	// the atom bounces in place: every later impact is closer than 2 units to the first ripple
	EXPECT_EQ(result.rings.size(), 1u);
}

/// FloorRippleOnWater with the synthetic SoundAction.h: the ring comes from the spell file above (InitialScale 1.5)
/// and the drop point, not from the game data
TEST(PSysWater, FloorRippleOnWaterSynthetic)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(psys::SoundActionValue("SOUND_SPELL_FIREBALL_HIT"), k_SyntheticImpactSound);
	const auto result = Drop(true, "SOUND_SPELL_FIREBALL_HIT");
	ASSERT_FALSE(result.rings.empty());
	const auto& ring = result.rings.front();
	EXPECT_FLOAT_EQ(ring.growth, 4.0f * 1.5f); // 4 x the atom's radius
	EXPECT_EQ(ring.cell, 0x30);
	EXPECT_EQ(ring.argb, 0xFFFFFFFFu);
	EXPECT_FLOAT_EQ(ring.rate, 1.0f);
	EXPECT_FLOAT_EQ(ring.aspect, 1.0f);
	EXPECT_FLOAT_EQ(ring.position.x, 85.0f);
	EXPECT_FLOAT_EQ(ring.position.y, 0.0f); // put back on the ground
	EXPECT_FLOAT_EQ(ring.position.z, 85.0f);
	// the atom bounces in place, so the later impacts are too close to the first ripple to make another
	EXPECT_EQ(result.rings.size(), 1u);
	// a name the header does not have reads as no sound: no ripple
	EXPECT_EQ(psys::SoundActionValue("SOUND_NOT_IN_THE_HEADER"), -1);
	EXPECT_TRUE(Drop(true, "SOUND_NOT_IN_THE_HEADER").rings.empty());
}

TEST(PSysWater, NoRippleWithoutImpactSound)
{
	// SF_ExplodeObject's fragments: ImpactSound NO_SOUND (-1) -> the impact returns before the ripple
	EXPECT_TRUE(Drop(true, "NO_SOUND").rings.empty());
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH or OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(PSysWater, NoRippleOnDryLand)
{
	// the cell is not water
	if (!HaveSoundActions())
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set (Data\\SoundAction.h)";
	}
	EXPECT_TRUE(Drop(false, "SOUND_SPELL_FIREBALL_HIT").rings.empty());
}

/// NoRippleOnDryLand with the synthetic SoundAction.h: a real impact sound, but the cells have no water
TEST(PSysWater, NoRippleOnDryLandSynthetic)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(psys::SoundActionValue("SOUND_SPELL_FIREBALL_HIT"), k_SyntheticImpactSound);
	EXPECT_TRUE(Drop(false, "SOUND_SPELL_FIREBALL_HIT").rings.empty());
	// the same drop on water does ripple, so the dry land is what stops it
	EXPECT_FALSE(Drop(true, "SOUND_SPELL_FIREBALL_HIT").rings.empty());
}
