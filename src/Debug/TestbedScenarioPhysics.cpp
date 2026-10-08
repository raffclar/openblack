/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's physics scenarios: things lying still, one thing thrown, a crowd of things thrown and dropped together,
// buildings broken by thrown rocks, rocks split by hard landings and a creature struck. Each throw is the player's
// hand's, so the same rules run as in a game; run with --frame-stats to see what a frame costs while they fly and once
// they have come to rest.

#include <cmath>

#include <array>
#include <numbers>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the physics happens: south of the middle, in front of the camera and clear of the lake
constexpr glm::vec2 k_Field {0.0f, -40.0f};
/// A throw towards the field's middle from this far away lands near it
constexpr float k_ThrowFrom = 30.0f;
/// The hand's speed for a throw across the field, up and along
constexpr float k_ThrowUp = 9.0f;
constexpr float k_ThrowAlong = 16.0f;

/// The things thrown in the crowd, in turn: the kinds whose bodies the physics builds differently
const std::array<ObjectSetup, 10> k_Kinds {
    ObjectSetup {.type = MobileStaticInfo::RockChalk},
    ObjectSetup {.type = MobileStaticInfo::Boulder1Lime},
    ObjectSetup {.type = MobileObjectInfo::EgyptPotA},
    ObjectSetup {.type = PotInfo::FoodPile, .amount = 200},
    ObjectSetup {.type = PotInfo::WoodPile_1, .amount = 200},
    ObjectSetup {.type = MobileObjectInfo::EgyptBarrel},
    ObjectSetup {.type = VillagerInfo::CelticFarmerMale},
    ObjectSetup {.type = TreeInfo::Birch, .scale = 0.6f},
    ObjectSetup {.type = MobileObjectInfo::Ball},
    ObjectSetup {.type = MobileStaticInfo::SharprockSandstone},
};

/// A throw from a point on a ring round the field's middle, towards it
ThrowSetup Inward(size_t object, float angle, float delay, std::optional<float> repeat)
{
	const glm::vec2 out {std::cos(angle), std::sin(angle)};
	return {
	    .object = object,
	    .from = k_Field + (out * k_ThrowFrom),
	    .height = 4.0f,
	    .velocity = {-out.x * k_ThrowAlong, k_ThrowUp, -out.y * k_ThrowAlong},
	    .delaySeconds = delay,
	    .repeatSeconds = repeat,
	};
}

void AddIdleField(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	for (size_t i = 0; i < 40; ++i)
	{
		auto object = k_Kinds.at(i % k_Kinds.size());
		object.offset =
		    k_Field + glm::vec2 {(static_cast<float>(i % 8) - 3.5f) * 9.0f, (static_cast<float>(i / 8) - 2.0f) * 9.0f};
		objects.push_back(object);
	}
	all.push_back({
	    .id = "physics.idle",
	    .name = "Things lying still",
	    .facet = Facet::Physics,
	    .description = "Forty rocks, boulders, pots, piles, barrels, villagers, trees and balls lie on the land in a grid, "
	                   "and nothing throws them.",
	    .expected = "Nothing is in the physics: a frame costs what the land, the things and the villagers cost, and the "
	                "physics adds nothing to it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = objects,
	});
}

void AddOneThrow(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "physics.throw_one",
	    .name = "One rock thrown",
	    .facet = Facet::Physics,
	    .description = "A rock is thrown across the field every eight seconds, from the west towards the middle.",
	    .expected = "It flies in an arc, lands with a thump and dust, rolls and comes to rest, after which it costs nothing "
	                "until it is thrown again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = k_Field}},
	    .throws = {Inward(0, std::numbers::pi_v<float>, 1.0f, 8.0f)},
	});
}

void AddCrowdThrow(std::vector<Scenario>& all)
{
	constexpr size_t k_Count = 60;
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	for (size_t i = 0; i < k_Count; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(k_Count);
		auto object = k_Kinds.at(i % k_Kinds.size());
		object.offset = k_Field + glm::vec2 {std::cos(angle), std::sin(angle)} * k_ThrowFrom;
		objects.push_back(object);
		throws.push_back(Inward(i, angle, 1.0f, 12.0f));
	}
	all.push_back({
	    .id = "physics.throw_many",
	    .name = "Sixty things thrown together",
	    .facet = Facet::Physics,
	    .description = "Sixty rocks, boulders, pots, piles, barrels, villagers, trees and balls on a ring are all thrown "
	                   "at once towards its middle, and again every twelve seconds.",
	    .expected = "They fly in, strike one another and pile up in the middle, then come to rest one by one; once they "
	                "have, the physics costs nothing until the next throw.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = objects,
	    .throws = throws,
	});
}

void AddBuildingBreak(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	constexpr std::array<float, 3> k_Huts {-20.0f, 0.0f, 20.0f};
	for (const float x : k_Huts)
	{
		objects.push_back({.type = AbodeInfo::CelticHut, .offset = k_Field + glm::vec2 {x, 10.0f}});
	}
	for (size_t i = 0; i < k_Huts.size() * 2; ++i)
	{
		const float x = k_Huts.at(i % k_Huts.size()) + (i < k_Huts.size() ? -2.0f : 2.0f);
		objects.push_back({.type = MobileStaticInfo::Boulder2Chalk, .offset = k_Field + glm::vec2 {x, -20.0f}});
		throws.push_back({
		    .object = objects.size() - 1,
		    .from = k_Field + glm::vec2 {x, -20.0f},
		    .height = 4.0f,
		    .velocity = {0.0f, 4.0f, 20.0f},
		    .delaySeconds = 1.0f + static_cast<float>(i) * 0.5f,
		    .repeatSeconds = 6.0f,
		});
	}
	all.push_back({
	    .id = "physics.building_breaks",
	    .name = "Huts broken by thrown boulders",
	    .facet = Facet::Physics,
	    .description = "Three huts in a row, and two boulders thrown at each from the south, again and again.",
	    .expected = "A boulder knocks holes in a hut, pieces of it fly off and come to rest, and they fade away in time.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = objects,
	    .throws = throws,
	});
}

void AddRockSplit(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	for (size_t i = 0; i < 6; ++i)
	{
		const glm::vec2 offset = k_Field + glm::vec2 {(static_cast<float>(i) - 2.5f) * 12.0f, 0.0f};
		objects.push_back({.type = MobileStaticInfo::RockVolcanic, .offset = offset, .scale = 2.0f});
		throws.push_back({
		    .object = i,
		    .from = offset,
		    .height = 30.0f,
		    .velocity = {0.0f, -60.0f, 0.0f},
		    .delaySeconds = 1.0f + static_cast<float>(i) * 0.3f,
		    .repeatSeconds = 5.0f,
		});
	}
	all.push_back({
	    .id = "physics.rock_split",
	    .name = "Rocks hurled at the ground",
	    .facet = Facet::Physics,
	    .description = "Six big rocks hurled down at the ground from high up, again and again.",
	    .expected = "Each hard landing wears a rock down, and a worn rock splits into halves that fly apart and come to "
	                "rest.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = objects,
	    .throws = throws,
	});
}

void AddCreatureHit(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	for (size_t i = 0; i < 8; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / 8.0f;
		objects.push_back({.type = MobileStaticInfo::RockChalk,
		                   .offset = k_Field + glm::vec2 {std::cos(angle), std::sin(angle)} * k_ThrowFrom});
		throws.push_back(Inward(i, angle, 2.0f + static_cast<float>(i), 10.0f));
	}
	all.push_back({
	    .id = "physics.creature_hit",
	    .name = "A creature struck by thrown rocks",
	    .facet = Facet::Physics,
	    .description = "A cow stands in the middle of a ring of rocks, which are thrown at it one after another.",
	    .expected = "A rock that strikes it hurts it and it sways from the blow; the rocks bounce off and come to rest.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .creatures = {{.species = CreatureType::Cow, .offset = k_Field}},
	    .objects = objects,
	    .throws = throws,
	});
}

} // namespace

void testbed_scenarios::AddPhysicsScenarios(std::vector<Scenario>& all)
{
	AddIdleField(all);
	AddOneThrow(all);
	AddCrowdThrow(all);
	AddBuildingBreak(all);
	AddRockSplit(all);
	AddCreatureHit(all);
}
