/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlightRules.h"
#include "ECS/ScriptHighlightWorld.h"
#include "Enums.h"
#include "Resources/ResourceManager.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ScriptHighlightSystem.h"

using namespace openblack;
using namespace openblack::ecs::script_highlights;
using openblack::ecs::components::ScriptHighlight;
using openblack::ecs::components::ScriptHighlightGlow;
using openblack::ecs::components::Sprite;
using openblack::ecs::components::Transform;
using openblack::ecs::systems::ScriptHighlightSystem;

namespace
{
constexpr uint32_t k_Sign = static_cast<uint32_t>(HighlightInfo::DidYouKnowSign);
constexpr uint32_t k_Silver = static_cast<uint32_t>(HighlightInfo::Silver);
constexpr uint32_t k_Gold = static_cast<uint32_t>(HighlightInfo::Gold);

/// A land of its own: flat ground at height 2, the shipped table's four rows, every model a box 1 across and 2 tall
/// about a middle 1 up, and a record of what was asked of the particles, the sounds and the help system
class FakeWorld final: public ScriptHighlightWorldInterface
{
public:
	ecs::Registry registry;
	std::vector<ThingBelow> below;
	uint32_t turn {0};
	bool scrollsDrawn {true};
	uint32_t nextEffect {1};
	std::vector<std::pair<ParticleType, glm::vec3>> started;
	std::vector<uint32_t> deleted;
	std::vector<uint32_t> stepped;
	std::vector<uint32_t> sounds;
	std::vector<uint32_t> helpEvents;
	std::vector<std::string> helpScripts;
	std::vector<uint32_t> tipsShown;
	int tipsHidden {0};
	std::vector<uint32_t> replayed;

	ecs::Registry& Entities() override { return registry; }
	[[nodiscard]] std::optional<KindInfo> InfoOf(uint32_t kind) const override
	{
		switch (kind)
		{
		case 0:
			return KindInfo {MeshId::I_Miniscroll, MeshId::I_MiniscrollActive, ParticleType::None, ParticleType::None};
		case 1:
			return KindInfo {static_cast<MeshId>(368), static_cast<MeshId>(368), ParticleType::None, ParticleType::None};
		case 2:
			return KindInfo {MeshId::I_MiniscrollSilver, MeshId::I_MiniscrollSilverActive,
			                 ParticleType::ScriptHighlightSilverGlints, ParticleType::ScriptHighlightSilverActive};
		case 3:
			return KindInfo {MeshId::I_MiniscrollGold, MeshId::I_MiniscrollGoldActive, ParticleType::ScriptHighlightGoldGlints,
			                 ParticleType::ScriptHighlightGoldActive};
		default:
			return std::nullopt;
		}
	}
	[[nodiscard]] std::optional<ModelBox> BoxOf(MeshId /*mesh*/) const override
	{
		return ModelBox {.centre = {0.0f, 1.0f, 0.0f}, .halfSize = {0.5f, 1.0f, 0.5f}};
	}
	[[nodiscard]] float LandHeight(glm::vec2 /*point*/) const override { return 2.0f; }
	[[nodiscard]] std::vector<ThingBelow> ThingsBelow(entt::entity /*highlight*/, glm::vec3 /*point*/) const override
	{
		return below;
	}
	[[nodiscard]] uint32_t Turn() const override { return turn; }
	[[nodiscard]] float MillisecondsPerTurn() const override { return 100.0f; }
	[[nodiscard]] bool ScrollsDrawn() const override { return scrollsDrawn; }
	uint32_t StartEffect(ParticleType type, glm::vec3 at) override
	{
		started.emplace_back(type, at);
		return nextEffect++;
	}
	void TargetEffect(uint32_t /*effect*/, entt::entity /*target*/) override {}
	void StepEffect(uint32_t effect, float /*seconds*/) override { stepped.push_back(effect); }
	void MoveEffect(uint32_t /*effect*/, glm::vec3 /*to*/) override {}
	void DeleteEffect(uint32_t effect) override { deleted.push_back(effect); }
	void PlaySound(uint32_t sample) override { sounds.push_back(sample); }
	[[nodiscard]] std::optional<Sprite> GlowLook() const override
	{
		return Sprite {.texture = {}, .uvMin = {}, .uvExtent = glm::vec2(0.125f), .tint = glm::vec4(1.0f)};
	}
	void HelpEvent(uint32_t event) override { helpEvents.push_back(event); }
	void StartHelpScript(std::string_view name) override { helpScripts.emplace_back(name); }
	void ShowTip(entt::entity /*sign*/, uint32_t text, uint32_t /*category*/) override { tipsShown.push_back(text); }
	void HideTip() override { ++tipsHidden; }
	void ReplayChallenge(uint32_t challenge) override { replayed.push_back(challenge); }
};

struct Highlights
{
	FakeWorld* world;
	std::unique_ptr<ScriptHighlightSystem> system;
};

Highlights Make()
{
	auto world = std::make_unique<FakeWorld>();
	auto* raw = world.get();
	return {raw, std::make_unique<ScriptHighlightSystem>(std::move(world))};
}
} // namespace

TEST(ScriptHighlightRules, ThePulseGoesRoundOnceInAboutThirteenTurns)
{
	Pulse pulse;
	StepPulse(pulse, 100.0f);
	EXPECT_FLOAT_EQ(pulse.phase, 0.5f);
	EXPECT_FLOAT_EQ(pulse.level, (1.0f - std::cos(0.5f)) * 0.5f);
	EXPECT_FLOAT_EQ(pulse.previous, 0.0f);
	for (int i = 0; i < 12; ++i)
	{
		StepPulse(pulse, 100.0f);
	}
	// 13 steps of half a radian pass a whole turn and start again
	EXPECT_NEAR(pulse.phase, 6.5f - 6.2831855f, 1e-5f);
}

TEST(ScriptHighlightRules, TheBeatBlendsBetweenTurnsFromFourTenthsToOne)
{
	const Pulse low {.phase = 0.0f, .level = 0.0f, .previous = 0.0f};
	EXPECT_FLOAT_EQ(PulseShare(low, 0.5f), 0.4f);
	const Pulse rising {.phase = 0.0f, .level = 1.0f, .previous = 0.0f};
	EXPECT_FLOAT_EQ(PulseShare(rising, 0.5f), 0.7f);
	EXPECT_FLOAT_EQ(PulseShare(rising, 1.0f), 1.0f);
}

TEST(ScriptHighlightRules, OnlyTheTopOfTheBeatShowsAYellowBeam)
{
	EXPECT_EQ(BeamColour(HighlightInfo::Silver, 0.7f), 0x14b4dcffu);
	EXPECT_EQ(BeamColour(HighlightInfo::Gold, 0.99f), 0x00ffff00u);
	EXPECT_EQ(BeamColour(HighlightInfo::Gold, 1.0f), 0x50ffff00u);
}

TEST(ScriptHighlightRules, AHighlightSpinsHalfATurnASecond)
{
	EXPECT_NEAR(Spin(0.0f, 1000.0f), glm::pi<float>(), 1e-5f);
	// Past a whole turn it comes round
	EXPECT_NEAR(Spin(6.0f, 1000.0f), 6.0f + glm::pi<float>() - 6.2831855f, 1e-5f);
	// A negative angle is brought round towards none
	EXPECT_NEAR(WrapAngle(-7.0f), -7.0f + 6.2831855f, 1e-5f);
}

TEST(ScriptHighlightRules, AStartedSignFacesTheCamera)
{
	// The camera due east: a quarter turn past the angle to it
	EXPECT_NEAR(FacingAngle({10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}), glm::half_pi<float>(), 1e-5f);
	EXPECT_NEAR(FacingAngle({0.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 0.0f}), glm::pi<float>(), 1e-5f);
}

TEST(ScriptHighlightRules, AScrollGrowsWithTheCamerasDistanceFromTenToThirty)
{
	EXPECT_FLOAT_EQ(DrawnScale(HighlightInfo::Gold, 1.0f, 5.0f), 10.0f * 0.0333333351f);
	EXPECT_FLOAT_EQ(DrawnScale(HighlightInfo::Gold, 1.0f, 20.9f), 20.0f * 0.0333333351f);
	EXPECT_FLOAT_EQ(DrawnScale(HighlightInfo::Silver, 1.0f, 500.0f), 30.0f * 0.0333333351f);
	EXPECT_FLOAT_EQ(DrawnScale(HighlightInfo::DidYouKnowSign, 1.0f, 5.0f), 1.0f);
}

TEST(ScriptHighlightRules, TheGlowStandsBeforeTheModelTowardsTheCamera)
{
	EXPECT_FLOAT_EQ(GlowHalfSize(1.5f), 3.0f);
	EXPECT_FLOAT_EQ(GlowHalfSize(0.0f), 0.0001f);
	const auto at = GlowPosition({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, 2.0f);
	EXPECT_FLOAT_EQ(at.z, 2.8f);
	EXPECT_EQ(GlowAlpha(HighlightInfo::DidYouKnowSign), 0x32);
	EXPECT_EQ(GlowAlpha(HighlightInfo::Silver), 0x96);
	EXPECT_EQ(GlowAlpha(HighlightInfo::Gold), 0x64);
	EXPECT_EQ(GlowAlpha(HighlightInfo::Scroll), 0);
	EXPECT_FALSE(GlowShown(HighlightInfo::DidYouKnowSign, true));
	EXPECT_TRUE(GlowShown(HighlightInfo::Gold, true));
}

TEST(ScriptHighlightRules, ATapNeedsATextAndAScrollMustHaveStarted)
{
	EXPECT_FALSE(ValidToTap(HighlightInfo::DidYouKnowSign, 0, false));
	EXPECT_TRUE(ValidToTap(HighlightInfo::DidYouKnowSign, 4127, false));
	EXPECT_FALSE(ValidToTap(HighlightInfo::Gold, 52, false));
	EXPECT_TRUE(ValidToTap(HighlightInfo::Gold, 52, true));
	EXPECT_FALSE(ValidToTap(HighlightInfo::Gold, 0, true));

	const auto sign = Tap(HighlightInfo::DidYouKnowSign, 4127, true);
	EXPECT_EQ(sign.helpEvent, 0x22u);
	EXPECT_TRUE(sign.starts && sign.signSound && sign.showsTip);
	EXPECT_FALSE(sign.replaysChallenge);
	const auto elsewhere = Tap(HighlightInfo::Gold, 52, false);
	EXPECT_FALSE(elsewhere.helpEvent.has_value());
	EXPECT_TRUE(elsewhere.starts && elsewhere.replaysChallenge);
	EXPECT_FALSE(elsewhere.signSound);
	const auto none = Tap(HighlightInfo::Gold, 0, true);
	EXPECT_EQ(none.helpEvent, 0x23u);
	EXPECT_FALSE(none.starts);
}

TEST(ScriptHighlightRules, TipsAreKeptByCategoryUpToFortyEight)
{
	TipsRead tips;
	EXPECT_TRUE(tips.Empty());
	tips.Add(4127, 3);
	tips.Add(4127, 3);
	tips.Add(1, 5);
	EXPECT_TRUE(tips.Has(4127, 3));
	EXPECT_FALSE(tips.Has(4127, 2));
	EXPECT_FALSE(tips.Has(1, 5));
	for (uint32_t i = 0; i < 60; ++i)
	{
		tips.Add(i, 0);
	}
	EXPECT_TRUE(tips.Has(47, 0));
	EXPECT_FALSE(tips.Has(48, 0));
}

TEST(ScriptHighlightRules, AHighlightStandsOnTheHighestStillThingItOverlaps)
{
	const std::vector<ThingBelow> things {
	    {.inCell = {5.0f, 5.0f}, .radius = 2.0f, .top = 6.0f, .livingOrMoving = false},
	    {.inCell = {5.0f, 5.0f}, .radius = 2.0f, .top = 9.0f, .livingOrMoving = true},
	    {.inCell = {9.5f, 9.5f}, .radius = 0.1f, .top = 12.0f, .livingOrMoving = false},
	};
	EXPECT_FLOAT_EQ(HeightOnThings({5.5f, 5.0f}, 0.5f, things), 6.0f);
	// Nothing overlapping: the land
	EXPECT_FLOAT_EQ(HeightOnThings({0.5f, 0.5f}, 0.5f, std::span(things).first(1)), 0.0f);
	EXPECT_FLOAT_EQ(InCell({12.5f, 3.25f}).x, 2.5f);
}

TEST(ScriptHighlights, AGoldScrollIsMadeWithItsModelAndGlints)
{
	auto [world, system] = Make();
	const auto scroll = system->Create(k_Gold, {100.0f, 50.0f, 200.0f}, 52);
	ASSERT_TRUE(scroll != entt::null);
	const auto& highlight = world->registry.Get<ScriptHighlight>(scroll);
	EXPECT_EQ(highlight.kind, HighlightInfo::Gold);
	EXPECT_EQ(highlight.scriptId, 52u);
	EXPECT_FALSE(highlight.active);
	EXPECT_EQ(world->registry.Get<ecs::components::Mesh>(scroll).id, resources::HashIdentifier(MeshId::I_MiniscrollGold));
	ASSERT_EQ(world->started.size(), 1u);
	EXPECT_EQ(world->started[0].first, ParticleType::ScriptHighlightGoldGlints);
	// It stands on the land where it was made
	EXPECT_FLOAT_EQ(world->registry.Get<Transform>(scroll).position.y, 2.0f);
	// A kind past the table makes none
	EXPECT_TRUE(system->Create(4, {0.0f, 0.0f, 0.0f}, 0) == entt::null);
}

TEST(ScriptHighlights, StartingAScrollShowsItsActiveModelEffectAndSound)
{
	auto [world, system] = Make();
	const auto scroll = system->Create(k_Silver, {0.0f, 0.0f, 0.0f}, 31);
	system->SetActive(scroll, true);
	EXPECT_EQ(world->registry.Get<ecs::components::Mesh>(scroll).id,
	          resources::HashIdentifier(MeshId::I_MiniscrollSilverActive));
	ASSERT_EQ(world->started.size(), 2u);
	EXPECT_EQ(world->started[1].first, ParticleType::ScriptHighlightSilverActive);
	EXPECT_EQ(world->sounds, std::vector<uint32_t> {k_ScrollStartedSound});
	// Stopped, its active effect ends and its normal model shows
	const auto activeEffect = world->registry.Get<ScriptHighlight>(scroll).activeEffect;
	system->SetActive(scroll, false);
	EXPECT_EQ(world->deleted, std::vector<uint32_t> {activeEffect});
	EXPECT_EQ(world->registry.Get<ecs::components::Mesh>(scroll).id, resources::HashIdentifier(MeshId::I_MiniscrollSilver));
}

TEST(ScriptHighlights, AScrollStandsAtItsDrawHeightOrOnWhatIsUnderIt)
{
	auto [world, system] = Make();
	const auto scroll = system->Create(k_Gold, {5.0f, 0.0f, 5.0f}, 52);
	world->below = {{.inCell = {5.0f, 5.0f}, .radius = 3.0f, .top = 4.5f, .livingOrMoving = false}};
	system->ProcessTurn();
	EXPECT_FLOAT_EQ(world->registry.Get<ScriptHighlight>(scroll).heightAbove, 4.5f);
	system->SetDrawHeight(scroll, 10.0f);
	system->ProcessTurn();
	EXPECT_FLOAT_EQ(world->registry.Get<ScriptHighlight>(scroll).heightAbove, 10.0f);
	system->UpdateFrame(16.0f, 0.0f, {5.0f, 30.0f, 40.0f});
	EXPECT_FLOAT_EQ(world->registry.Get<Transform>(scroll).position.y, 12.0f);
}

TEST(ScriptHighlights, HiddenScrollsAreNotDrawnButSignsAre)
{
	auto [world, system] = Make();
	const auto scroll = system->Create(k_Gold, {0.0f, 0.0f, 0.0f}, 52);
	const auto sign = system->Create(k_Sign, {20.0f, 0.0f, 0.0f}, 0);
	world->scrollsDrawn = false;
	system->UpdateFrame(16.0f, 0.0f, {0.0f, 20.0f, 30.0f});
	EXPECT_TRUE(world->registry.AllOf<ecs::components::HiddenByState>(scroll));
	EXPECT_FALSE(world->registry.AllOf<ecs::components::HiddenByState>(sign));
	world->scrollsDrawn = true;
	system->UpdateFrame(16.0f, 0.0f, {0.0f, 20.0f, 30.0f});
	EXPECT_FALSE(world->registry.AllOf<ecs::components::HiddenByState>(scroll));
}

TEST(ScriptHighlights, AShownScrollSpinsScalesAndGlows)
{
	auto [world, system] = Make();
	const auto scroll = system->Create(k_Gold, {0.0f, 0.0f, 0.0f}, 52);
	system->UpdateFrame(1000.0f, 0.0f, {0.0f, 2.0f, 20.0f});
	const auto& highlight = world->registry.Get<ScriptHighlight>(scroll);
	EXPECT_NEAR(highlight.yAngle, glm::pi<float>(), 1e-5f);
	EXPECT_FLOAT_EQ(world->registry.Get<Transform>(scroll).scale.x, 20.0f * 0.0333333351f);
	// Its glints step as it is drawn
	EXPECT_EQ(world->stepped, std::vector<uint32_t> {highlight.glints});
	ASSERT_TRUE(highlight.glow != entt::null);
	const auto* sprite = world->registry.TryGet<Sprite>(highlight.glow);
	ASSERT_NE(sprite, nullptr);
	EXPECT_FLOAT_EQ(sprite->tint.a, static_cast<float>(0x64) / 255.0f);
	// Its glow goes after it
	const auto glow = highlight.glow;
	world->registry.Destroy(scroll);
	system->UpdateFrame(16.0f, 0.0f, {0.0f, 2.0f, 20.0f});
	EXPECT_FALSE(world->registry.Valid(glow));
}

TEST(ScriptHighlights, AGoneHighlightEndsItsEffects)
{
	auto [world, system] = Make();
	const auto scroll = system->Create(k_Gold, {0.0f, 0.0f, 0.0f}, 52);
	system->SetActive(scroll, true);
	const auto highlight = world->registry.Get<ScriptHighlight>(scroll);
	world->registry.Destroy(scroll);
	EXPECT_EQ(world->deleted, (std::vector<uint32_t> {highlight.glints, highlight.activeEffect}));
}

TEST(ScriptHighlights, TappingASignShowsItsTipAndTheFirstExplainsThem)
{
	auto [world, system] = Make();
	const auto sign = system->Create(k_Sign, {0.0f, 0.0f, 0.0f}, 0);
	// Without its tip the tap does nothing
	EXPECT_FALSE(system->Tap(sign, true));
	system->SetProperties(sign, 4127, 3);
	EXPECT_TRUE(system->Tap(sign, true));
	EXPECT_TRUE(world->registry.Get<ScriptHighlight>(sign).active);
	EXPECT_EQ(world->sounds, std::vector<uint32_t> {k_SignTappedSound});
	EXPECT_EQ(world->helpEvents, std::vector<uint32_t> {0x22});
	EXPECT_EQ(world->helpScripts, std::vector<std::string> {std::string(k_FirstTipScript)});
	EXPECT_EQ(world->tipsShown, std::vector<uint32_t> {4127});
	EXPECT_TRUE(system->GetTipsRead().Has(4127, 3));
	// Tapped again, the bubble shows no tip
	EXPECT_TRUE(system->Tap(sign, true));
	EXPECT_EQ(world->tipsHidden, 1);
	EXPECT_EQ(world->helpScripts.size(), 1u);
}

TEST(ScriptHighlights, ASignOfATipReadStartsWhenItChecks)
{
	auto [world, system] = Make();
	const auto first = system->Create(k_Sign, {0.0f, 0.0f, 0.0f}, 0);
	system->SetProperties(first, 4127, 3);
	system->Tap(first, true);
	// Made after its tip was read, it starts at once
	const auto again = system->Create(k_Sign, {30.0f, 0.0f, 0.0f}, 0);
	EXPECT_FALSE(world->registry.Get<ScriptHighlight>(again).active);
	system->SetProperties(again, 4127, 3);
	// It checks only on its eighth turns
	for (uint32_t turn = 0; turn < 8; ++turn)
	{
		world->turn = turn;
		system->ProcessTurn();
	}
	EXPECT_TRUE(world->registry.Get<ScriptHighlight>(again).active);
}

TEST(ScriptHighlights, AStartedScrollTappedReplaysItsChallenge)
{
	auto [world, system] = Make();
	const auto scroll = system->Create(k_Gold, {0.0f, 0.0f, 0.0f}, 52);
	EXPECT_FALSE(system->Tap(scroll, true));
	system->SetActive(scroll, true);
	EXPECT_TRUE(system->Tap(scroll, true));
	EXPECT_EQ(world->replayed, std::vector<uint32_t> {52});
	EXPECT_TRUE(world->tipsShown.empty());
}
