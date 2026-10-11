/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <ParticleFile.h>
#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleEffect.h"
#include "Particles/VolumeGlow.h"

using namespace openblack;
using namespace openblack::particles;
namespace volume_glow = openblack::particles::volume_glow;

namespace
{
constexpr float k_Epsilon = 1e-5f;
constexpr float k_Step = 0.1f;

TEST(VolumeGlowRules, StrengthIsThePowerClampedToOne)
{
	EXPECT_EQ(volume_glow::Strength(-1.0f), 0.0f);
	EXPECT_EQ(volume_glow::Strength(0.0f), 0.0f);
	EXPECT_EQ(volume_glow::Strength(0.4f), 0.4f);
	EXPECT_EQ(volume_glow::Strength(1.5f), 1.0f);
	EXPECT_EQ(volume_glow::Strength(std::nanf("")), 0.0f);
}

TEST(VolumeGlowRules, ReachGrowsWithStrengthUpToTheMostThenStops)
{
	// An artefact's shell: from a tenth to eight tenths of its half diagonal
	const volume_glow::Settings artefact {.zoomMin = 0.1f, .zoomMax = 0.8f};
	EXPECT_NEAR(volume_glow::Zoom(artefact, 0.0f, 1.0f), 0.1f, k_Epsilon);
	EXPECT_NEAR(volume_glow::Zoom(artefact, 0.5f, 1.0f), 0.45f, k_Epsilon);
	// An artefact glows only once worth more than 1, so its shell always reaches the furthest, whatever its worth
	EXPECT_NEAR(volume_glow::Zoom(artefact, 1.5f, 1.0f), 0.8f, k_Epsilon);
	EXPECT_NEAR(volume_glow::Zoom(artefact, 30.0f, 1.0f), 0.8f, k_Epsilon);
	// Times the scale of the atom kept on the object
	EXPECT_NEAR(volume_glow::Zoom(artefact, 1.0f, 2.0f), 1.6f, k_Epsilon);
	// One whose strength sets its alpha keeps the least reach, whatever the atom's scale
	auto fading = artefact;
	fading.alphaFromStrength = true;
	EXPECT_NEAR(volume_glow::Zoom(fading, 1.0f, 2.0f), 0.1f, k_Epsilon);
}

TEST(VolumeGlowRules, AlphaIsTheRulesOrScaledByStrengthRoundedDown)
{
	volume_glow::Settings settings {.alpha = 207};
	EXPECT_EQ(volume_glow::Alpha(settings, 0.3f), 207u);
	settings.alphaFromStrength = true;
	EXPECT_EQ(volume_glow::Alpha(settings, 0.5f), 103u);
	EXPECT_EQ(volume_glow::Alpha(settings, 2.0f), 207u);
}

TEST(VolumeGlowRules, ColourIsThePlayersWhenAskedAndFadesWithItsCollection)
{
	const volume_glow::Settings players {.alpha = 207, .usePlayerColour = true};
	EXPECT_EQ(volume_glow::Argb(players, 207, 255, 0xFF4646u), 0xCFFF4646u);
	// A collection half faded: whole steps rounded down
	EXPECT_EQ(volume_glow::Argb(players, 207, 128, 0xFF4646u), (103u << 24u) | 0xFF4646u);
	// White without a player, or for a rule that doesn't take one
	EXPECT_EQ(volume_glow::Argb(players, 207, 255, std::nullopt), 0xCFFFFFFFu);
	const volume_glow::Settings white {.alpha = 207};
	EXPECT_EQ(volume_glow::Argb(white, 207, 255, 0xFF4646u), 0xCFFFFFFFu);
}

TEST(VolumeGlowRules, TheTextureSlidesByTheGamesMillisecondsAndWraps)
{
	const auto offset = volume_glow::UvOffset({0.3f, 0.31f}, 5000);
	EXPECT_NEAR(offset.x, 0.5f, 1e-4f);
	EXPECT_NEAR(offset.y, 0.55f, 1e-4f);
	const auto back = volume_glow::UvOffset({-0.3f, 0.0f}, 5000);
	EXPECT_NEAR(back.x, -0.5f, 1e-4f);
	EXPECT_EQ(volume_glow::SinceStarted(7000, 2000), 5000u);
	EXPECT_EQ(volume_glow::SinceStarted(1000, 2000), 0u);
}

TEST(VolumeGlowRules, TheGamesTimeIsATenthOfASecondATurnAndTheMillisecondsIntoTheNext)
{
	EXPECT_EQ(volume_glow::GameMilliseconds(0, 0.0f), 0u);
	EXPECT_EQ(volume_glow::GameMilliseconds(25, 0.0f), 2500u);
	EXPECT_EQ(volume_glow::GameMilliseconds(25, 0.456f), 2545u);
	// Never past 99 into a turn
	EXPECT_EQ(volume_glow::GameMilliseconds(25, 1.0f), 2599u);
}

TEST(VolumeGlowRules, TheViewNormalIsWidenedByTheProjectionThenEachAxisMadeOneLong)
{
	const std::array<glm::vec3, 3> level {glm::vec3 {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
	// Axes along the camera's own are left as they are
	const auto side = volume_glow::ViewNormal({1.0f, 0.0f, 0.0f}, level, {1.43f, 1.79f});
	EXPECT_NEAR(side.x, 1.0f, k_Epsilon);
	EXPECT_NEAR(side.y, 0.0f, k_Epsilon);
	// An axis half towards the camera leans across once widened
	const std::array<glm::vec3, 3> turned {glm::vec3 {0.6f, 0.0f, 0.8f}, {0.0f, 1.0f, 0.0f}, {-0.8f, 0.0f, 0.6f}};
	const auto leaning = volume_glow::ViewNormal({1.0f, 0.0f, 0.0f}, turned, {2.0f, 2.0f});
	EXPECT_NEAR(leaning.x, 1.2f / std::sqrt(1.44f + 0.64f), k_Epsilon);
	EXPECT_NEAR(leaning.z, 0.8f / std::sqrt(1.44f + 0.64f), k_Epsilon);
}

TEST(VolumeGlowRules, TheInnerEdgeGlowsTheMoreSideOnItIsSeen)
{
	// Facing the camera: dark
	EXPECT_EQ(volume_glow::EdgeLevel({0.0f, 0.0f}, false), 0u);
	// Wholly side on, a model without bones reaches only a little of the colour
	EXPECT_EQ(volume_glow::EdgeLevel({1.0f, 0.0f}, false), 24u);
	EXPECT_EQ(volume_glow::EdgeLevel({0.7071f, 0.7071f}, false), 24u);
	// A normal widened by the projection can lean further
	EXPECT_EQ(volume_glow::EdgeLevel({1.2f, 0.6f}, false), 125u);
	// A model moved by bones measures side on by the sum of the two
	EXPECT_EQ(volume_glow::EdgeLevel({0.7071f, 0.7071f}, true), 151u);
	EXPECT_EQ(volume_glow::EdgeLevel({1.5f, 1.5f}, true), 255u);
	EXPECT_EQ(volume_glow::EdgeLevel({0.5f, 0.0f}, true), 0u);
}

TEST(VolumeGlowRules, TheInnerColourScalesItsChannelsAsWholeBytesKeepingItsAlpha)
{
	EXPECT_EQ(volume_glow::InnerArgb(0xCFFF4646u, 255), 0xCFFF4646u);
	EXPECT_EQ(volume_glow::InnerArgb(0xCFFF4646u, 24), 0xCF170606u);
	EXPECT_EQ(volume_glow::InnerArgb(0xCFFF4646u, 0), 0xCF000000u);
}

TEST(VolumeGlowRules, TextureCoordinatesRunAcrossByTheNormalAndBackToTheOuterEdge)
{
	const auto inner = volume_glow::TextureCoordinates(0.5f, {0.25f, 0.4f}, {0.7f, 0.2f}, false);
	EXPECT_NEAR(inner.x, 0.6f, k_Epsilon);
	EXPECT_NEAR(inner.y, 0.4f, k_Epsilon);
	const auto outer = volume_glow::TextureCoordinates(0.5f, {0.25f, 0.4f}, {0.7f, 0.2f}, true);
	EXPECT_NEAR(outer.x, 0.6f, k_Epsilon);
	EXPECT_NEAR(outer.y, 0.2f, k_Epsilon);
}

TEST(VolumeGlowRules, EachEdgeOfEachTriangleBecomesABandOfTwoTriangles)
{
	const std::vector<uint16_t> triangle {0, 1, 2};
	const auto bands = volume_glow::ShellTriangles(triangle);
	const std::vector<uint32_t> expected {0, 1, 3, 3, 2, 0, 2, 3, 5, 5, 4, 2, 4, 5, 1, 1, 0, 4};
	EXPECT_EQ(bands, expected);
	const std::vector<uint16_t> two {0, 1, 2, 2, 1, 3};
	EXPECT_EQ(volume_glow::ShellTriangles(two).size(), 36u);
	EXPECT_TRUE(volume_glow::ShellTriangles(std::vector<uint16_t> {0, 1}).empty());
}

/// Objects standing where they are told, a red player
class GlowWorld final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return 0.0f; }
	[[nodiscard]] uint32_t PlayerColour(int /*player*/) const override { return 0xFF4646u; }
	[[nodiscard]] glm::vec3 CameraRight() const override { return {1.0f, 0.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraUp() const override { return {0.0f, 1.0f, 0.0f}; }
	void StartSound(const Effect& /*effect*/, const std::shared_ptr<ParticleSoundLink>& /*sound*/) override {}
	[[nodiscard]] uint64_t GameMilliseconds() const override { return gameMilliseconds; }
	[[nodiscard]] std::optional<glm::vec3> ObjectPosition(entt::entity object) const override
	{
		if (const auto found = objects.find(object); found != objects.end())
		{
			return found->second;
		}
		return std::nullopt;
	}

	std::map<entt::entity, glm::vec3> objects;
	uint64_t gameMilliseconds {0};
};

class GlowRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t /*n*/) override { return 0; }
	float GameFloatRand(float /*x*/) override { return 0.0f; }
	uint32_t LocalRand(int32_t /*n*/) override { return 0; }
	float LocalFloatRand(float /*x*/) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds /*seeds*/) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) override {}
};

/// The artefact's magic file as the game ships it, without its fading rules
constexpr std::string_view k_ArtefactGlow = "BEGINPROPERTIES\n"
                                            "PROPERTY DeleteOnCloseDown BOOL 0\n"
                                            "PROPERTY Hierarchies ARRAY SIZE 2 0 0\n"
                                            "PROPERTY InitiallyCreated ARRAY SIZE 2 1 0\n"
                                            "PROPERTY MaxSpellAge FLOAT -1\n"
                                            "ENDPROPERTIES\n"
                                            "BEGINCLASS UR_VolFXOnObject UR_VolFXOnObject0\n"
                                            "BEGINPROPERTIES\n"
                                            "PROPERTY Alpha INTEGER 207\n"
                                            "PROPERTY AnimSpeedU FLOAT 0.3\n"
                                            "PROPERTY AnimSpeedV FLOAT 0.31\n"
                                            "PROPERTY Group INTEGER 0\n"
                                            "PROPERTY MatInd INTEGER 2\n"
                                            "PROPERTY NextGroups ARRAY SIZE 0 \n"
                                            "PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0\n"
                                            "PROPERTY TextFracU FLOAT 0.7\n"
                                            "PROPERTY TextFracV FLOAT 0.2\n"
                                            "PROPERTY ZoomProportionMax FLOAT 0.8\n"
                                            "PROPERTY ZoomProportionMin FLOAT 0.1\n"
                                            "PROPERTY UsePlayerColor BOOL 1\n"
                                            "ENDPROPERTIES\n"
                                            "ENDCLASS\n"
                                            "BEGINCLASS ParticlePointCreator ParticlePointCreator0\n"
                                            "BEGINPROPERTIES\n"
                                            "PROPERTY InitialScale FLOAT 1\n"
                                            "ENDPROPERTIES\n"
                                            "ENDCLASS\n";

class VolumeGlowEffectTest: public ::testing::Test
{
protected:
	std::unique_ptr<Effect> Make()
	{
		auto file = psys::ParticleFile::Parse(std::string(k_ArtefactGlow));
		EXPECT_TRUE(file.has_value());
		return std::make_unique<Effect>(std::make_shared<const psys::ParticleFile>(std::move(*file)),
		                                EffectServices {classes, world, random, noise}, glm::vec3(0.0f), 1.0f, false);
	}

	ParticleClassRegistry classes {ParticleClassRegistry::WithAllClasses()};
	GlowWorld world;
	GlowRandom random;
	maths::ValueNoise noise;
};

TEST_F(VolumeGlowEffectTest, AnArtefactsShellIsKeptOnItInItsOwnersColourAndDrawnWhereItStands)
{
	const auto rock = static_cast<entt::entity>(7);
	world.objects[rock] = {10.0f, 2.0f, 20.0f};
	auto effect = Make();
	EXPECT_TRUE(effect->UnportedClasses().empty());
	effect->SetPlayer(0);
	auto info = effect->GetProcessInfo();
	info.power = 1.5f;
	effect->SetProcessInfo(info);
	effect->AddTarget(rock);
	for (int i = 0; i < 3; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_EQ(effect->AtomCount(), 1u);

	Effect::DrawWalk walk;
	effect->Walk(1.0f, walk);
	ASSERT_EQ(walk.atoms.size(), 1u);
	ASSERT_TRUE(walk.atoms[0].objectGlow.has_value());
	const auto& glow = *walk.atoms[0].objectGlow;
	EXPECT_EQ(glow.object, rock);
	EXPECT_NEAR(glow.zoom, 0.8f, k_Epsilon);
	EXPECT_EQ(glow.argb, 0xCFFF4646u);
	EXPECT_EQ(glow.texture, 2);

	draw::Frame frame;
	draw::Sources sources;
	sources.objectPosition = [this](entt::entity object) { return world.ObjectPosition(object); };
	draw::AddEffect(frame, walk, draw::DrawPath::Sorted, glm::vec3(0.0f), 0, sources);
	ASSERT_EQ(frame.shells.size(), 1u);
	ASSERT_EQ(frame.items.size(), 1u);
	EXPECT_EQ(frame.items[0].kind, draw::ItemKind::Shell);
	EXPECT_EQ(frame.items[0].sortPoint, glm::vec3(10.0f, 2.0f, 20.0f));
	EXPECT_EQ(frame.shells[0].texture, entt::hashed_string("raw/S_VolLight3").value());
	EXPECT_EQ(frame.shells[0].alphaTexture, entt::hashed_string("raw/S_VolLight3a").value());
	EXPECT_NEAR(frame.shells[0].textureFraction.x, 0.7f, k_Epsilon);
	EXPECT_EQ(frame.shells[0].uvOffset, glm::vec2(0.0f));

	// The object gone, its shell goes with it
	world.objects.clear();
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
}

TEST_F(VolumeGlowEffectTest, AWeakerEffectReachesLessAndATargetThatIsntThereMakesNoShell)
{
	const auto rock = static_cast<entt::entity>(7);
	world.objects[rock] = {0.0f, 0.0f, 0.0f};
	auto effect = Make();
	auto info = effect->GetProcessInfo();
	info.power = 0.5f;
	effect->SetProcessInfo(info);
	effect->AddTarget(static_cast<entt::entity>(99));
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
	effect->AddTarget(rock);
	effect->Step(k_Step);
	effect->Step(k_Step);
	Effect::DrawWalk walk;
	effect->Walk(1.0f, walk);
	ASSERT_EQ(walk.atoms.size(), 1u);
	EXPECT_NEAR(walk.atoms[0].objectGlow->zoom, 0.45f, k_Epsilon);
	// Without a player it is white
	EXPECT_EQ(walk.atoms[0].objectGlow->argb, 0xCFFFFFFFu);
}
TEST_F(VolumeGlowEffectTest, TheShellsTextureSlidesByTheGamesTimeAndStandsStillWhilePaused)
{
	const auto rock = static_cast<entt::entity>(7);
	world.objects[rock] = {0.0f, 0.0f, 0.0f};
	world.gameMilliseconds = 10000;
	auto effect = Make();
	effect->AddTarget(rock);
	for (int i = 0; i < 3; ++i)
	{
		effect->Step(k_Step);
	}
	Effect::DrawWalk walk;
	effect->Walk(1.0f, walk);
	const auto offsetAt = [&walk](uint64_t gameMilliseconds) {
		draw::Sources sources;
		sources.gameMilliseconds = gameMilliseconds;
		draw::Frame frame;
		draw::AddEffect(frame, walk, draw::DrawPath::Sorted, glm::vec3(0.0f), -1, sources);
		return frame.shells.empty() ? glm::vec2(-1.0f) : frame.shells[0].uvOffset;
	};
	EXPECT_EQ(offsetAt(10000), glm::vec2(0.0f));
	// Two seconds of the game's time later it has slid by its speeds
	const auto slid = offsetAt(12000);
	EXPECT_NEAR(slid.x, 0.6f, 1e-4f);
	EXPECT_NEAR(slid.y, 0.62f, 1e-4f);
	// Paused, the game's time stands still, and so does the texture, however long the frames take
	EXPECT_EQ(offsetAt(12000), slid);
}
} // namespace
