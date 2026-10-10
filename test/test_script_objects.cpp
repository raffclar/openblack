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
#include <set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/ScriptSpotVisual.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ScriptObjectTable.h"
#include "ECS/ScriptObjectsWorld.h"
#include "ECS/ScriptSpotVisuals.h"
#include "ECS/Systems/Implementations/ScriptObjectsSystem.h"

using namespace openblack::ecs::script_objects;
using openblack::ecs::systems::ScriptObjectsSystem;

namespace
{
enum class Container : uint8_t
{
	None,
	Flock,
	Dance,
	Town,
};

/// A world of numbered objects, recording what the table does to them
struct FakeWorld final: World
{
	struct Object
	{
		Kind kind {Kind::Other};
		bool available {true};
		bool inScript {false};
		bool controlled {false};
		bool inPhysics {false};
		bool inMap {true};
		Container container {Container::None};
		std::vector<entt::entity> members;
		/// A marker or a timer
		bool deletedWhenReleased {false};
		bool highlight {false};
	};
	std::map<entt::entity, Object> objects;
	std::vector<entt::entity> decided;
	std::vector<entt::entity> decideAfterPhysics;
	std::vector<entt::entity> abandoned;
	std::vector<entt::entity> deleted;
	/// The members that left a container, waiting for their script
	std::vector<entt::entity> waiting;

	entt::entity Add(uint32_t id, Object object)
	{
		const auto entity = static_cast<entt::entity>(id);
		objects[entity] = object;
		return entity;
	}
	entt::entity Add(uint32_t id) { return Add(id, Object {}); }

	[[nodiscard]] bool Exists(entt::entity object) const override { return objects.contains(object); }
	[[nodiscard]] bool IsAvailable(entt::entity object) const override
	{
		return Exists(object) && objects.at(object).available;
	}
	[[nodiscard]] Kind KindOf(entt::entity object) const override { return objects.at(object).kind; }
	[[nodiscard]] bool IsInScript(entt::entity object) const override { return objects.at(object).inScript; }
	void SetInScript(entt::entity object, bool inScript) override { objects.at(object).inScript = inScript; }
	[[nodiscard]] bool IsControlled(entt::entity object) const override { return objects.at(object).controlled; }
	void SetControlled(entt::entity object, bool controlled) override { objects.at(object).controlled = controlled; }
	[[nodiscard]] bool IsInPhysics(entt::entity object) const override { return objects.at(object).inPhysics; }
	[[nodiscard]] bool IsInMap(entt::entity object) const override { return objects.at(object).inMap; }
	void SetVillagerDecideWhatToDo(entt::entity villager) override { decided.push_back(villager); }
	void SetVillagerPreviousDecideWhatToDo(entt::entity villager) override { decideAfterPhysics.push_back(villager); }
	void AbandonCreatureAction(entt::entity creature) override { abandoned.push_back(creature); }
	void Delete(entt::entity object) override
	{
		deleted.push_back(object);
		objects.erase(object);
	}
	[[nodiscard]] bool IsContainer(entt::entity object) const override
	{
		return objects.at(object).container != Container::None;
	}
	std::optional<std::vector<entt::entity>> Disband(entt::entity container) override
	{
		auto& object = objects.at(container);
		if (object.container == Container::None)
		{
			return std::nullopt;
		}
		if (object.container == Container::Town)
		{
			return std::vector<entt::entity> {};
		}
		auto left = std::move(object.members);
		object.members.clear();
		for (const auto member : left)
		{
			if (objects.at(member).controlled)
			{
				waiting.push_back(member);
			}
		}
		return left;
	}
	[[nodiscard]] std::vector<entt::entity> FlockMembers(entt::entity object) const override
	{
		const auto& found = objects.at(object);
		return found.container == Container::Flock ? found.members : std::vector<entt::entity> {};
	}
	[[nodiscard]] bool IsDeletedWhenReleased(entt::entity object) const override
	{
		return objects.at(object).deletedWhenReleased;
	}
	[[nodiscard]] bool IsHighlight(entt::entity object) const override { return objects.at(object).highlight; }
};

struct System
{
	FakeWorld* world;
	std::unique_ptr<ScriptObjectsSystem> system;
};

System MakeSystem()
{
	// The table reports its errors to the scripts' log
	if (spdlog::get("scripting") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	auto world = std::make_unique<FakeWorld>();
	auto* raw = world.get();
	return {.world = raw, .system = std::make_unique<ScriptObjectsSystem>(std::move(world))};
}

constexpr uint32_t k_MoveNative = 33;
constexpr uint32_t k_CreateNative = 27;
} // namespace

TEST(ScriptObjects, OnlyTheNativesThatMoveOrSetObjectsTakeControl)
{
	EXPECT_TRUE(TakesControl(17));  // set script state
	EXPECT_TRUE(TakesControl(33));  // move a thing
	EXPECT_TRUE(TakesControl(218)); // start a refereed match
	EXPECT_FALSE(TakesControl(27)); // create
	EXPECT_FALSE(TakesControl(110));
}

TEST(ScriptObjects, AnObjectKeepsItsPlace)
{
	Table table;
	const auto first = table.Register(42, true);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(*first, 1);
	// The scripts know it by the object itself, so it never takes a second place
	EXPECT_EQ(table.Register(42, false), first);
	EXPECT_TRUE(table.At(*first).createdByScript);
	EXPECT_EQ(table.Register(43, false), 2);
}

TEST(ScriptObjects, TheFirstReferenceIsTold)
{
	Table table;
	const auto place = *table.Register(7, false);
	EXPECT_EQ(table.AddReference(place), Referenced::First);
	EXPECT_EQ(table.AddReference(place), Referenced::Again);
	table.RemoveReference(place);
	table.RemoveReference(place);
	table.RemoveReference(place);
	EXPECT_EQ(table.At(place).count, 0);
	// Let go of every reference, the place is still the object's until it is freed
	EXPECT_EQ(table.Find(7), place);
	EXPECT_EQ(table.Unreferenced(), std::vector<uint16_t> {place});
	table.Free(place);
	EXPECT_FALSE(table.Find(7).has_value());
	EXPECT_TRUE(table.Unreferenced().empty());
	EXPECT_EQ(table.AddReference(0), Referenced::Nothing);
}

TEST(ScriptObjects, TheTableFillsAndIsClearedWithTheProgram)
{
	Table table;
	for (uint32_t object = 1; object < k_Places; ++object)
	{
		ASSERT_TRUE(table.Register(object, false).has_value());
	}
	EXPECT_FALSE(table.Register(9999, false).has_value());
	table.Clear();
	// Every place is free, and the search starts at the first again
	EXPECT_EQ(table.Register(9999, false), 1);
}

TEST(ScriptObjects, ANewLandKeepsThePlacesStillCounted)
{
	Table table;
	const auto counted = *table.Register(5, false);
	table.AddReference(counted);
	const auto free = *table.Register(6, false);
	table.ClearObjects();
	EXPECT_FALSE(table.Find(5).has_value());
	EXPECT_EQ(table.At(counted).count, 1);
	// The search goes on from where it stopped: past the counted place, the other is free again once it comes round
	const auto next = table.Register(7, false);
	ASSERT_TRUE(next.has_value());
	EXPECT_NE(*next, counted);
	EXPECT_EQ(*next, free);
}

TEST(ScriptObjects, ADeadTreeTakesItsTreesPlace)
{
	Table table;
	const auto place = *table.Register(5, false);
	table.Replace(5, 6);
	EXPECT_EQ(table.Find(6), place);
	EXPECT_FALSE(table.Find(5).has_value());
}

TEST(ScriptObjectsSystem, AFoundObjectIsInAScriptButNotControlledAtItsFirstReference)
{
	auto [world, system] = MakeSystem();
	const auto object = world->Add(3);
	system->AddReference(object);
	EXPECT_TRUE(world->objects.at(object).inScript);
	EXPECT_FALSE(world->objects.at(object).controlled);
}

TEST(ScriptObjectsSystem, AnObjectAScriptMadeIsControlledAtItsFirstReference)
{
	auto [world, system] = MakeSystem();
	const auto object = world->Add(3);
	ASSERT_TRUE(system->Register(object, true));
	system->AddReference(object);
	EXPECT_TRUE(world->objects.at(object).controlled);
}

TEST(ScriptObjectsSystem, OnlyAControllingNativeTakesControlOfWhatItIsGiven)
{
	auto [world, system] = MakeSystem();
	const auto object = world->Add(3);
	system->EnterNative(k_CreateNative);
	system->Fetch(object);
	EXPECT_FALSE(world->objects.at(object).controlled);
	system->EnterNative(k_MoveNative);
	EXPECT_EQ(system->Fetch(object), object);
	EXPECT_TRUE(world->objects.at(object).controlled);
}

TEST(ScriptObjectsSystem, AnObjectThatHasGoneIsNoObject)
{
	auto [world, system] = MakeSystem();
	const auto object = world->Add(3);
	world->Delete(object);
	system->EnterNative(k_MoveNative);
	EXPECT_TRUE(system->Fetch(object) == entt::null);
	EXPECT_TRUE(system->Fetch(static_cast<entt::entity>(99999)) == entt::null);
}

TEST(ScriptObjectsSystem, AnObjectNoLongerToBeDealtWithIsNoObjectAndStaysUncontrolled)
{
	auto [world, system] = MakeSystem();
	const auto villager = world->Add(4, {.kind = Kind::Villager, .available = false});
	system->EnterNative(k_MoveNative);
	EXPECT_TRUE(system->Fetch(villager) == entt::null);
	EXPECT_FALSE(world->objects.at(villager).controlled);
}

TEST(ScriptObjectsSystem, AReleasedVillagerInTheMapDecidesWhatToDo)
{
	auto [world, system] = MakeSystem();
	const auto villager = world->Add(4, {.kind = Kind::Villager, .controlled = true});
	system->ReleaseFromScript(villager);
	EXPECT_FALSE(world->objects.at(villager).controlled);
	EXPECT_EQ(world->decided, std::vector {villager});
	EXPECT_TRUE(world->decideAfterPhysics.empty());
}

TEST(ScriptObjectsSystem, AReleasedVillagerInThePhysicsDecidesWhatToDoOnceOut)
{
	auto [world, system] = MakeSystem();
	const auto villager = world->Add(4, {.kind = Kind::Villager, .controlled = true, .inPhysics = true, .inMap = false});
	system->ReleaseFromScript(villager);
	EXPECT_TRUE(world->decided.empty());
	EXPECT_EQ(world->decideAfterPhysics, std::vector {villager});
}

TEST(ScriptObjectsSystem, AReleasedVillagerHeldOutOfTheMapOnlyLosesControl)
{
	auto [world, system] = MakeSystem();
	const auto held = world->Add(4, {.kind = Kind::Villager, .controlled = true, .inMap = false});
	const auto dying = world->Add(5, {.kind = Kind::Villager, .available = false, .controlled = true});
	system->ReleaseFromScript(held);
	system->ReleaseFromScript(dying);
	EXPECT_FALSE(world->objects.at(held).controlled);
	EXPECT_TRUE(world->decided.empty());
	EXPECT_TRUE(world->decideAfterPhysics.empty());
}

TEST(ScriptObjectsSystem, AReleasedCreatureGivesUpWhatItWasDoing)
{
	auto [world, system] = MakeSystem();
	const auto creature = world->Add(6, {.kind = Kind::Creature, .controlled = true});
	const auto free = world->Add(7, {.kind = Kind::Creature});
	system->ReleaseFromScript(creature);
	system->ReleaseFromScript(free);
	EXPECT_EQ(world->abandoned, std::vector {creature});
}

TEST(ScriptObjectsSystem, ClearingTheScriptsDeletesWhatTheyMadeAndLetsGoOfTheRest)
{
	auto [world, system] = MakeSystem();
	const auto made = world->Add(10);
	const auto villager = world->Add(11, {.kind = Kind::Villager});
	const auto rock = world->Add(12);
	ASSERT_TRUE(system->Register(made, true));
	system->AddReference(made);
	system->AddReference(villager);
	system->EnterNative(k_MoveNative);
	system->Fetch(villager);
	system->AddReference(rock);
	system->Reset();
	EXPECT_EQ(world->deleted, std::vector {made});
	EXPECT_FALSE(world->objects.at(villager).controlled);
	EXPECT_FALSE(world->objects.at(villager).inScript);
	EXPECT_EQ(world->decided, std::vector {villager});
	EXPECT_FALSE(world->objects.at(rock).inScript);
	// Every place is free again: the villager is given the first place anew
	system->AddReference(villager);
	EXPECT_TRUE(world->objects.at(villager).inScript);
	EXPECT_FALSE(world->objects.at(villager).controlled);
}

TEST(ScriptObjectsSystem, ANewLandLetsNothingGoBackIntoTheGame)
{
	auto [world, system] = MakeSystem();
	const auto made = world->Add(10);
	const auto villager = world->Add(11, {.kind = Kind::Villager});
	ASSERT_TRUE(system->Register(made, true));
	system->AddReference(made);
	system->AddReference(villager);
	system->EnterNative(k_MoveNative);
	system->Fetch(villager);
	system->ClearForNewLand();
	// The land's objects went with it: nothing is deleted or released by the scripts
	EXPECT_TRUE(world->deleted.empty());
	EXPECT_TRUE(world->decided.empty());
}

TEST(ScriptObjectsSystem, ADeadTreeTakesOnlyItsTreesPlace)
{
	auto [world, system] = MakeSystem();
	const auto tree = world->Add(20);
	const auto dead = world->Add(21);
	ASSERT_TRUE(system->Register(tree, true));
	system->AddReference(tree);
	system->Replace(tree, dead);
	EXPECT_FALSE(world->objects.at(dead).inScript);
	EXPECT_FALSE(world->objects.at(dead).controlled);
	// Its next reference puts it in a script again, but control only comes with a place's first reference
	system->AddReference(dead);
	EXPECT_TRUE(world->objects.at(dead).inScript);
	EXPECT_FALSE(world->objects.at(dead).controlled);
}

TEST(ScriptObjectsSystem, AVisualAScriptKeepsPutsOnlyItsOwnThingInTheScript)
{
	// A land's things, the visual's own number among them
	openblack::ecs::Registry registry;
	constexpr uint32_t k_Things = 152;
	constexpr uint32_t k_Effect = 86;
	auto [world, system] = MakeSystem();
	for (uint32_t i = 0; i < k_Things; ++i)
	{
		world->Add(entt::to_integral(registry.Create()));
	}
	// The script is given the visual's thing, not the visual's number
	const auto thing = openblack::ecs::script_spot_visuals::MakeThing(registry, k_Effect, {1.0f, 2.0f, 3.0f});
	EXPECT_EQ(registry.Get<openblack::ecs::components::ScriptSpotVisual>(thing).effect, k_Effect);
	EXPECT_EQ(registry.Get<openblack::ecs::components::Transform>(thing).position, glm::vec3(1.0f, 2.0f, 3.0f));
	world->Add(entt::to_integral(thing));
	ASSERT_TRUE(system->Register(thing, true));
	// Keeping it in a variable references the thing alone
	system->AddReference(thing);
	EXPECT_TRUE(world->objects.at(thing).inScript);
	for (const auto& [entity, object] : world->objects)
	{
		EXPECT_TRUE(entity == thing || !object.inScript) << entt::to_integral(entity);
	}
}

TEST(ScriptObjectsSystem, AVisualsThingGoesWhenItsVisualEnds)
{
	openblack::ecs::Registry registry;
	const auto ending = openblack::ecs::script_spot_visuals::MakeThing(registry, 4, glm::vec3(0.0f));
	const auto lasting = openblack::ecs::script_spot_visuals::MakeThing(registry, 5, glm::vec3(0.0f));
	openblack::ecs::script_spot_visuals::RemoveEnded(registry, [](uint32_t effect) { return effect == 5; });
	EXPECT_FALSE(registry.Valid(ending));
	EXPECT_TRUE(registry.Valid(lasting));
}

TEST(ScriptObjectsSystem, AnObjectNoVariableHoldsFreesItsPlaceAfterTheScriptsTurn)
{
	auto [world, system] = MakeSystem();
	const auto found = world->Add(3);
	const auto kept = world->Add(4);
	ASSERT_TRUE(system->Register(found, false));
	ASSERT_TRUE(system->Register(kept, false));
	system->AddReference(kept);
	system->ReleaseUnreferenced();
	const auto places = system->Places();
	ASSERT_EQ(places.size(), 1U);
	EXPECT_TRUE(places.front().object == kept);
	EXPECT_EQ(places.front().references, 1);
	EXPECT_TRUE(world->objects.at(kept).inScript);
	// Its variable let go, the kept one goes too, and is in no script
	system->RemoveReference(kept);
	system->ReleaseUnreferenced();
	EXPECT_TRUE(system->Places().empty());
	EXPECT_FALSE(world->objects.at(kept).inScript);
	EXPECT_TRUE(world->deleted.empty());
}

TEST(ScriptObjectsSystem, ATableKeptFreeNeverFills)
{
	// A script that makes a marker each time round its loop, keeping only the last
	auto [world, system] = MakeSystem();
	entt::entity last = entt::null;
	for (uint32_t i = 0; i < 5 * k_Places; ++i)
	{
		const auto marker = world->Add(1000 + i, {.deletedWhenReleased = true});
		ASSERT_TRUE(system->Register(marker, true)) << i;
		system->AddReference(marker);
		if (last != entt::null)
		{
			system->RemoveReference(last);
		}
		last = marker;
		system->ReleaseUnreferenced();
	}
	EXPECT_EQ(system->TimesFull(), 0U);
	EXPECT_EQ(system->Places().size(), 1U);
	// Every marker the script let go of went with it
	EXPECT_EQ(world->deleted.size(), static_cast<std::size_t>(5 * k_Places - 1));
	EXPECT_EQ(world->objects.size(), 1U);
}

TEST(ScriptObjectsSystem, ATableNeverFreedFillsAndSaysSo)
{
	auto [world, system] = MakeSystem();
	for (uint32_t i = 1; i < k_Places; ++i)
	{
		const auto object = world->Add(i);
		ASSERT_TRUE(system->Register(object, false));
		system->AddReference(object);
	}
	EXPECT_FALSE(system->Register(world->Add(9999), false));
	EXPECT_EQ(system->TimesFull(), 1U);
	system->ReleaseUnreferenced();
	EXPECT_EQ(system->Places().size(), k_Places - 1U);
	system->Reset();
	EXPECT_EQ(system->TimesFull(), 0U);
}

TEST(ScriptObjectsSystem, AMarkerOrTimerLetGoOfGoes)
{
	auto [world, system] = MakeSystem();
	const auto marker = world->Add(3, {.deletedWhenReleased = true});
	const auto found = world->Add(4, {.deletedWhenReleased = true});
	ASSERT_TRUE(system->Register(marker, true));
	system->AddReference(marker);
	// A marker a script didn't make is only let go of while no script controls it
	ASSERT_TRUE(system->Register(found, false));
	system->AddReference(found);
	system->RemoveReference(marker);
	system->RemoveReference(found);
	system->ReleaseUnreferenced();
	EXPECT_EQ(world->deleted, std::vector {marker});
	EXPECT_FALSE(world->objects.at(found).inScript);
}

TEST(ScriptObjectsSystem, AHighlightGoesOnlyWhenTheScriptMadeAndStillControlsIt)
{
	auto [world, system] = MakeSystem();
	const auto held = world->Add(3, {.highlight = true});
	const auto released = world->Add(4, {.highlight = true});
	for (const auto highlight : {held, released})
	{
		ASSERT_TRUE(system->Register(highlight, true));
		system->AddReference(highlight);
	}
	// A sign the script released stays up once its script ends
	system->ReleaseFromScript(released);
	system->RemoveReference(held);
	system->RemoveReference(released);
	system->ReleaseUnreferenced();
	EXPECT_EQ(world->deleted, std::vector {held});
	EXPECT_FALSE(world->objects.at(released).inScript);
}

TEST(ScriptObjectsSystem, AnythingElseAScriptMadeGoesBackIntoTheGame)
{
	auto [world, system] = MakeSystem();
	const auto villager = world->Add(3, {.kind = Kind::Villager});
	ASSERT_TRUE(system->Register(villager, true));
	system->AddReference(villager);
	EXPECT_TRUE(world->objects.at(villager).controlled);
	system->RemoveReference(villager);
	system->ReleaseUnreferenced();
	EXPECT_TRUE(world->deleted.empty());
	EXPECT_FALSE(world->objects.at(villager).controlled);
	EXPECT_FALSE(world->objects.at(villager).inScript);
	EXPECT_EQ(world->decided, std::vector {villager});
}

TEST(ScriptObjectsSystem, AFlockAScriptMadeIsDisbandedAndGoes)
{
	auto [world, system] = MakeSystem();
	const auto member = world->Add(5, {.kind = Kind::Villager});
	const auto flock = world->Add(4, {.container = Container::Flock, .members = {member}});
	ASSERT_TRUE(system->Register(flock, true));
	system->AddReference(flock);
	// A member keeps a reference while in the flock
	system->AddReference(member);
	system->RemoveReference(flock);
	system->ReleaseUnreferenced();
	EXPECT_EQ(world->deleted, std::vector {flock});
	// The member let go of its reference, and its place is freed in the same pass, coming after the flock's
	EXPECT_TRUE(system->Places().empty());
	EXPECT_FALSE(world->objects.at(member).inScript);
}

TEST(ScriptObjectsSystem, AFlockNoScriptControlsLetsGoOfItsMembersReferences)
{
	auto [world, system] = MakeSystem();
	const auto member = world->Add(5);
	const auto flock = world->Add(4, {.container = Container::Flock, .members = {member}});
	system->AddReference(member);
	system->AddReference(flock);
	system->RemoveReference(flock);
	system->ReleaseUnreferenced();
	// The member came first in the table, so its place is freed at the next turn; the flock stays
	EXPECT_TRUE(world->deleted.empty());
	EXPECT_EQ(world->objects.at(flock).members, std::vector {member});
	ASSERT_EQ(system->Places().size(), 1U);
	EXPECT_EQ(system->Places().front().references, 0);
	system->ReleaseUnreferenced();
	EXPECT_TRUE(system->Places().empty());
}

TEST(ScriptObjectsSystem, DisbandingLetsEveryMemberGo)
{
	auto [world, system] = MakeSystem();
	const auto controlled = world->Add(5, {.kind = Kind::Villager, .controlled = true});
	const auto free = world->Add(6, {.kind = Kind::Villager});
	const auto dance = world->Add(4, {.container = Container::Dance, .members = {controlled, free}});
	const auto town = world->Add(7, {.container = Container::Town});
	const auto rock = world->Add(8);
	system->AddReference(controlled);
	system->AddReference(free);
	EXPECT_TRUE(system->Disband(dance));
	EXPECT_TRUE(world->objects.at(dance).members.empty());
	EXPECT_EQ(world->waiting, std::vector {controlled});
	for (const auto& place : system->Places())
	{
		EXPECT_EQ(place.references, 0);
	}
	EXPECT_TRUE(system->Disband(town));
	EXPECT_FALSE(system->Disband(rock));
}
