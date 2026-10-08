/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The leash keys as the game reads them once a frame: which of L, V and B went down (Creature/LeashKeys.h), and what
// the game passes to the leash service for them (ECS/CreatureLoop.h), with a fake of the controls and of the service

#include <cstdint>

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/LeashKeys.h"
#include "ECS/CreatureLoop.h"
#include "Enums.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::test::creature_loop_fakes;
using input::BindableActionMap;
using openblack::creature_leash::LeashKey;

namespace
{
uint64_t Bits(BindableActionMap action)
{
	return static_cast<uint64_t>(action);
}

/// The controls with the actions held down this frame and those held the frame before
class FakeActions final: public input::GameActionInterface
{
public:
	uint64_t down {0};
	uint64_t downBefore {0};

	[[nodiscard]] bool GetBindable(BindableActionMap action) const override { return (down & Bits(action)) != 0; }
	[[nodiscard]] bool GetUnbindable(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableChanged(BindableActionMap action) const override
	{
		return ((down ^ downBefore) & Bits(action)) != 0;
	}
	[[nodiscard]] bool GetUnbindableChanged(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap action) const override
	{
		return (down & downBefore & Bits(action)) != 0;
	}
	[[nodiscard]] bool GetUnbindableRepeat(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return {}; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return {}; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override { return {}; }
	void Frame() override {}
	void ProcessEvent(const SDL_Event&) override {}
};

/// A press for PressedKey: the action is the given one
auto Only(BindableActionMap pressed)
{
	return [pressed](BindableActionMap action) { return action == pressed; };
}
} // namespace

TEST(LeashKeysPressed, EachKeyIsFound)
{
	EXPECT_EQ(creature_leash::PressedKey(Only(BindableActionMap::LEASH_UNLEASH_CREATURE)), LeashKey::Leash);
	EXPECT_EQ(creature_leash::PressedKey(Only(BindableActionMap::PREVIOUS_LEASH)), LeashKey::PreviousLeash);
	EXPECT_EQ(creature_leash::PressedKey(Only(BindableActionMap::NEXT_LEASH)), LeashKey::NextLeash);
}

TEST(LeashKeysPressed, NoKeyIsNone)
{
	EXPECT_EQ(creature_leash::PressedKey([](BindableActionMap) { return false; }), std::nullopt);
	// the quick load is Ctrl+L, not the leash
	EXPECT_EQ(creature_leash::PressedKey(Only(BindableActionMap::QUICK_LOAD)), std::nullopt);
}

TEST(LeashKeysPressed, LeashKeyComesFirst)
{
	EXPECT_EQ(creature_leash::PressedKey([](BindableActionMap) { return true; }), LeashKey::Leash);
	EXPECT_EQ(creature_leash::PressedKey(
	              [](BindableActionMap action) { return action != BindableActionMap::LEASH_UNLEASH_CREATURE; }),
	          LeashKey::PreviousLeash);
}

namespace
{
class LeashKeysInputTest: public ::testing::Test
{
protected:
	LeashKeysInputTest()
	{
		// no audio state: the local player is player one
		Locator::audioState::reset();
		leash = &static_cast<FakeLeash&>(Locator::leashSystem::emplace<FakeLeash>(log));
	}

	static constexpr auto k_Creature = static_cast<entt::entity>(3);

	CallLog log;
	FakeLeash* leash {nullptr};
	FakeActions actions;

private:
	const test::RestoreService<Locator::audioState> _restoreAudioState;
	const test::RestoreService<Locator::leashSystem> _restoreLeash;
};
} // namespace

TEST_F(LeashKeysInputTest, NoCreatureNoCall)
{
	actions.down = Bits(BindableActionMap::LEASH_UNLEASH_CREATURE);
	ecs::creature_loop::ProcessLeashKeys(actions);
	EXPECT_TRUE(log.empty());
}

TEST_F(LeashKeysInputTest, CreatureNotLeashableNoCall)
{
	leash->playersCreature = k_Creature;
	actions.down = Bits(BindableActionMap::LEASH_UNLEASH_CREATURE);
	ecs::creature_loop::ProcessLeashKeys(actions);
	EXPECT_TRUE(log.empty());
}

TEST_F(LeashKeysInputTest, LeashableCreatureGetsTheKey)
{
	leash->playersCreature = k_Creature;
	leash->leashable = true;
	actions.down = Bits(BindableActionMap::NEXT_LEASH);
	ecs::creature_loop::ProcessLeashKeys(actions);
	EXPECT_EQ(log,
	          (CallLog {{.name = "leash.PressKey",
	                     .args = {static_cast<float>(PlayerNames::PLAYER_ONE), static_cast<float>(LeashKey::NextLeash)}}}));
}

TEST_F(LeashKeysInputTest, HeldKeyIsNotPressedAgain)
{
	leash->playersCreature = k_Creature;
	leash->leashable = true;
	actions.down = Bits(BindableActionMap::LEASH_UNLEASH_CREATURE);
	actions.downBefore = actions.down;
	ecs::creature_loop::ProcessLeashKeys(actions);
	// let go of: changed, but not down
	actions.down = 0;
	ecs::creature_loop::ProcessLeashKeys(actions);
	EXPECT_TRUE(log.empty());
}

TEST_F(LeashKeysInputTest, NoLeashServiceNoCall)
{
	Locator::leashSystem::reset();
	actions.down = Bits(BindableActionMap::LEASH_UNLEASH_CREATURE);
	ecs::creature_loop::ProcessLeashKeys(actions);
	EXPECT_TRUE(log.empty());
}
