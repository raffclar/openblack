/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ParticleFile.h>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Particles/LightSheet.h"
#include "Particles/ObjectLightSheet.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"

using namespace openblack;
using namespace openblack::particles;
namespace rules = openblack::particles::object_light_sheet;

namespace
{
constexpr float k_Step = 0.1f;

TEST(ObjectLightSheetRules, AnAtomTakesItsEffectsPlayersColourOrGreyWithoutAPlayer)
{
	EXPECT_EQ(rules::AtomArgb(0xFF4646u), 0xFFFF4646u);
	EXPECT_EQ(rules::AtomArgb(0xAA4777FFu), 0xFF4777FFu);
	EXPECT_EQ(rules::AtomArgb(std::nullopt), 0xFF777777u);
}

TEST(ObjectLightSheetRules, TheRingIsEachByteOfTheAtomsColourTimesItsAlphaOver256RoundedDown)
{
	EXPECT_EQ(rules::RingArgb(0xFFFF4646u, 255), 0xFEFE4545u);
	EXPECT_EQ(rules::RingArgb(0xFFFF4646u, 30), 0x1D1D0808u);
	EXPECT_EQ(rules::RingArgb(0xFF777777u, 30), 0x1D0D0D0Du);
	EXPECT_EQ(rules::RingArgb(0xFFFFFFFFu, 0), 0u);
	EXPECT_EQ(rules::RingArgb(0x80402010u, 128), 0x40201008u);
}

TEST(ObjectLightSheetRules, ARingStandsOnTheLandRoundItsCentreAndCloses)
{
	const auto points =
	    LightSheet::RingPoints({100.0f, 200.0f}, 15.0f, LightSheet::k_RingPoints, [](glm::vec2 at) { return at.y * 0.5f; });
	ASSERT_EQ(points.size(), 32u);
	EXPECT_FLOAT_EQ(points.front().x, 115.0f);
	EXPECT_FLOAT_EQ(points.front().z, 200.0f);
	EXPECT_FLOAT_EQ(points.front().y, 100.0f);
	EXPECT_NEAR(points.back().x, points.front().x, 1e-3f);
	EXPECT_NEAR(points.back().z, points.front().z, 1e-3f);
	for (const auto& point : points)
	{
		EXPECT_NEAR(std::hypot(point.x - 100.0f, point.z - 200.0f), 15.0f, 1e-3f);
		EXPECT_FLOAT_EQ(point.y, point.z * 0.5f);
	}
}

/// A land that slopes along x, players of one colour, and a list of the sheets of light the rules stand
class SheetWorld final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 xz) const override { return xz.x * 0.1f; }
	[[nodiscard]] uint32_t PlayerColour(int /*player*/) const override { return 0xFF4646u; }
	[[nodiscard]] glm::vec3 CameraRight() const override { return {1.0f, 0.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraUp() const override { return {0.0f, 1.0f, 0.0f}; }
	void StartSound(const Effect& /*effect*/, const std::shared_ptr<ParticleSoundLink>& /*sound*/) override {}
	void AddLightSheet(const std::shared_ptr<LightSheet>& sheet) override { sheets.push_back(sheet); }

	[[nodiscard]] std::vector<std::shared_ptr<LightSheet>> Live() const
	{
		std::vector<std::shared_ptr<LightSheet>> live;
		for (const auto& sheet : sheets)
		{
			if (auto locked = sheet.lock())
			{
				live.push_back(std::move(locked));
			}
		}
		return live;
	}

	std::vector<std::weak_ptr<LightSheet>> sheets;
};

class SheetRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t /*n*/, DrawSite /*site*/) override { return 0; }
	float SyncedFloatRand(float /*x*/, DrawSite /*site*/) override { return 0.0f; }
	uint32_t LocalRand(int32_t /*n*/) override { return 0; }
	float LocalFloatRand(float /*x*/) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds /*seeds*/) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) override {}
};

/// The ring part of the singing stones' heal as the game ships it, with the rule that ends it a while after the effect
/// closes down
std::string HealRing(std::string_view radius)
{
	return "BEGINPROPERTIES\n"
	       "PROPERTY DeleteOnCloseDown BOOL 0\n"
	       "PROPERTY Hierarchies ARRAY SIZE 2 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 2 1 0\n"
	       "PROPERTY MaxSpellAge FLOAT -1\n"
	       "ENDPROPERTIES\n"
	       "BEGINCLASS MagnitudeFloatProvider MagnitudeFloatProvider0\n"
	       "BEGINPROPERTIES\n"
	       "PROPERTY Maximum FLOAT 1000\n"
	       "PROPERTY Minimum FLOAT 0\n"
	       "PROPERTY ScaleBy FLOAT 1.11062\n"
	       "ENDPROPERTIES\n"
	       "ENDCLASS\n"
	       "BEGINCLASS UR_LightSheetOnObject UR_LightSheetOnObject0\n"
	       "BEGINPROPERTIES\n"
	       "PROPERTY Condition PERSIS_PNTR NULL_STRING\n"
	       "PROPERTY Group INTEGER 0\n"
	       "PROPERTY NextGroups ARRAY SIZE 0 \n"
	       "PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0\n"
	       "PROPERTY RemoveOnCloseDown BOOL 0\n"
	       "PROPERTY RadiusFP PERSIS_PNTR " +
	       std::string(radius) +
	       "\n"
	       "PROPERTY NumAtoms INTEGER 3\n"
	       "ENDPROPERTIES\n"
	       "ENDCLASS\n"
	       "BEGINCLASS ParticlePointCreator ParticlePointCreator0\n"
	       "BEGINPROPERTIES\n"
	       "PROPERTY ColorA INTEGER 30\n"
	       "PROPERTY ColorB INTEGER 255\n"
	       "PROPERTY ColorG INTEGER 255\n"
	       "PROPERTY ColorR INTEGER 255\n"
	       "PROPERTY InitialScale FLOAT 1\n"
	       "ENDPROPERTIES\n"
	       "ENDCLASS\n"
	       "BEGINCLASS RemoveRuleAfterCloseDown RemoveRuleAfterCloseDown0\n"
	       "BEGINPROPERTIES\n"
	       "PROPERTY Condition PERSIS_PNTR NULL_STRING\n"
	       "PROPERTY Delay FLOAT 2.2\n"
	       "PROPERTY Group INTEGER 0\n"
	       "PROPERTY RemoveOnCloseDown BOOL 0\n"
	       "ENDPROPERTIES\n"
	       "ENDCLASS\n";
}

class ObjectLightSheetTest: public ::testing::Test
{
protected:
	std::unique_ptr<Effect> Make(std::string_view radius = "MagnitudeFloatProvider0", float magnitude = 1.0f)
	{
		auto file = psys::ParticleFile::Parse(HealRing(radius));
		EXPECT_TRUE(file.has_value());
		return std::make_unique<Effect>(std::make_shared<const psys::ParticleFile>(std::move(*file)),
		                                EffectServices {classes, world, random, noise}, glm::vec3(100.0f, 50.0f, 200.0f),
		                                magnitude, false);
	}

	ParticleClassRegistry classes {ParticleClassRegistry::WithAllClasses()};
	SheetWorld world;
	SheetRandom random;
	maths::ValueNoise noise;
};

TEST_F(ObjectLightSheetTest, ItsAtomsAreMadeOnceEachWithARingOnTheLandAsWideAsTheMagnitudeGives)
{
	auto effect = Make();
	EXPECT_TRUE(effect->UnportedClasses().empty());
	effect->SetPlayer(0);
	// The script makes the heal 13.5 times its size before its first step
	effect->SetMagnitude(13.5f);
	for (int i = 0; i < 5; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_EQ(effect->AtomCount(), 3u);
	const auto sheets = world.Live();
	ASSERT_EQ(sheets.size(), 3u);
	const float radius = 13.5f * 1.11062f;
	for (const auto& sheet : sheets)
	{
		ASSERT_EQ(sheet->Points(), 32u);
		EXPECT_FALSE(sheet->Pulsed());
		std::vector<LightSheet::Vertex> vertices;
		std::vector<uint32_t> triangles;
		sheet->Build(vertices, triangles);
		// The bottom corner of each point is on the land, round the effect's place
		for (size_t i = 0; i < vertices.size(); i += 3)
		{
			const auto& ground = vertices[i].position;
			EXPECT_NEAR(std::hypot(ground.x - 100.0f, ground.z - 200.0f), radius, 1e-3f);
			EXPECT_FLOAT_EQ(ground.y, ground.x * 0.1f);
		}
	}
}

TEST_F(ObjectLightSheetTest, TheRingIsThePlayersColourAsDimAsItsAtomIsClear)
{
	// Red over the point creator's alpha of 30
	auto effect = Make();
	effect->SetPlayer(0);
	effect->Step(k_Step);
	auto sheets = world.Live();
	ASSERT_EQ(sheets.size(), 3u);
	for (const auto& sheet : sheets)
	{
		EXPECT_EQ(sheet->Rgb(), 0x1D0808u);
	}
	// An effect with no player gives grey
	world.sheets.clear();
	auto grey = Make();
	grey->Step(k_Step);
	sheets = world.Live();
	ASSERT_EQ(sheets.size(), 3u);
	EXPECT_EQ(sheets.front()->Rgb(), 0x0D0D0Du);
	// The ring follows its atom's alpha at each step, and keeps the colour its atom was made with
	grey->SetPlayer(0);
	grey->Step(k_Step);
	EXPECT_EQ(sheets.front()->Rgb(), 0x0D0D0Du);
}

TEST_F(ObjectLightSheetTest, WithoutAFloatValueTheRingTakesTheDefaultRadius)
{
	auto effect = Make("NULL_STRING", 13.5f);
	effect->Step(k_Step);
	const auto sheets = world.Live();
	ASSERT_EQ(sheets.size(), 3u);
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheets.front()->Build(vertices, triangles);
	ASSERT_FALSE(vertices.empty());
	EXPECT_NEAR(std::hypot(vertices[0].position.x - 100.0f, vertices[0].position.z - 200.0f), 20.0f, 1e-3f);
}

TEST_F(ObjectLightSheetTest, TheRingsGoWithTheirAtomsAfterTheEffectClosesDown)
{
	auto effect = Make();
	effect->Step(k_Step);
	EXPECT_EQ(world.Live().size(), 3u);
	effect->CloseDown();
	// The atoms stay for the delay after the close down, then go with their rings
	for (int i = 0; i < 10; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_EQ(world.Live().size(), 3u);
	for (int i = 0; i < 20; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_EQ(effect->AtomCount(), 0u);
	EXPECT_TRUE(world.Live().empty());
	effect.reset();
	EXPECT_TRUE(world.Live().empty());
}
} // namespace
