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

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"
#include "Enums.h"

/// The actions with which a creature helps a town or plays with its people: it brings food or wood to a store, puts food
/// by a worship site, drops a cow in a store, makes a villager a breeder, takes a villager home, brings a villager to a
/// worship site, raises or lowers a town's totem, and frightens, plays with or kisses a villager. The agendas and the
/// rules here are pure, free of the game.
namespace openblack::creature_town_help
{
using creature_mind::Random;
using creature_mind::Step;

/// The thing that a handful is taken from (a field, a tree or a pile), where it is, how near the creature goes to it,
/// and what the handful looks like
struct HandfulSource
{
	uint32_t thing {0};
	glm::vec2 point {0.0f};
	float within {0.0f};
	creature_mind::ObjectOrder::Handful look {creature_mind::ObjectOrder::Handful::Wheat};
};

/// The creature goes to a field until it is within one and a half times its height of the field
constexpr float k_FieldReachHeights = 1.5f;
/// It goes to a tree or a pile until within a fifth more than both their radii
constexpr float k_ThingReach = 1.2f;
/// It turns to face a thing for a tenth of a second
constexpr float k_FaceSeconds = 0.1f;

/// How near the creature goes to a thing on the ground to take a handful from it: a field by its height, anything else
/// by the radii of the two
[[nodiscard]] float FieldReach(float height);
[[nodiscard]] float ThingReach(float radius, float thingRadius);

/// The kind of pot that a handful of a look is
[[nodiscard]] PotInfo HandfulPot(creature_mind::ObjectOrder::Handful look);

/// The look of a handful of magic wood, by a draw of three: an evergreen branch, a hardwood branch or logs
[[nodiscard]] creature_mind::ObjectOrder::Handful MagicWoodLook(const Random& random);

/// Giving a handful to a store. Unless its hand holds what it gives, it walks to the source, turns to face it a tenth of
/// a second and takes a handful from it. Then it goes to where it can throw into the store, and turns to face the store
/// a tenth of a second (where the action counts as done), both with a look of compassion; then it throws the handful in.
[[nodiscard]] std::vector<Step> GiveHandfulToStore(const std::optional<HandfulSource>& take, uint32_t store, float height);

/// Putting food from a field by a worship site. Unless its hand holds food, it walks to within its height of the field,
/// turns to face the field a tenth of a second, makes the plan act on itself, and takes a handful of wheat. Then it walks
/// to within its height of the place where the site keeps its food, and puts down what it holds there (where the action
/// counts as done).
[[nodiscard]] std::vector<Step> PutFieldFoodBySite(const std::optional<HandfulSource>& take, glm::vec2 siteFood, float height);

/// Dropping a cow in a store: unless it holds a cow, it picks the cow up. It turns to face the store a tenth of a second,
/// goes to where it can throw into the store with a look of compassion (where the action counts as done), and throws the
/// cow in.
[[nodiscard]] std::vector<Step> DropCowInStore(std::optional<uint32_t> pickUp, uint32_t store);

/// Making a villager a breeder: it picks the villager up, strokes it with a look of compassion (where the action counts
/// as done), makes it a breeder and puts it down
[[nodiscard]] std::vector<Step> MakeBreeder(uint32_t villager);

/// Taking a villager home to sleep: unless it holds a villager, it picks the villager up. It goes near the villager's
/// home, to two metres from it, turns to face the home a tenth of a second and puts the villager down.
[[nodiscard]] std::vector<Step> TakeVillagerHome(std::optional<uint32_t> pickUp, uint32_t home);
constexpr float k_NearHome = 2.0f;

/// Bringing a villager to a worship site: unless it holds a villager, it picks the villager up. It turns to face the site
/// a tenth of a second, goes right up to it, and puts the villager down with a look of compassion.
[[nodiscard]] std::vector<Step> BringVillagerToSite(std::optional<uint32_t> pickUp, uint32_t site);

/// Raising or lowering a totem: it goes near the totem, to twelve metres from it, turns to face it a tenth of a second,
/// plays the summoning clip with a playful look, then moves the totem up or down turn by turn (where the action counts
/// as done)
[[nodiscard]] std::vector<Step> MoveTotem(uint32_t totem, bool raise);
constexpr float k_NearTotem = 12.0f;
/// Each turn a creature moves a totem by this share of the town's people
constexpr float k_TotemStep = 0.02f;
/// One turn of moving a totem: the share it sets, and whether the totem is now all the way up or down
struct TotemTurn
{
	float share {0.0f};
	bool done {false};
};
/// The turn of a totem at a share. Raising, it goes up by a step; when less than a step is left to the top, it goes to
/// the top and is done. Lowering is the same, down to none.
[[nodiscard]] TotemTurn MoveTotemTurn(float share, bool raise);

/// Frightening villagers in play: it goes near the villager, to three times its height from it, turns to face it for two
/// seconds and plays the frightened clip, eyes wide
[[nodiscard]] std::vector<Step> FrightenVillagers(uint32_t villager, float height);
constexpr float k_FrightenHeights = 3.0f;
constexpr float k_FrightenFaceSeconds = 2.0f;

/// Playing with a villager: it goes near the villager, to two times its height from it, and turns to face it a tenth of
/// a second. Two times in three it plays the playful clip. It pulls a face picked from the first ten, points at the
/// villager for five seconds with a smile (where the action counts as done), and pulls another face. The head looks at
/// the villager from the turn to the end.
[[nodiscard]] std::vector<Step> PlayWithVillager(uint32_t villager, float height, const Random& random);
constexpr float k_PlayHeights = 2.0f;
constexpr float k_PlayPointSeconds = 5.0f;

/// Kissing a villager in play: it goes near the villager, to two times its height from it, turns to face it a tenth of a
/// second and kisses it
[[nodiscard]] std::vector<Step> KissVillager(uint32_t villager, float height);

/// Whether an animal of a kind is one that a creature drops in a store as a cow: the farm animals and the tortoise, and
/// their puzzle pieces
[[nodiscard]] bool IsCowKind(AnimalInfo type);

/// A villager is far from home when its home is more than 40 m from it
constexpr float k_FarFromHome = 40.0f;
[[nodiscard]] bool FarFromHome(float distanceFromHome);

/// What tells whether a creature may make a villager a breeder
struct BreederFacts
{
	/// The villager is about, grown up, alive, and not under a script's control
	bool available {false};
	/// It belongs to a town, is no disciple and follows no disciple
	bool hasTown {false};
	bool disciple {false};
	bool follower {false};
	/// The breeders that its town has now
	uint32_t townBreeders {0};
};
/// A town has fewer breeders than this for a creature to make another
constexpr uint32_t k_MostBreeders = 3;
[[nodiscard]] bool CanMakeBreeder(const BreederFacts& facts);

} // namespace openblack::creature_town_help
