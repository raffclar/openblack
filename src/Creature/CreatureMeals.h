/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <map>
#include <optional>

/// The rules for the things that a creature eats from (a field, the storage pit of a town, a tree), for the handful that
/// it takes from them, for the decrease of its hunger after a meal, and for the counts of its meals.
namespace openblack::creature_meals
{
/// A creature eats from a field that is sown (its crop has an age) and that has food in its crop. The crop can be ripe
/// or not ripe.
[[nodiscard]] bool FieldHasFood(float cropAge, float cropFood);

/// A creature eats from a storage pit that is not wider (its radius on the ground) than this number of times its size. It
/// eats from a tree that is as tall as this number of times its size, or taller.
constexpr float k_ThingsPerSize = 15.0f;
/// A creature eats from a storage pit that is not too wide for it and that has food in its food pile. The amount in the
/// pile counts, not the full store of the pit.
[[nodiscard]] bool StoragePitServes(float pitRadius, float size, uint32_t pileAmount);
/// A creature eats from a tree that is tall enough for it
[[nodiscard]] bool TreeBigEnough(float treeHeight, float size);

/// A handful holds 1000 of food for each unit of the size of the creature, as a whole number. It holds no more than the
/// store of food of the thing. A field and a tree have no store of food, thus a handful from them holds no food.
constexpr float k_HandfulPerSize = 1000.0f;
[[nodiscard]] uint32_t HandfulAmount(float size, uint32_t thingHolds);

/// The hunger after a meal that the creature did not bring up: the hunger less the energy of the meal (its food, divided
/// by the size of the creature and the food for each unit of energy of its species). The hunger stays from 0 to 1.
[[nodiscard]] float HungerAfterMeal(float hunger, float energy);

/// The counts of what a creature ate
struct Tallies
{
	/// The people, animals and creatures that it killed when it ate them, or when it stomped on them
	uint32_t peopleKilled {0};
	uint32_t animalsKilled {0};
	uint32_t creaturesKilled {0};
	/// The meals that it did not bring up, by the sort of object (the object type of its info)
	std::map<uint32_t, uint32_t> keptDown;
};
/// The object types of the living sorts that a creature counts as killed when it eats one
constexpr uint32_t k_CreatureSort = 1;
constexpr uint32_t k_VillagerSort = 2;
constexpr uint32_t k_AnimalSort = 4;
/// Count a meal. A living thing that the creature ate counts as killed. A meal that it did not bring up counts by its
/// sort.
void CountMeal(Tallies& tallies, std::optional<uint32_t> sort, bool keptDown);
/// Count a kill of a thing of a sort: a person, an animal or a creature. Other sorts do not count.
void CountKill(Tallies& tallies, uint32_t sort);
} // namespace openblack::creature_meals
