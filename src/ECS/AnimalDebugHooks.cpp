/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Debug/DebugEnv.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace
{
/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct AnimalDebugHooksState
{
	entt::entity foodBehindObstacleTracked {entt::null};
	entt::entity scriptHeldTestHeld {entt::null};
};

AnimalDebugHooksState& AnimalDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<AnimalDebugHooksState>();
}
} // namespace

namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Life;
using components::Transform;

namespace
{
std::optional<entt::entity> NthAnimal(int wanted)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> found;
	int index = 0;
	// OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>: count only that species
	static const char* species = std::getenv("OPENBLACK_TEST_ANIMAL_SPECIES");
	registry.Each<const Animal, const Transform>([&](entt::entity e, const Animal& animal, const Transform&) {
		if (species != nullptr && static_cast<int>(animal.type) != std::atoi(species))
		{
			return;
		}
		if (!found && index++ == wanted)
		{
			found = e;
		}
	});
	return found;
}

/// OPENBLACK_TEST_FOOD_BEHIND (RunDebugHooks)
void FoodBehindObstacle(const char* spec, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& tracked = AnimalDebugHooksData().foodBehindObstacleTracked;
	int wanted = 8;
	unsigned when = 3;
	int kind = 0;
	float behind = 6.0f;
	std::sscanf(spec, "%d,%u,%d,%f", &wanted, &when, &kind, &behind);
	if (turn == when)
	{
		std::optional<entt::entity> target;
		registry.Each<const Animal, const Transform>([&](entt::entity e, const Animal& animal, const Transform&) {
			if (!target && static_cast<int>(animal.type) == wanted)
			{
				target = e;
			}
		});
		if (!target)
		{
			return;
		}
		const glm::vec3 from = registry.Get<const Transform>(*target).position;
		std::optional<entt::entity> obstacle;
		float nearest = std::numeric_limits<float>::max();
		registry.Each<const components::Fixed, const Transform>(
		    [&](entt::entity e, const components::Fixed& fixed, const Transform&) {
			    const bool sized = fixed.boundingRadius >= 2.0f && fixed.boundingRadius <= 8.0f;
			    const bool right = kind == 1 ? registry.AllOf<components::Tree>(e)
			                       : kind == 2
			                           ? registry.AllOf<components::Feature>(e) && sized
			                           : registry.AllOf<components::Abode>(e) && !registry.AllOf<components::Field>(e) && sized;
			    const float d = glm::distance(fixed.boundingCenter, glm::vec2(from.x, from.z));
			    if (right && d < nearest)
			    {
				    nearest = d;
				    obstacle = e;
			    }
		    });
		if (!obstacle)
		{
			return;
		}
		const auto& fixed = registry.Get<const components::Fixed>(*obstacle);
		const glm::vec2 centre = kind == 1 ? glm::vec2(registry.Get<const Transform>(*obstacle).position.x,
		                                               registry.Get<const Transform>(*obstacle).position.z)
		                                   : fixed.boundingCenter;
		const float radius = kind == 1 ? 0.3f : fixed.boundingRadius;
		for (int k = 0; k < 8; ++k)
		{
			const float a = glm::radians(45.0f * static_cast<float>(k));
			const glm::vec2 dir(std::cos(a), std::sin(a));
			const glm::vec2 animalAt = centre - dir * (radius + 8.0f);
			const glm::vec2 pileAt = centre + dir * (radius + behind);
			if (!detail::InBounds(animalAt) || !detail::InBounds(pileAt) || detail::Collides(animalAt, 1u) ||
			    detail::Collides(pileAt, 1u))
			{
				continue;
			}
			auto& transform = registry.Get<Transform>(*target);
			const auto& island = Locator::terrainSystem::value();
			transform.position = glm::vec3(animalAt.x, island.GetHeightAt(animalAt), animalAt.y);
			auto& brain = registry.Get<AnimalBrain>(*target);
			brain.hunger = static_cast<int16_t>(Locator::infoConstants::value().animal.at(static_cast<size_t>(wanted)).hunger);
			brain.altitude = 0.0f;
			if (auto* flock = registry.Valid(registry.Get<const Animal>(*target).flock)
			                      ? registry.TryGet<components::Flock>(registry.Get<const Animal>(*target).flock)
			                      : nullptr;
			    flock != nullptr)
			{
				flock->domainCentre = transform.position;
				flock->savedDomainCentre = flock->domainCentre;
				flock->leaderTurns = 0;
			}
			registry.SetDirty();
			const auto pile = archetypes::PotArchetype::Create(glm::vec3(pileAt.x, island.GetHeightAt(pileAt), pileAt.y), 0.0f,
			                                                   PotInfo::FoodPile, 1000);
			if (pile != entt::null)
			{
				SetupPotReaction(pile);
			}
			tracked = *target;
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Animal test: animal {} at ({:.1f}, {:.1f}), obstacle {} at ({:.1f}, {:.1f}) r {:.1f}, "
			                   "food at ({:.1f}, {:.1f})",
			                   static_cast<uint32_t>(*target), animalAt.x, animalAt.y, static_cast<uint32_t>(*obstacle),
			                   centre.x, centre.y, radius, pileAt.x, pileAt.y);
			break;
		}
	}
	if (tracked != entt::null && registry.Valid(tracked) && turn > when && turn <= when + 600 && turn % 2 == 0)
	{
		const auto& t = registry.Get<const Transform>(tracked);
		const auto& brain = registry.Get<const AnimalBrain>(tracked);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Animal track {}: turn {} ({:.2f}, {:.2f}) state {} move {:#x} turnsToObj {} angle {}",
		                   static_cast<uint32_t>(tracked), turn, t.position.x, t.position.z, brain.topState, brain.moveState,
		                   brain.turnsToObj, brain.angle);
	}
}

/// OPENBLACK_TEST_SCRIPT_HELD / OPENBLACK_TEST_CANNOT_BE_EATEN (RunDebugHooks)
void ScriptHeldTest(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& held = AnimalDebugHooksData().scriptHeldTestHeld;
	static const debug_env::Variable k_TestScriptHeld("OPENBLACK_TEST_SCRIPT_HELD");
	if (const char* spec = k_TestScriptHeld.Get(); spec != nullptr)
	{
		int wanted = 0;
		unsigned when = 5;
		unsigned release = 0;
		std::sscanf(spec, "%d,%u,%u", &wanted, &when, &release);
		if (turn == when)
		{
			if (const auto e = NthAnimal(wanted); e)
			{
				// as a CREATE whose result a script variable keeps (added, then the VM's reference)
				held = *e;
				script_held::AddScriptThing(held, true);
				script_held::IncrementReference(held);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: animal {} (entity {}) held by a script, controlled {}",
				                   wanted, static_cast<uint32_t>(held), script_held::IsControlledByScript(held));
			}
		}
		if (release != 0 && turn == release && held != entt::null)
		{
			script_held::DecrementReference(held);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: script reference of entity {} dropped",
			                   static_cast<uint32_t>(held));
		}
		if (held != entt::null && turn % 25 == 0)
		{
			if (!registry.Valid(held) || !registry.AllOf<AnimalBrain>(held))
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: held entity {} gone by turn {}",
				                   static_cast<uint32_t>(held), turn);
				held = entt::null;
			}
			else
			{
				const auto& brain = registry.Get<const AnimalBrain>(held);
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Animal test: held entity {} turn {} state {} counter {} inScript {} controlled {}",
				                   static_cast<uint32_t>(held), turn, brain.topState, brain.counter,
				                   script_held::IsInScript(held), script_held::IsControlledByScript(held));
			}
		}
	}
	static const debug_env::Variable k_TestCannotBeEaten("OPENBLACK_TEST_CANNOT_BE_EATEN");
	if (const char* spec = k_TestCannotBeEaten.Get(); spec != nullptr && turn == static_cast<uint32_t>(std::atoi(spec)))
	{
		int count = 0;
		registry.Each<const Transform>([&count, &registry](entt::entity e, const Transform&) {
			if (registry.AnyOf<Animal, components::Villager>(e))
			{
				script_held::SetCannotBeEaten(e);
				++count;
			}
		});
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: {} animals and villagers cannot be eaten", count);
	}
}
} // namespace

/// Test hooks, once per turn (docs/bw1-notes/animals.md):
/// - OPENBLACK_TEST_VIEW_ANIMAL="n[,distance[,angle[,every]]]": the camera flies to the n-th animal from that many
///   metres (default 6), from that side (degrees around it, 0 = +z), slightly above; again every that many turns.
/// - OPENBLACK_TEST_THROW_ANIMAL="n,turn[,vx,vy,vz]": at that turn the n-th animal is thrown with that velocity.
/// - OPENBLACK_TEST_KILL_ANIMAL="n,turn": at that turn the n-th animal loses its life (DestroyedByEffect).
/// - OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>: n counts only the animals of that species (4 sheep, 8 cow...).
/// - OPENBLACK_TEST_HUNGRY=<AnimalInfo>: at turn 1 every animal of that species is hungry (0 lion, 1 tiger, 2 wolf).
/// - OPENBLACK_TEST_SPREAD_REACTIONS=<turn>: at that turn every predator spreads its flee reaction again (what the
///   original's disabled per-turn respreading of reactions would do).
/// - OPENBLACK_TEST_HUNT_VILLAGER="<AnimalInfo>,<turn>": that species' first animal is put next to the first villager,
///   hungry.
/// - OPENBLACK_TEST_CORPSE_TURNS=<n>: with KILL_ANIMAL, the corpse lasts that many turns instead of 600.
/// - OPENBLACK_TEST_SCRIPT_HELD="n,turn[,release]": at that turn the n-th animal is held by a script as if a CREATE had
///   made it (controlled by the script); at `release` the reference goes. Its state, counter and flags every 25 turns.
/// - OPENBLACK_TEST_CANNOT_BE_EATEN=<turn>: at that turn every animal and villager gets the vortex's cannot-be-eaten flag.
/// - OPENBLACK_TEST_FOOD_PILE="<AnimalInfo>,<turn>": a food pile next to that species, all of them hungry.
/// - OPENBLACK_TEST_FOOD_BEHIND="<AnimalInfo>,<turn>[,<kind>[,<m>]]": that species' first animal, hungry, is put 8 m in
///   front of the nearest building (kind 0, not a field: the circle iterator skips fields) or feature (2) of 2..8 m
///   radius, or tree (1), and a food pile m (6) behind it
///   (within the food reaction's 7 x 7 cells), to see the circle hug of GOTO_FOOD; its track (position, states,
///   TurnsToObj) is logged every 2 turns for 600 turns.
/// - OPENBLACK_TEST_SMOKE=<n>: the corpse smoke over the n-th animal every 30 turns.
/// - OPENBLACK_ANIMAL_TRACE=1: every state change, and every 50 turns how many animals are in each state.
void RunDebugHooks(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	static const debug_env::Variable k_TestViewAnimal("OPENBLACK_TEST_VIEW_ANIMAL");
	if (const char* view = k_TestViewAnimal.Get(); view != nullptr && Locator::camera::has_value())
	{
		int wanted = 0;
		float distance = 6.0f;
		float angle = 0.0f;
		unsigned every = 0;
		std::sscanf(view, "%d,%f,%f,%u", &wanted, &distance, &angle, &every);
		// OPENBLACK_TEST_VIEW_LOCK=1: the camera is put there every turn (no flight), for fast animals such as birds
		static const bool lock = std::getenv("OPENBLACK_TEST_VIEW_LOCK") != nullptr;
		const bool aim = lock || turn == 0 || (every != 0 && turn % every == 0);
		if (const auto e = NthAnimal(wanted); e && aim)
		{
			const auto& t = registry.Get<const Transform>(*e);
			const float radians = glm::radians(angle);
			const glm::vec3 focus = t.position + glm::vec3(0.0f, 0.8f, 0.0f);
			const glm::vec3 origin =
			    focus + glm::vec3(std::sin(radians) * distance, distance * 0.35f, std::cos(radians) * distance);
			if (lock)
			{
				Locator::camera::value().SetOrigin(origin).SetFocus(focus);
			}
			else
			{
				Locator::camera::value().GetModel().SetFlight(origin, focus);
			}
			if (!lock || turn % 50 == 0)
			{
				SPDLOG_LOGGER_INFO(
				    spdlog::get("game"), "Animal view: animal {} (entity {}) at ({:.1f}, {:.1f}, {:.1f}) state {}", wanted,
				    static_cast<uint32_t>(*e), t.position.x, t.position.y, t.position.z, static_cast<int>(TopState(*e)));
			}
		}
	}
	static const debug_env::Variable k_TestThrowAnimal("OPENBLACK_TEST_THROW_ANIMAL");
	if (const char* throwTest = k_TestThrowAnimal.Get(); throwTest != nullptr)
	{
		int wanted = 0;
		unsigned when = 0;
		glm::vec3 velocity(0.0f, 8.0f, 4.0f);
		std::sscanf(throwTest, "%d,%u,%f,%f,%f", &wanted, &when, &velocity.x, &velocity.y, &velocity.z);
		if (turn == when)
		{
			if (const auto e = NthAnimal(wanted); e)
			{
				auto& transform = registry.Get<Transform>(*e);
				transform.position.y += 1.0f;
				const bool thrown =
				    physics::PhysicsObjects::AddObject(*e, velocity, glm::vec3(2.0f, 0.0f, 1.0f), entt::null, true) != nullptr;
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: animal {} thrown {}", wanted, thrown);
			}
		}
	}
	static const debug_env::Variable k_TestKillAnimal("OPENBLACK_TEST_KILL_ANIMAL");
	if (const char* kill = k_TestKillAnimal.Get(); kill != nullptr)
	{
		int wanted = 0;
		unsigned when = 0;
		std::sscanf(kill, "%d,%u", &wanted, &when);
		if (turn == when)
		{
			if (const auto e = NthAnimal(wanted); e)
			{
				(registry.AllOf<Life>(*e) ? registry.Get<Life>(*e) : registry.Assign<Life>(*e)).value = 0.0f;
				DestroyedByEffect(*e);
				// OPENBLACK_TEST_CORPSE_TURNS=<n>: a shorter corpse than the original's 600 turns, to see the smoke
				static const debug_env::Variable k_TestCorpseTurns("OPENBLACK_TEST_CORPSE_TURNS");
				if (const char* quick = k_TestCorpseTurns.Get(); quick != nullptr)
				{
					if (auto* brain = registry.TryGet<AnimalBrain>(*e); brain != nullptr)
					{
						brain->counter = static_cast<int16_t>(std::atoi(quick));
					}
				}
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: animal {} (entity {}) killed", wanted,
				                   static_cast<uint32_t>(*e));
			}
		}
	}
	static const debug_env::Variable k_TestHungry("OPENBLACK_TEST_HUNGRY");
	if (const char* hungry = k_TestHungry.Get(); hungry != nullptr && turn == 1)
	{
		const int wanted = std::atoi(hungry);
		registry.Each<const Animal, AnimalBrain>([wanted](entt::entity, const Animal& animal, AnimalBrain& brain) {
			if (static_cast<int>(animal.type) == wanted)
			{
				brain.hunger =
				    static_cast<int16_t>(Locator::infoConstants::value().animal.at(static_cast<size_t>(wanted)).hunger);
			}
		});
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: species {} hungry", wanted);
	}
	static const debug_env::Variable k_TestSpreadReactions("OPENBLACK_TEST_SPREAD_REACTIONS");
	if (const char* spread = k_TestSpreadReactions.Get(); spread != nullptr && turn == static_cast<uint32_t>(std::atoi(spread)))
	{
		std::vector<entt::entity> animals;
		registry.Each<const Animal>([&animals](entt::entity e, const Animal&) { animals.push_back(e); });
		for (const auto e : animals)
		{
			SpreadPredatorReaction(e);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: predator reactions spread");
	}
	// OPENBLACK_TEST_HUNT_VILLAGER="<AnimalInfo>,<turn>": that species' first animal is moved next to the first villager
	// and made hungry (a predator's hunt of a villager is hard to catch in a normal game)
	static const debug_env::Variable k_TestHuntVillager("OPENBLACK_TEST_HUNT_VILLAGER");
	if (const char* hunt = k_TestHuntVillager.Get(); hunt != nullptr)
	{
		int wanted = 0;
		unsigned when = 2;
		std::sscanf(hunt, "%d,%u", &wanted, &when);
		if (turn == when)
		{
			std::optional<entt::entity> predator;
			registry.Each<const Animal, const Transform>([&](entt::entity e, const Animal& animal, const Transform&) {
				if (!predator && static_cast<int>(animal.type) == wanted)
				{
					predator = e;
				}
			});
			std::optional<glm::vec3> at;
			registry.Each<const components::Villager, const Transform>(
			    [&](entt::entity, const components::Villager&, const Transform& t) {
				    if (!at)
				    {
					    at = t.position;
				    }
			    });
			if (predator && at)
			{
				auto& transform = registry.Get<Transform>(*predator);
				transform.position = *at + glm::vec3(12.0f, 0.0f, 0.0f);
				if (Locator::terrainSystem::has_value())
				{
					transform.position.y =
					    Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
				}
				auto& brain = registry.Get<AnimalBrain>(*predator);
				brain.hunger =
				    static_cast<int16_t>(Locator::infoConstants::value().animal.at(static_cast<size_t>(wanted)).hunger);
				brain.altitude = 0.0f;
				// its flock's domain too, or KeepLeaderWithinDomain walks it back home instead of hunting
				if (auto* flock = registry.Valid(registry.Get<const Animal>(*predator).flock)
				                      ? registry.TryGet<components::Flock>(registry.Get<const Animal>(*predator).flock)
				                      : nullptr;
				    flock != nullptr)
				{
					flock->domainCentre = transform.position;
					flock->savedDomainCentre = flock->domainCentre;
					flock->leaderTurns = 0;
				}
				registry.SetDirty();
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: predator {} put next to a villager at ({:.1f}, {:.1f})",
				                   static_cast<uint32_t>(*predator), transform.position.x, transform.position.z);
			}
		}
	}
	// OPENBLACK_TEST_FOOD_PILE="<AnimalInfo>,<turn>": a food pile next to that species' first animal, which is made
	// hungry (the food reaction: the hungry grazers come and eat 50 of it)
	static const debug_env::Variable k_TestFoodPile("OPENBLACK_TEST_FOOD_PILE");
	if (const char* food = k_TestFoodPile.Get(); food != nullptr)
	{
		int wanted = 8;
		unsigned when = 3;
		std::sscanf(food, "%d,%u", &wanted, &when);
		if (turn == when)
		{
			std::optional<entt::entity> target;
			int index = 0;
			registry.Each<const Animal, const Transform>([&](entt::entity e, const Animal& animal, const Transform&) {
				if (!target && static_cast<int>(animal.type) == wanted && index++ == 0)
				{
					target = e;
				}
			});
			if (target)
			{
				// every animal of that species is hungry, so several come
				registry.Each<const Animal, AnimalBrain>([wanted](entt::entity, const Animal& animal, AnimalBrain& brain) {
					if (static_cast<int>(animal.type) == wanted)
					{
						brain.hunger =
						    static_cast<int16_t>(Locator::infoConstants::value().animal.at(static_cast<size_t>(wanted)).hunger);
					}
				});
				auto position = registry.Get<const Transform>(*target).position + glm::vec3(8.0f, 0.0f, 0.0f);
				if (Locator::terrainSystem::has_value())
				{
					position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
				}
				const auto pile = archetypes::PotArchetype::Create(position, 0.0f, PotInfo::FoodPile, 1000);
				if (pile != entt::null)
				{
					SetupPotReaction(pile);
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal test: food pile at ({:.1f}, {:.1f})", position.x,
					                   position.z);
				}
			}
		}
	}
	static const debug_env::Variable k_TestFoodBehind("OPENBLACK_TEST_FOOD_BEHIND");
	if (const char* behind = k_TestFoodBehind.Get(); behind != nullptr)
	{
		FoodBehindObstacle(behind, turn);
	}
	// OPENBLACK_TEST_SMOKE=<n>: the corpse puff (ecs/DisappearSmoke.h) over the n-th animal every 30 turns
	static const debug_env::Variable k_TestSmoke("OPENBLACK_TEST_SMOKE");
	if (const char* smoke = k_TestSmoke.Get(); smoke != nullptr && turn % 30 == 5)
	{
		if (const auto e = NthAnimal(std::atoi(smoke)); e)
		{
			const auto& transform = registry.Get<const Transform>(*e);
			DisappearSmoke::Create(transform.position + glm::vec3(0.0f, transform.scale.y, 0.0f), 1.0f);
		}
	}
	// OPENBLACK_TEST_LAIRS=<turn>: every predator flock leader recomputes its lair (CalculateLairPosition) and logs it
	static const debug_env::Variable k_TestLairs("OPENBLACK_TEST_LAIRS");
	if (const char* lairs = k_TestLairs.Get(); lairs != nullptr && turn == static_cast<uint32_t>(std::atoi(lairs)))
	{
		detail::TestLairs();
	}
	ScriptHeldTest(turn);
	if (debug_env::AnimalTrace() && turn % 50 == 0)
	{
		std::map<int, int> states;
		registry.Each<const AnimalBrain>([&states](entt::entity, const AnimalBrain& brain) { ++states[brain.topState]; });
		std::string line;
		for (const auto& [state, count] : states)
		{
			line += fmt::format(" {}:{}", state, count);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animals turn {}: state:count{}", turn, line);
	}
}

} // namespace openblack::ecs::animal_ai
