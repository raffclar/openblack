/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "Particles/ParticleDrawFrame.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// The rules of the particles for the test: no effect runs, and the test gives the symbols of the players and the
/// names of the sheets
class FakeRules final: public ParticleRuleSystemInterface
{
public:
	std::vector<particles::draw::PlayerSymbol> symbols;
	std::vector<std::string> sheetNames;

	EffectId Start(std::string_view /*file*/, glm::vec3 /*origin*/, float /*magnitude*/, bool /*synced*/) override
	{
		return k_NoEffect;
	}
	EffectId Start(ParticleType /*type*/, glm::vec3 /*origin*/, float /*magnitude*/, bool /*synced*/) override
	{
		return k_NoEffect;
	}
	EffectId StartForSpell(ParticleType /*type*/, glm::vec3 /*origin*/, glm::vec3 /*direction*/, float /*magnitude*/,
	                       particles::SpellSink& /*sink*/, bool /*synced*/) override
	{
		return k_NoEffect;
	}
	bool ProcessForSpell(EffectId /*id*/, const particles::ProcessInfo& /*info*/, float /*seconds*/) override { return false; }
	EffectId StartSpotVisual(SpotVisualType /*type*/, glm::vec3 /*position*/, std::optional<int> /*turns*/,
	                         entt::entity /*owner*/, float /*magnitude*/, bool /*synced*/) override
	{
		return k_NoEffect;
	}
	EffectHandle Serve(const EffectRequest& /*request*/, EffectHandle /*handle*/) override { return k_NoEffectHandle; }
	void Move(EffectHandle /*handle*/, glm::vec3 /*position*/) override {}
	void Stop(EffectHandle /*handle*/) override {}
	void Remove(EffectHandle /*handle*/) override {}
	void SetMagnitude(EffectHandle /*handle*/, float /*magnitude*/) override {}
	void SetAlpha(EffectHandle /*handle*/, float /*alpha*/) override {}
	void SetProcessInfo(EffectHandle /*handle*/, const particles::ProcessInfo& /*info*/) override {}
	void SetEnabled(EffectHandle /*handle*/, bool /*enabled*/) override {}
	void SetOrigin(EffectId /*id*/, glm::vec3 /*origin*/) override {}
	void SetPlayer(EffectId /*id*/, int /*player*/) override {}
	void SetDrawPath(EffectId /*id*/, particles::draw::DrawPath /*path*/) override {}
	void SetDrawOffset(EffectId /*id*/, glm::vec3 /*offset*/) override {}
	void SetLiveHand(EffectId /*id*/, glm::vec3 /*hand*/) override {}
	void SetInTemple(EffectId /*id*/) override {}
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3 /*point*/, float /*margin*/) const override
	{
		return nullptr;
	}
	[[nodiscard]] size_t GetSoundCount() const override { return 0; }
	void AddTarget(EffectId /*id*/, entt::entity /*target*/) override {}
	void AddGestureTrail(std::shared_ptr<particles::GestureTrail> /*trail*/) override {}
	void UpdateFrame(float /*gameSeconds*/, const HandFrame& /*hand*/) override {}
	void CloseDown(EffectId /*id*/) override {}
	void Delete(EffectId /*id*/) override {}
	[[nodiscard]] bool IsRunning(EffectId /*id*/) const override { return false; }
	[[nodiscard]] particles::Effect* Find(EffectId /*id*/) override { return nullptr; }
	void ProcessTurn() override {}
	void Reset() override {}
	void SetFreshIdsAtNextReset() override {}
	[[nodiscard]] std::vector<EffectInfo> GetEffects() const override { return {}; }
	[[nodiscard]] std::vector<std::string> GetFileNames() const override { return {}; }
	void SetPaused(bool /*paused*/) override {}
	[[nodiscard]] bool IsPaused() const override { return false; }

	[[nodiscard]] std::span<const particles::draw::PlayerSymbol> PlayerSymbols() const override { return symbols; }
	[[nodiscard]] std::span<const std::string> SheetNames() const override { return sheetNames; }
};

/// The sheets on the disk for the test. It counts how many times the draw looks at the disk.
class FakeDisk
{
public:
	int finds {0};

	ParticleSystem::FindTextureStems Finder()
	{
		return [this]() {
			++finds;
			return std::map<std::string, std::string, std::less<>> {{"playerssymbols", "PlayersSymbols"},
			                                                        {"s_spritesheet3", "S_SpriteSheet3"}};
		};
	}
};

} // namespace

TEST(ParticleSystemDraw, WithNoRulesTheFrameIsEmpty)
{
	FakeDisk disk;
	const ParticleSystem draw([]() -> const ParticleRuleSystemInterface* { return nullptr; }, disk.Finder());
	particles::draw::Frame frame;
	frame.sprites.resize(3);
	draw.CollectDrawFrame(0.5f, frame, false);
	EXPECT_TRUE(frame.sprites.empty());
	EXPECT_TRUE(frame.groups.empty());
	EXPECT_EQ(draw.GetDrawStats().sprites, 0u);
	draw.CollectEffectDrawFrame(1, 0.5f, frame);
	EXPECT_TRUE(frame.groups.empty());
	EXPECT_EQ(disk.finds, 0);
}

TEST(ParticleSystemDraw, ASymbolOfAPlayerDrawsOnlyAfterTheRulesNamedItsSheets)
{
	FakeRules rules;
	FakeDisk disk;
	const ParticleSystem draw([&rules]() -> const ParticleRuleSystemInterface* { return &rules; }, disk.Finder());
	rules.symbols.push_back({.position = {1.0f, 2.0f, 3.0f}, .scale = 1.0f, .player = 0});
	particles::draw::Frame frame;
	draw.CollectDrawFrame(0.0f, frame, false);
	EXPECT_TRUE(frame.sprites.empty());
	// The rules started an effect that shows the symbols
	rules.sheetNames = {"S_SpriteSheet3", "PlayersSymbols"};
	draw.CollectDrawFrame(0.0f, frame, false);
	EXPECT_EQ(frame.sprites.size(), 3u);
	EXPECT_EQ(draw.GetDrawStats().sprites, 3u);
	// The temple does not draw the symbols of the world
	draw.CollectDrawFrame(0.0f, frame, true);
	EXPECT_TRUE(frame.sprites.empty());
}

TEST(ParticleSystemDraw, TheDrawLooksAtTheDiskOneTime)
{
	FakeRules rules;
	FakeDisk disk;
	ParticleSystem draw([&rules]() -> const ParticleRuleSystemInterface* { return &rules; }, disk.Finder());
	particles::draw::Frame frame;
	// No names: no look at the disk
	draw.CollectDrawFrame(0.0f, frame, false);
	EXPECT_EQ(disk.finds, 0);
	rules.sheetNames = {"PlayersSymbols"};
	draw.CollectDrawFrame(0.0f, frame, false);
	rules.sheetNames.emplace_back("S_SpriteSheet3");
	draw.CollectDrawFrame(0.0f, frame, false);
	draw.LoadTextures();
	EXPECT_EQ(disk.finds, 1);
}
