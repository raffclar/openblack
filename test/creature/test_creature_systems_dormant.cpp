/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Every pass the game loop makes over the creatures (ECS/CreatureLoop.h), on a land with villagers and a temple but no
// creature: no storage and no entity is made, no Transform is added, and no random stream moves. This is why the
// creatures' turn and frame leave Land 1 and Land 2 as they were.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/sparse_set.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureHandPackets.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/CreatureHairSystem.h"
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "ECS/Systems/Implementations/CreatureLocomotionSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/CreatureSkinSystem.h"
#include "ECS/Systems/Implementations/FootprintSystem.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "Input/GamePackets.h"
#include "Magic/Spells/SpellCreature.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace std::chrono_literals;
namespace fakes = openblack::test::creature_fakes;

namespace
{
/// What a pass over no creature must leave as it was
struct Snapshot
{
	/// Every storage of the registry, the entities' own first, with how many it holds
	std::vector<std::pair<entt::id_type, size_t>> storages;
	uint32_t synced {0};
	uint32_t local {0};
	uint32_t crt {0};
	size_t transforms {0};
};

Snapshot Take()
{
	Snapshot snapshot;
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	registry.EachStorage(
	    [&snapshot](entt::id_type id, const entt::sparse_set& storage) { snapshot.storages.emplace_back(id, storage.size()); });
	snapshot.synced = game_random::Current().synced;
	snapshot.local = game_random::Current().local;
	snapshot.crt = game_random::crt::Seed();
	snapshot.transforms = registry.Size<Transform>();
	return snapshot;
}

class CreatureSystemsDormantTest: public ::testing::Test
{
protected:
	CreatureSystemsDormantTest()
	{
		// the services the creature systems ask for, so that their passes go past the "is it there" tests
		Locator::handSystem::emplace<fakes::FakeHand>();
		Locator::creatureAnimationSystem::emplace<fakes::FakeAnimation>();
		Locator::creatureLocomotionSystem::emplace<fakes::FakeLocomotion>();
		Locator::creatureSkinSystem::emplace<fakes::FakeSkin>();
		Locator::creatureMindSystem::emplace<fakes::FakeMind>();
		Locator::creatureObjectActionSystem::emplace<fakes::FakeObjectAction>();

		// a land with no creature: a villager and a temple
		auto& registry = test::creature_world::World::Registry();
		const auto villager = registry.Create();
		registry.Assign<Transform>(villager, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Villager>(villager);
		const auto temple = registry.Create();
		registry.Assign<Temple>(temple, PlayerNames::PLAYER_ONE);
		registry.Assign<Transform>(temple, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(temple, entt::id_type {0}, int8_t {0}, int8_t {0});
	}

	/// The pass leaves the registry and the random streams as they were
	template <typename Pass>
	static void ExpectDormant(Pass pass)
	{
		const auto before = Take();
		pass();
		const auto after = Take();
		EXPECT_EQ(after.storages, before.storages);
		EXPECT_EQ(after.synced, before.synced);
		EXPECT_EQ(after.local, before.local);
		EXPECT_EQ(after.crt, before.crt);
		EXPECT_EQ(after.transforms, before.transforms);
	}

	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;

private:
	const test::RestoreService<Locator::handSystem> _restoreHand;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	const test::RestoreService<Locator::creatureSkinSystem> _restoreSkin;
	const test::RestoreService<Locator::creatureMindSystem> _restoreMind;
	const test::RestoreService<Locator::creatureObjectActionSystem> _restoreObjectAction;
};
} // namespace

TEST_F(CreatureSystemsDormantTest, Physiology)
{
	CreaturePhysiologySystem system;
	ExpectDormant([&system]() { system.ProcessTurn(); });
	ExpectDormant([&system]() { system.Update(0.016f); });
}

TEST_F(CreatureSystemsDormantTest, Animation)
{
	CreatureAnimationSystem system;
	ExpectDormant([&system]() { system.ProcessTurn(); });
	ExpectDormant([&system]() { system.Update(16ms); });
}

TEST_F(CreatureSystemsDormantTest, Skin)
{
	CreatureSkinSystem system;
	ExpectDormant([&system]() { system.ProcessTurn(); });
	ExpectDormant([&system]() { system.Update(); });
}

TEST_F(CreatureSystemsDormantTest, LeashPlacesNoPostsAtTheTemple)
{
	LeashSystem system;
	ExpectDormant([&system]() { system.ProcessTurn(); });
	ExpectDormant([&system]() { system.Update(0.016f); });
	ExpectDormant([&system]() { EXPECT_FALSE(system.LastRefusal(PlayerNames::PLAYER_ONE).has_value()); });
}

TEST_F(CreatureSystemsDormantTest, Mind)
{
	CreatureMindSystem system;
	ExpectDormant([&system]() { system.ProcessTurn(); });
	ExpectDormant([&system]() { system.PlanTurn(); });
	ExpectDormant([&system]() { system.LearnTurn(); });
}

TEST_F(CreatureSystemsDormantTest, Locomotion)
{
	CreatureLocomotionSystem system;
	ExpectDormant([&system]() { system.ProcessTurn(); });
	ExpectDormant([&system]() { system.Update(0.5f); });
}

TEST_F(CreatureSystemsDormantTest, ObjectActions)
{
	CreatureObjectActionSystem system;
	ExpectDormant([&system]() { system.ProcessTurn(); });
	ExpectDormant([&system]() { system.UpdateDraw(0.5f); });
	ExpectDormant([&system]() { system.UpdateHeldDraw(); });
}

TEST_F(CreatureSystemsDormantTest, Fight)
{
	CreatureFightSystem system;
	ExpectDormant([&system]() { system.ProcessTurn(); });
	ExpectDormant([&system]() { system.Update(0.5f, 16.0f); });
	ExpectDormant([&system]() { system.Release(); });
	ExpectDormant([&system]() { EXPECT_FALSE(system.IsPressed()); });
}

TEST_F(CreatureSystemsDormantTest, Hair)
{
	CreatureHairSystem system;
	ExpectDormant([&system]() { system.Update(16ms); });
}

TEST_F(CreatureSystemsDormantTest, Audio)
{
	CreatureAudioSystem system;
	ExpectDormant([&system]() { system.Update(16ms); });
}

TEST_F(CreatureSystemsDormantTest, FootprintsAndANewLand)
{
	FootprintSystem system([]() { return CalendarDate {}; });
	ExpectDormant([&system]() { system.Update(16ms); });
	ExpectDormant([&system]() { system.Reset(); });
}

TEST_F(CreatureSystemsDormantTest, CreatureSpells)
{
	ExpectDormant([]() { magic::spell_creature::ProcessTurn(); });
}

TEST_F(CreatureSystemsDormantTest, CreatureHandAndItsPackets)
{
	CreatureHandSystem system;
	fakes::FakeMind mind;
	const auto thing = test::creature_world::World::Registry().Create();
	const auto pending = game_packets::Pending();
	ExpectDormant([&]() {
		EXPECT_FALSE(system.CreatureAlong(glm::vec3(10.0f, 5.0f, 30.0f), glm::vec3(0.0f, 0.0f, -1.0f)).has_value());
		EXPECT_FALSE(system.MayHold(thing));
		EXPECT_FALSE(system.Grab(thing));
		EXPECT_FALSE(system.GetCreature().has_value());
		system.SetCreatureUnderHand(std::nullopt);
		EXPECT_FALSE(system.CreatureUnderHand().has_value());
		EXPECT_FALSE(
		    system.Update(glm::vec3(10.0f, 5.0f, 30.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f), 0.016f).has_value());
		EXPECT_FALSE(system.Release().has_value());
		EXPECT_FLOAT_EQ(system.GetLastFeedbackSum(), 0.0f);
		// a feedback packet for something that is not a creature reaches no mind
		game_packets::Packet packet {game_packets::Type::CreatureFeedback, thing};
		packet.data[0] = 1.0f;
		ecs::creature_hand_packets::ApplyFeedback(&mind, packet);
	});
	EXPECT_TRUE(mind.feedback.empty());
	EXPECT_EQ(game_packets::Pending(), pending);
}
