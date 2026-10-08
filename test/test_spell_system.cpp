/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spell system's state: the live spells, the sinks their particle effects keep a pointer to, the spell classes'
// operations and the spell presence grid, the class lists; and the magic objects' lists

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/MagicObjectsSystem.h"
#include "ECS/Systems/Implementations/SpellSystem.h"

using namespace openblack;
using openblack::ecs::systems::SpellSystem;

namespace
{
/// Counts its own destruction
class CountingSink final: public psys::SpellSink
{
public:
	explicit CountingSink(int& destroyed)
	    : _destroyed(destroyed)
	{
	}
	CountingSink(const CountingSink&) = delete;
	CountingSink& operator=(const CountingSink&) = delete;
	CountingSink(CountingSink&&) = delete;
	CountingSink& operator=(CountingSink&&) = delete;
	~CountingSink() override { ++_destroyed; }
	int SpellEvent(const psys::SpellEventInfo&) override { return 0; }
	[[nodiscard]] int PowerUpLevel() const override { return -1; }

private:
	int& _destroyed;
};
} // namespace

TEST(SpellSystem, startsEmpty)
{
	SpellSystem spells;
	EXPECT_TRUE(spells.Spells().empty());
}

TEST(SpellSystem, theListIsTheCallersToOrder)
{
	SpellSystem spells;
	auto& list = spells.Spells();
	list.insert(list.begin(), entt::entity {1});
	list.insert(list.begin(), entt::entity {2});
	EXPECT_EQ(spells.Spells(), (std::vector<entt::entity> {entt::entity {2}, entt::entity {1}}));
}

TEST(SpellSystem, aSinkStaysWhereItIsWhileOthersAreAdded)
{
	SpellSystem spells;
	int destroyed = 0;
	auto first = std::make_unique<CountingSink>(destroyed);
	const psys::SpellSink* const kept = first.get();
	spells.SetSink(entt::entity {0}, std::move(first));
	for (uint32_t i = 1; i < 200; ++i)
	{
		spells.SetSink(entt::entity {i}, std::make_unique<CountingSink>(destroyed));
	}
	// the particle system's pointer still reaches the same sink
	EXPECT_EQ(kept->PowerUpLevel(), -1);
	EXPECT_EQ(destroyed, 0);
}

TEST(SpellSystem, aNewSinkReplacesTheOld)
{
	SpellSystem spells;
	int destroyed = 0;
	spells.SetSink(entt::entity {3}, std::make_unique<CountingSink>(destroyed));
	spells.SetSink(entt::entity {3}, std::make_unique<CountingSink>(destroyed));
	EXPECT_EQ(destroyed, 1);
	spells.EraseSink(entt::entity {3});
	EXPECT_EQ(destroyed, 2);
	spells.EraseSink(entt::entity {3});
	EXPECT_EQ(destroyed, 2);
}

TEST(SpellSystem, clearEmptiesTheListAndTheSinks)
{
	SpellSystem spells;
	int destroyed = 0;
	spells.Spells().push_back(entt::entity {5});
	spells.SetSink(entt::entity {5}, std::make_unique<CountingSink>(destroyed));
	spells.Clear();
	EXPECT_TRUE(spells.Spells().empty());
	EXPECT_EQ(destroyed, 1);
}

TEST(SpellSystem, theClassesRegisterOncePerSystem)
{
	SpellSystem spells;
	EXPECT_TRUE(spells.TakeOpsRegistration());
	EXPECT_FALSE(spells.TakeOpsRegistration());
	EXPECT_FALSE(spells.TakeOpsRegistration());
	// a new game's system registers them again
	SpellSystem next;
	EXPECT_TRUE(next.TakeOpsRegistration());
}

TEST(SpellSystem, theOperationsStartEmptyAndSurviveClear)
{
	SpellSystem spells;
	EXPECT_EQ(spells.Ops(ecs::components::SpellClass::General).process, nullptr);
	spells.Ops(ecs::components::SpellClass::General).process = [](entt::entity) { return 5; };
	spells.Clear();
	ASSERT_NE(spells.Ops(ecs::components::SpellClass::General).process, nullptr);
	EXPECT_EQ(spells.Ops(ecs::components::SpellClass::General).process(entt::null), 5);
}

TEST(SpellSystem, theNotPortedWarningIsTakenOncePerClass)
{
	SpellSystem spells;
	EXPECT_TRUE(spells.TakeNotPortedWarning(ecs::components::SpellClass::General));
	EXPECT_FALSE(spells.TakeNotPortedWarning(ecs::components::SpellClass::General));
	EXPECT_TRUE(spells.TakeNotPortedWarning(ecs::components::SpellClass::Heal));
}

TEST(SpellGrid, aMarkIsReadBackInItsCell)
{
	magic::SpellGrid grid;
	grid.Mark(glm::vec3(85.0f, 0.0f, 165.0f), 0xFF);
	EXPECT_EQ(grid.At(glm::vec3(85.0f, 0.0f, 165.0f)), 0xFF);
	// the same 80 m cell
	EXPECT_EQ(grid.At(glm::vec3(80.0f, 0.0f, 160.0f)), 0xFF);
	EXPECT_EQ(grid.At(glm::vec3(79.0f, 0.0f, 165.0f)), 0);
}

TEST(SpellGrid, outsideTheGridNothingIsMarked)
{
	magic::SpellGrid grid;
	grid.Mark(glm::vec3(-1.0f, 0.0f, 10.0f), 0xFF);
	grid.Mark(glm::vec3(64.0f * 80.0f, 0.0f, 10.0f), 0xFF);
	EXPECT_EQ(grid.At(glm::vec3(-1.0f, 0.0f, 10.0f)), 0);
	EXPECT_EQ(grid.At(glm::vec3(64.0f * 80.0f, 0.0f, 10.0f)), 0);
	EXPECT_EQ(grid.At(glm::vec3(0.0f, 0.0f, 10.0f)), 0);
}

TEST(SpellGrid, eachDecayTakesTheStartDecay)
{
	magic::SpellGrid grid;
	const glm::vec3 at(10.0f, 0.0f, 10.0f);
	grid.Mark(at, 0x50);
	grid.Decay();
	EXPECT_EQ(grid.At(at), 0x30);
	grid.Decay();
	EXPECT_EQ(grid.At(at), 0x10);
	grid.Decay();
	EXPECT_EQ(grid.At(at), 0);
}

TEST(SpellGrid, clearEmptiesEveryCell)
{
	magic::SpellGrid grid;
	grid.Mark(glm::vec3(10.0f, 0.0f, 10.0f), 0x40);
	grid.Clear();
	EXPECT_EQ(grid.At(glm::vec3(10.0f, 0.0f, 10.0f)), 0);
}

TEST(SpellSystem, theClassListsAreSeparateAndOutliveClear)
{
	SpellSystem spells;
	spells.ShieldSpells().push_back(entt::entity {1});
	spells.StormSpells().push_back(entt::entity {2});
	spells.Clear();
	EXPECT_EQ(spells.ShieldSpells(), (std::vector<entt::entity> {entt::entity {1}}));
	EXPECT_EQ(spells.StormSpells(), (std::vector<entt::entity> {entt::entity {2}}));
}

TEST(MagicObjectsSystem, theShieldsAndTheFireBallsAreSeparateLists)
{
	ecs::systems::MagicObjectsSystem objects;
	EXPECT_TRUE(objects.Shields().empty());
	EXPECT_TRUE(objects.FireBalls().empty());
	objects.Shields().push_back(entt::entity {4});
	EXPECT_EQ(objects.Shields(), (std::vector<entt::entity> {entt::entity {4}}));
	EXPECT_TRUE(objects.FireBalls().empty());
}
