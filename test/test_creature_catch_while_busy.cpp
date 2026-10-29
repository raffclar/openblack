/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureLayers.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/CreatureMindSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The hands of a creature. They record each end of a point.
class PointingHands final: public CreatureObjectActionSystemInterface
{
public:
	void ProcessTurn() override {}
	void ActionTurn(entt::entity) override {}
	void AfterPoseTurn(entt::entity) override {}
	void UpdateHeld() override {}
	void ShowHeldAtTurnEnd() override {}
	bool PickUp(entt::entity, entt::entity) override { return false; }
	bool PutDown(entt::entity) override { return false; }
	bool Discard(entt::entity) override { return false; }
	bool DiscardWith(entt::entity, size_t) override { return false; }
	bool EatHeld(entt::entity) override { return false; }
	bool EatHeldOnly(entt::entity) override { return false; }
	bool Keep(entt::entity, size_t) override { return false; }
	bool Throw(entt::entity, const glm::vec3&) override { return false; }
	bool ThrowTaking(entt::entity, const glm::vec3&, float) override { return false; }
	bool ThrowAimed(entt::entity, const glm::vec3&, float) override { return false; }
	bool Destroy(entt::entity, entt::entity) override { return false; }
	bool Kick(entt::entity, entt::entity) override { return false; }
	bool PointAt(entt::entity, const glm::vec3&) override { return false; }
	bool Catch(entt::entity, entt::entity) override { return false; }
	void Cancel(entt::entity creature) override { cancelled.push_back(creature); }
	void EndPoint(entt::entity creature) override { pointsEnded.push_back(creature); }
	void Drop(entt::entity) override {}
	[[nodiscard]] State GetState(entt::entity) const override { return State::Busy; }
	[[nodiscard]] bool IsCatching(entt::entity) const override { return false; }
	[[nodiscard]] bool PlaysOnAlone(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<float> GetProgress(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanPickUp(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanEat(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanEatHeld(entt::entity) const override { return false; }
	[[nodiscard]] bool CanDestroy(entt::entity) const override { return false; }

	std::vector<entt::entity> pointsEnded;
	std::vector<entt::entity> cancelled;
};

/// A creature that the game makes to catch a thing while its body is busy
class CreatureCatchWhileBusyTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"ai", "game"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_st>(name);
			}
		}
		_info = std::make_unique<InfoConstants>();
		Locator::infoConstants::emplace(*_info);
		Locator::entitiesRegistry::emplace<Registry>();
		hands = &static_cast<PointingHands&>(Locator::creatureObjectActionSystem::emplace<PointingHands>());
		auto& registry = Locator::entitiesRegistry::value();
		creature = registry.Create();
		thing = registry.Create();
		registry.Assign<CreatureMindState>(creature);
		registry.Assign<CreatureAnimation>(creature);
	}
	void TearDown() override
	{
		Locator::creatureObjectActionSystem::reset();
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	[[nodiscard]] static creature_layers::BodyAction& BodyOf(entt::entity entity)
	{
		return Locator::entitiesRegistry::value().Get<CreatureAnimation>(entity).body;
	}

	std::unique_ptr<InfoConstants> _info;
	CreatureMindSystem minds;
	PointingHands* hands {nullptr};
	entt::entity creature {entt::null};
	entt::entity thing {entt::null};
};
} // namespace

// The catch stops the point of the creature, so that the arm plays the end of the point
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCatchWhileBusyTest, TheCatchEndsAPoint)
{
	minds.ForceCatch(creature, thing);
	EXPECT_EQ(hands->pointsEnded, (std::vector<entt::entity> {creature}));
	// The point plays its end: the hands do not stop at once
	EXPECT_TRUE(hands->cancelled.empty());
}

// The catch stops a dance, which then plays its finish
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCatchWhileBusyTest, TheCatchEndsADance)
{
	auto& body = BodyOf(creature);
	body.kind = creature_layers::BodyAction::Kind::Dance;
	body.phase = creature_layers::BodyAction::Phase::Loop;
	const auto ended = creature_layers::EndDance(body);
	ASSERT_NE(ended.phase, body.phase);
	minds.ForceCatch(creature, thing);
	EXPECT_EQ(BodyOf(creature).phase, ended.phase);
}

// An action that the body plays once is not cut short: the catch waits for its end
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureCatchWhileBusyTest, TheCatchLetsAnActionPlayOn)
{
	auto& body = BodyOf(creature);
	body.kind = creature_layers::BodyAction::Kind::Once;
	body.animations = {7, 0, 0};
	body.timeMs = 250.0f;
	minds.ForceCatch(creature, thing);
	EXPECT_EQ(BodyOf(creature).kind, creature_layers::BodyAction::Kind::Once);
	EXPECT_FLOAT_EQ(BodyOf(creature).timeMs, 250.0f);
	EXPECT_TRUE(creature_layers::IsPlaying(BodyOf(creature)));
}
