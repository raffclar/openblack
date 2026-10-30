/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleEffect.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/EffectRequestSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// The particles for the test: they keep what the simulation asked for
class FakeServer final: public EffectServerInterface
{
public:
	std::vector<std::pair<EffectRequest, EffectHandle>> served;
	std::vector<std::pair<EffectHandle, glm::vec3>> moved;
	std::vector<EffectHandle> stopped;
	std::vector<EffectHandle> removed;
	/// The effects that run, by their numbers
	std::vector<EffectHandle> running;
	/// The calls that change a running effect, as the name of the call and the number of the effect
	std::vector<std::pair<std::string, EffectHandle>> changes;
	/// The number that the particles give an effect that only this computer shows
	EffectHandle ownNumber {7};

	void SetLifeListener(EffectLifeListener* lifeListener) override { listener = lifeListener; }
	EffectHandle Serve(const EffectRequest& request, EffectHandle handle) override
	{
		served.emplace_back(request, handle);
		const auto number = handle != k_NoEffectHandle ? handle : ownNumber++;
		running.push_back(number);
		if (listener != nullptr)
		{
			listener->EffectStarted(number);
		}
		return number;
	}
	void Move(EffectHandle handle, glm::vec3 position) override { moved.emplace_back(handle, position); }
	void Stop(EffectHandle handle) override { stopped.push_back(handle); }
	void Remove(EffectHandle handle) override
	{
		removed.push_back(handle);
		End(handle);
	}
	[[nodiscard]] bool IsRunning(EffectHandle handle) const { return std::ranges::find(running, handle) != running.end(); }
	/// The rules of the effect end it, and it goes
	void End(EffectHandle handle)
	{
		if (std::erase(running, handle) > 0 && listener != nullptr)
		{
			listener->EffectEnded(handle);
		}
	}
	void SetPlayer(EffectHandle handle, int /*player*/) override { changes.emplace_back("player", handle); }
	void SetPower(EffectHandle handle, float /*power*/) override { changes.emplace_back("power", handle); }
	void SetMagnitude(EffectHandle handle, float /*magnitude*/) override { changes.emplace_back("magnitude", handle); }
	void SetAlpha(EffectHandle handle, float /*alpha*/) override { changes.emplace_back("alpha", handle); }
	void SetProcessInfo(EffectHandle handle, const particles::ProcessInfo& /*info*/) override
	{
		changes.emplace_back("processInfo", handle);
	}
	void SetEnabled(EffectHandle handle, bool /*enabled*/) override { changes.emplace_back("enabled", handle); }
	void AddTarget(EffectHandle handle, entt::entity /*target*/) override { changes.emplace_back("target", handle); }
	void AddTargetPosition(EffectHandle handle, glm::vec3 /*position*/) override
	{
		changes.emplace_back("targetPosition", handle);
	}
	void AddPlasma(EffectHandle handle, const particles::PlasmaCommand& /*command*/) override
	{
		changes.emplace_back("plasma", handle);
	}
	void AddManaPathSpark(EffectHandle handle, const particles::mana_path::Spark& /*spark*/) override
	{
		changes.emplace_back("manaPathSpark", handle);
	}
	bool ProcessForSpell(EffectHandle handle, const particles::ProcessInfo& /*info*/, float /*seconds*/) override
	{
		changes.emplace_back("step", handle);
		// The effect ends at this step: it goes
		if (endsAtNextStep == handle)
		{
			End(handle);
			return false;
		}
		return IsRunning(handle);
	}
	void RestoreSink(EffectHandle handle, particles::SpellSink* sink) override
	{
		changes.emplace_back("sink", handle);
		restoredSink = sink;
	}
	void SetHidden(EffectHandle handle, bool /*hidden*/) override { changes.emplace_back("hidden", handle); }
	void SetDrawOffset(EffectHandle handle, glm::vec3 /*offset*/) override { changes.emplace_back("drawOffset", handle); }
	void SetLiveHand(EffectHandle handle, glm::vec3 /*hand*/) override { changes.emplace_back("liveHand", handle); }
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3 /*point*/, float /*margin*/) const override
	{
		return shield;
	}
	void AddBeliefSprite(const particles::BeliefSprite& /*sprite*/) override { shared.emplace_back("belief"); }
	void AddHandManaPathSpark(const particles::mana_path::Spark& /*spark*/) override { shared.emplace_back("manaPath"); }
	void AddStormBolt(glm::vec3 /*cloudPoint*/) override { shared.emplace_back("stormBolt"); }
	void SetFightMarks(std::vector<particles::draw::FightMark> marks) override
	{
		shared.emplace_back("fightMarks " + std::to_string(marks.size()));
	}
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& /*sheet*/) override { shared.emplace_back("lightSheet"); }
	void SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol> symbols) override
	{
		shared.emplace_back("playerSymbols " + std::to_string(symbols.size()));
	}

	/// The record that hears of each start and end
	EffectLifeListener* listener {nullptr};
	/// The effect that ends at its next step
	EffectHandle endsAtNextStep {k_NoEffectHandle};
	particles::SpellSink* restoredSink {nullptr};
	std::shared_ptr<particles::ShieldSphere> shield;
	/// The adds to the effects that run all the time, by name
	std::vector<std::string> shared;
};

/// A miracle for the test: it keeps the events of its effect
class FakeMiracle final: public particles::SpellSink
{
public:
	bool SpellEvent(const particles::SpellEventInfo& /*event*/) override { return true; }
	[[nodiscard]] int PowerUpLevel() const override { return 0; }
};

} // namespace

TEST(EffectRequests, EachStartTakesTheNextNumberAndThePresentationUsesIt)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	const auto first = effects.Start({.type = ParticleType::SpelldispenserVortex, .synced = false, .step = EffectStep::Frame});
	const auto second = effects.Start({.type = SpotVisualType::ObjectAppear, .position = {1.0f, 2.0f, 3.0f}});
	EXPECT_EQ(first, k_FirstEffectHandle);
	EXPECT_EQ(second, k_FirstEffectHandle + 1);
	EXPECT_EQ(effects.NextHandle(), k_FirstEffectHandle + 2);
	ASSERT_EQ(server.served.size(), 2u);
	EXPECT_EQ(server.served[0].second, first);
	EXPECT_EQ(std::get<ParticleType>(server.served[0].first.type), ParticleType::SpelldispenserVortex);
	EXPECT_EQ(server.served[0].first.step, EffectStep::Frame);
	EXPECT_FALSE(server.served[0].first.synced);
	EXPECT_EQ(server.served[1].second, second);
	EXPECT_EQ(std::get<SpotVisualType>(server.served[1].first.type), SpotVisualType::ObjectAppear);
	EXPECT_EQ(server.served[1].first.position, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(EffectRequests, AnEffectThatPlaysOutByItselfTakesNoNumber)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	effects.Show({.type = SpotVisualType::TownFireworks});
	EXPECT_EQ(effects.NextHandle(), k_FirstEffectHandle);
	ASSERT_EQ(server.served.size(), 1u);
	EXPECT_EQ(server.served[0].second, k_NoEffectHandle);
}

TEST(EffectRequests, AComputerThatShowsNothingGivesTheSameNumbers)
{
	FakeServer server;
	EffectRequestSystem shown([&server]() -> EffectServerInterface* { return &server; });
	EffectRequestSystem unshown([]() -> EffectServerInterface* { return nullptr; });
	for (auto* effects : {&shown, &unshown})
	{
		effects->Show({.type = SpotVisualType::GripLandscape});
		[[maybe_unused]] const auto first = effects->Start({.type = ParticleType::StormCast});
		effects->Move(first, glm::vec3(1.0f));
		effects->Stop(first);
		effects->Remove(first);
	}
	EXPECT_EQ(shown.NextHandle(), unshown.NextHandle());
	EXPECT_EQ(server.served.size(), 2u);
}

TEST(EffectRequests, MovesStopsAndRemovesGoToThePresentationByTheNumber)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	const auto handle = effects.Start({.type = ParticleType::StormCast});
	effects.Move(handle, {4.0f, 5.0f, 6.0f});
	effects.Stop(handle);
	effects.Remove(handle);
	// No effect: nothing goes to the presentation
	effects.Move(k_NoEffectHandle, glm::vec3(0.0f));
	effects.Stop(k_NoEffectHandle);
	effects.Remove(k_NoEffectHandle);
	ASSERT_EQ(server.moved.size(), 1u);
	EXPECT_EQ(server.moved[0].first, handle);
	EXPECT_EQ(server.moved[0].second, glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_EQ(server.stopped, std::vector {handle});
	EXPECT_EQ(server.removed, std::vector {handle});
}

TEST(EffectRequests, ANewLandNumbersFromTheFirstAgain)
{
	EffectRequestSystem effects([]() -> EffectServerInterface* { return nullptr; });
	[[maybe_unused]] const auto first = effects.Start({.type = ParticleType::StormCast});
	effects.Reset();
	EXPECT_EQ(effects.Start({.type = ParticleType::StormCast}), k_FirstEffectHandle);
}

TEST(EffectRequests, ASaveKeepsTheNextNumber)
{
	const entt::meta_ctx context;
	EffectRequestSystem saved([]() -> EffectServerInterface* { return nullptr; });
	[[maybe_unused]] const auto first = saved.Start({.type = ParticleType::StormCast});
	[[maybe_unused]] const auto second = saved.Start({.type = ParticleType::StormCast});
	ecs::reflection::BinaryWriter out(context);
	saved.SaveState(out);

	EffectRequestSystem loaded([]() -> EffectServerInterface* { return nullptr; });
	const auto bytes = out.Bytes();
	const ecs::reflection::EntityRemap identity;
	ecs::reflection::BinaryReader in(context, bytes, identity);
	ASSERT_TRUE(loaded.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_EQ(loaded.NextHandle(), saved.NextHandle());
}

TEST(EffectRequests, AnEffectOfThisComputerTakesTheNumberOfTheParticlesAndNoNumberOfTheSimulation)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	const auto own = effects.StartHere({.type = ParticleType::FlockFlyingCastGood, .synced = false});
	EXPECT_EQ(own, 7u);
	EXPECT_EQ(effects.NextHandle(), k_FirstEffectHandle);
	ASSERT_EQ(server.served.size(), 1u);
	EXPECT_EQ(server.served[0].second, k_NoEffectHandle);
	EXPECT_TRUE(effects.IsRunning(own));

	EffectRequestSystem unshown([]() -> EffectServerInterface* { return nullptr; });
	EXPECT_EQ(unshown.StartHere({.type = ParticleType::FlockFlyingCastGood}), k_NoEffectHandle);
}

TEST(EffectRequests, TheTurnsChangeAndReadARunningEffectByItsNumber)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	const auto handle = effects.Start({.type = SpotVisualType::MagicBeam});
	EXPECT_TRUE(effects.IsRunning(handle));
	EXPECT_FALSE(effects.IsRunning(handle + 1));
	effects.SetPlayer(handle, 2);
	effects.SetPower(handle, 0.5f);
	effects.SetMagnitude(handle, 3.0f);
	effects.SetAlpha(handle, 0.25f);
	effects.SetProcessInfo(handle, {});
	effects.SetEnabled(handle, false);
	effects.AddTarget(handle, entt::entity {4});
	effects.AddTargetPosition(handle, glm::vec3(1.0f));
	effects.AddPlasma(handle, {});
	effects.AddManaPathSpark(handle, {});
	const std::vector<std::string> expected {"player",  "power",  "magnitude",      "alpha",  "processInfo",
	                                         "enabled", "target", "targetPosition", "plasma", "manaPathSpark"};
	ASSERT_EQ(server.changes.size(), expected.size());
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(server.changes[i].first, expected[i]);
		EXPECT_EQ(server.changes[i].second, handle);
	}
}

TEST(EffectRequests, NoEffectAndAComputerThatShowsNothingChangeNothingAndRunNothing)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	EXPECT_FALSE(effects.IsRunning(k_NoEffectHandle));
	effects.SetPlayer(k_NoEffectHandle, 1);
	effects.AddTarget(k_NoEffectHandle, entt::entity {1});
	effects.AddPlasma(k_NoEffectHandle, {});
	effects.AddManaPathSpark(k_NoEffectHandle, {});
	EXPECT_TRUE(server.changes.empty());

	EffectRequestSystem unshown([]() -> EffectServerInterface* { return nullptr; });
	const auto handle = unshown.Start({.type = SpotVisualType::MagicBeam});
	EXPECT_FALSE(unshown.IsRunning(handle));
	unshown.SetMagnitude(handle, 2.0f);
	unshown.SetEnabled(handle, true);
}

TEST(EffectRequests, AMiracleStepsItsOwnEffectAndGetsItsEvents)
{
	FakeServer server;
	FakeMiracle miracle;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	EXPECT_TRUE(effects.ShowsEffects());
	const auto handle = effects.Start({.type = ParticleType::StormCast, .sink = &miracle});
	EXPECT_EQ(handle, k_FirstEffectHandle);
	ASSERT_EQ(server.served.size(), 1u);
	EXPECT_EQ(server.served[0].first.sink, &miracle);
	EXPECT_TRUE(effects.Step(handle, {}, 0.1f));
	effects.SetHidden(handle, true);
	effects.SetDrawOffset(handle, glm::vec3(1.0f));
	effects.SetLiveHand(handle, glm::vec3(2.0f));
	effects.RestoreSink(handle, &miracle);
	EXPECT_EQ(server.restoredSink, &miracle);
	// The effect ends at its step, and is gone after it
	server.endsAtNextStep = handle;
	EXPECT_FALSE(effects.Step(handle, {}, 0.1f));
	EXPECT_FALSE(effects.IsRunning(handle));
	const std::vector<std::string> expected {"step", "hidden", "drawOffset", "liveHand", "sink", "step"};
	ASSERT_EQ(server.changes.size(), expected.size());
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(server.changes[i].first, expected[i]);
		EXPECT_EQ(server.changes[i].second, handle);
	}
	// No effect: no step
	EXPECT_FALSE(effects.Step(k_NoEffectHandle, {}, 0.1f));
	EXPECT_EQ(server.changes.size(), expected.size());
}

TEST(EffectRequests, TheWorldFindsAShieldOnlyOnAComputerThatShowsTheEffects)
{
	FakeServer server;
	server.shield = std::make_shared<particles::ShieldSphere>();
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	EXPECT_EQ(effects.FindShield(glm::vec3(0.0f), 1.0f), server.shield);

	EffectRequestSystem unshown([]() -> EffectServerInterface* { return nullptr; });
	EXPECT_FALSE(unshown.ShowsEffects());
	EXPECT_EQ(unshown.FindShield(glm::vec3(0.0f), 1.0f), nullptr);
	FakeMiracle miracle;
	const auto handle = unshown.Start({.type = ParticleType::StormCast, .sink = &miracle});
	EXPECT_FALSE(unshown.Step(handle, {}, 0.1f));
	unshown.RestoreSink(handle, &miracle);
}

TEST(EffectRequests, TheAddsToTheEffectsThatRunAllTheTimeGoToThePresentation)
{
	FakeServer server;
	EffectRequestSystem shown([&server]() -> EffectServerInterface* { return &server; });
	EffectRequestSystem unshown([]() -> EffectServerInterface* { return nullptr; });
	for (auto* effects : {&shown, &unshown})
	{
		effects->AddBeliefSprite({});
		effects->AddHandManaPathSpark({});
		effects->AddStormBolt(glm::vec3(1.0f));
		effects->SetFightMarks(std::vector<particles::draw::FightMark>(2));
		effects->AddLightSheet(std::make_shared<particles::LightSheet>());
		effects->SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol>(3));
	}
	// Only the computer that shows the effects adds to them, and an add takes no number
	const std::vector<std::string> expected {"belief",       "manaPath",   "stormBolt",
	                                         "fightMarks 2", "lightSheet", "playerSymbols 3"};
	EXPECT_EQ(server.shared, expected);
	EXPECT_TRUE(server.served.empty());
	EXPECT_EQ(shown.NextHandle(), k_FirstEffectHandle);
}

TEST(EffectRequests, TheTurnsReadTheRecordThatTheParticlesTellOfEachStartAndEnd)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	ASSERT_EQ(server.listener, &effects);
	const auto beam = effects.Start({.type = SpotVisualType::MagicBeamOnCitadel, .turns = -1});
	const auto glow = effects.Start({.type = SpotVisualType::MagicFxOnObject});
	EXPECT_TRUE(effects.IsRunning(beam));
	EXPECT_TRUE(effects.IsRunning(glow));
	// The rules of the particles end the glow: the record hears of it, and the beam still runs
	server.End(glow);
	EXPECT_FALSE(effects.IsRunning(glow));
	EXPECT_TRUE(effects.IsRunning(beam));
	// An effect that the particles start by themselves is in the record too
	effects.EffectStarted(42);
	EXPECT_TRUE(effects.IsRunning(42));
	// A new land or a load ends every effect at the same time
	effects.EffectsCleared();
	EXPECT_FALSE(effects.IsRunning(beam));
	EXPECT_FALSE(effects.IsRunning(42));
	// The load of the effects tells the record of each effect that the save kept
	effects.EffectStarted(beam);
	EXPECT_TRUE(effects.IsRunning(beam));
	EXPECT_FALSE(effects.IsRunning(k_NoEffectHandle));
}

TEST(EffectRequests, ARequestThatStartsNoEffectIsNotInTheRecord)
{
	EffectRequestSystem unshown([]() -> EffectServerInterface* { return nullptr; });
	const auto handle = unshown.Start({.type = SpotVisualType::MagicBeam});
	EXPECT_FALSE(unshown.IsRunning(handle));
	// A new land numbers from the first again, but only the particles end the effects
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	const auto first = effects.Start({.type = SpotVisualType::MagicBeam});
	effects.Reset();
	EXPECT_TRUE(effects.IsRunning(first));
	effects.Remove(first);
	EXPECT_FALSE(effects.IsRunning(first));
}

TEST(EffectRequests, TheRecordStopsToListenWhenTheSystemGoes)
{
	FakeServer server;
	{
		const EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
		EXPECT_NE(server.listener, nullptr);
	}
	EXPECT_EQ(server.listener, nullptr);
}
