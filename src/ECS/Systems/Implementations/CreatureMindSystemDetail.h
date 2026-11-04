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

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureAnimationLog.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureDrink.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/LeashRules.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}
namespace openblack::ecs::components
{
struct Creature;
struct CreatureMindState;
struct Transform;
} // namespace openblack::ecs::components
namespace openblack::creature_mind_tables
{
struct Tables;
}
namespace openblack
{
struct GWorshipSiteInfo;
}

/// What the files of the creature mind system share: how the desires of a species start, and how to find the food and
/// the water near a creature
namespace openblack::ecs::systems::mind_detail
{
[[nodiscard]] std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> SetupFor(CreatureType species);
/// How far the game spreads a new source of a kind at random either way: its value, then its threshold
[[nodiscard]] creature_desires::DesireSetup::Spread SourceSpreadOf(uint32_t type);
/// The personality a species is born with, from the game's creature table, before it is spread for a creature
[[nodiscard]] creature_desire_sources::InnateTraits InnateTraitsOf(CreatureType species);
[[nodiscard]] std::optional<float> FoodValueOf(entt::entity entity);
/// Where a creature drinks from the sea: by the nearest water point that the land marks, else at the nearest coast of
/// its map of the land. None when it finds no place where it can stand to drink.
[[nodiscard]] std::optional<creature_drink::Place> DrinkPlaceOf(entt::entity creature);
/// The sort of a thing, by the object type of its info (an animal's own info, or the info of any other thing); none for a
/// thing without info. A creature remembers the sorts of things that it was given and that it cannot eat.
[[nodiscard]] std::optional<uint32_t> ObjectSortOf(const ecs::Registry& registry, entt::entity thing);
/// The kind of belief a creature keeps about a thing, by the game's belief types; none for the things it does not yet
/// know the kind of here
[[nodiscard]] std::optional<uint32_t> BeliefKindOf(const ecs::Registry& registry, entt::entity thing);
/// The kind of a thing, by how the desires examine it. A scaffold is a loose object.
[[nodiscard]] creature_activity::Thing KindOfThing(const ecs::Registry& registry, entt::entity thing);
/// How much a thing catches the eye of a creature, by what it is. The temple, fields, features and the other
/// things that stand on the land are fixed things. A scaffold is a loose object.
[[nodiscard]] creature_look::Interest LookInterestOf(const ecs::Registry& registry, entt::entity thing);
/// The data of a worship site's altar: the data of the worship site of its site's tribe. None for another thing.
[[nodiscard]] const GWorshipSiteInfo* AltarInfoOf(const ecs::Registry& registry, entt::entity altar);
/// What a creature knows of a thing. When the creature has a belief about the thing, these are the attributes that it
/// last took in, and the place where it last came across the thing. Else they are the attributes of the thing and its
/// place now.
[[nodiscard]] std::optional<creature_tree::Belief> BeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                            entt::entity self);
/// What a creature finds of a thing now, as far as the world here tells it
[[nodiscard]] std::optional<creature_tree::Belief> BeliefNowOf(const ecs::Registry& registry, entt::entity entity,
                                                               entt::entity self);
/// The same, for a valid thing of which the caller knows the kind of belief (BeliefKindOf) and the map place
/// (thing_map_place::Of). The answer is the same, but the function does not find the kind and the place again.
[[nodiscard]] std::optional<creature_tree::Belief> BeliefNowOf(const ecs::Registry& registry, entt::entity entity,
                                                               entt::entity self, uint32_t kind,
                                                               const map_coords::MapCoords& place);
/// What a creature knows of a thing that it acts on. The temple here is one thing, which stands for its heart when a
/// creature acts on it.
[[nodiscard]] std::optional<creature_tree::Belief> ActedOnBeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                                   entt::entity self);
/// What a plan's desire was about, for the lessons feedback teaches: what the plan says it was about, else for a desire
/// about something in itself the thing it acted on, else nothing
[[nodiscard]] std::optional<uint32_t> LessonActivity(const creature_mind_tables::Tables& tables,
                                                     creature_desires::Desire desire, std::optional<uint32_t> object,
                                                     std::optional<uint32_t> about);
/// The value of nearness for a thing at a known place, from the distance weight of a desire. If the leash of the
/// player is tied to a thing, the distance starts at that thing. If not, the distance starts at the creature. A thing
/// that the creature holds has the highest value.
[[nodiscard]] float Nearness(const ecs::Registry& registry, entt::entity creature, const glm::vec3& at, entt::entity object,
                             const map_coords::MapCoords& place, float distanceWeight);
/// The town nearest a point along the ground, of any player or none; none when there is no town
[[nodiscard]] entt::entity NearestTown(const ecs::Registry& registry, glm::vec3 point);
/// Whether a creature lies dead: knocked out, or with a plan that has the action of dying or of lying dead for good
[[nodiscard]] bool IsDead(const ecs::Registry& registry, entt::entity creature, const creature_mind_tables::Tables& tables);
/// Whether a script made two creatures friends
[[nodiscard]] bool AreFriends(const ecs::Registry& registry, entt::entity first, entt::entity second);
/// Whether a creature can be angry with a different creature. The other creature must not be dead or a friend. Its
/// species must let creatures attack it.
[[nodiscard]] bool MayBeAngryWith(const ecs::Registry& registry, entt::entity other, entt::entity creature,
                                  const creature_mind_tables::Tables& tables);
/// How often a creature has seen a miracle cast, and how often it must see it to learn it; none for a miracle it can't
/// learn by watching
[[nodiscard]] std::optional<std::pair<float, float>> MiracleSightings(const components::CreatureMindState& mind,
                                                                      const creature_mind_tables::Tables& tables,
                                                                      CreatureType species, MagicType magic);
/// Having done an action, the desire it satisfies is less, by the game's action table: multiplied by the action's
/// multiplier, no lower than the species' floor for desires
void Lessen(entt::entity creature, creature_desires::Desires& desires, std::string_view action);
/// The least a creature's desires are lessened to by its actions, or wanted while another is made dominant, by its
/// species
[[nodiscard]] float DesireFloorOf(entt::entity creature);
/// Having done an action, its body pays for it
void BodyPaysFor(entt::entity creature, std::string_view action);
/// Both, as a step that takes effect at once does
void Satisfied(entt::entity creature, creature_desires::Desires& desires, std::string_view action);
/// The town nearest a point along the ground, of any player or none; none when there are no towns
[[nodiscard]] entt::entity NearestTown(const ecs::Registry& registry, glm::vec3 point);
/// The middle of the nearest area so wide about a point that a creature may go to: land it can walk on, with nothing
/// standing in its way there (nor anything living, when asked), inside any area it is kept within. None without one.
[[nodiscard]] std::optional<glm::vec2> ClearAreaFor(const ecs::Registry& registry, entt::entity creature, glm::vec2 point,
                                                    float width, bool livingBlocks);
/// Whether a cell of the land is clear for a creature: land it can walk on, with nothing standing in its way there (nor
/// anything living, when asked)
[[nodiscard]] bool CellClearFor(const ecs::Registry& registry, entt::entity creature, int32_t x, int32_t z, bool livingBlocks);
/// The way a creature looks, as a game angle: towards what its head is turned to, or else the way its body faces
[[nodiscard]] uint16_t LookingAngle(const components::Transform& transform, const std::optional<glm::vec3>& lookAt);
/// Whether the body of the creature can pay for a miracle of a type
[[nodiscard]] bool CanPayForMiracle(entt::entity creature, MagicType type);
/// Where a creature's head is as posed this frame: its head bone, else its height over its feet
[[nodiscard]] glm::vec3 HeadPoint(const ecs::Registry& registry, entt::entity creature);

// What the files of the mind system share about the plans of a creature, the things it acts on and what it learns
/// The advisor may tell the player of what the creature learnt
void TellHelp(entt::entity creature, const creature_help::Message& message);
/// The source of the desire to copy the player, and how much a stroke while copying adds to it
inline constexpr uint32_t k_FollowPlayerSource = 54;
/// The actions that cast a power-up of a miracle, which only a creature grown up enough tries
[[nodiscard]] bool IsPowerUpCast(std::string_view action);
[[nodiscard]] std::optional<glm::vec2> PointOf(const ecs::Registry& registry, entt::entity entity);
[[nodiscard]] uint32_t PlayerNumberOf(PlayerNames owner);
/// Which toy the thing is: the football is a ball, and of the statics those with a toy's model
[[nodiscard]] std::optional<creature_object_actions::Toy> ToyOf(const ecs::Registry& registry, entt::entity entity);
/// The one-shot miracle that a globe holds, as a creature sees it; none for a thing that is not a globe
[[nodiscard]] std::optional<creature_plan_actions::OneShot> OneShotOf(const ecs::Registry& registry, entt::entity entity);
/// Put the food pile of a storage pit, and the point of the pile, in the situation of an action on the pit
void SituateStoreFood(const ecs::Registry& registry, entt::entity pit, creature_plan_actions::Situation& situation);
/// The game's turn now
[[nodiscard]] uint32_t TurnNow();
[[nodiscard]] bool Accepts(const ecs::Registry& registry, creature_plan_actions::Target target, entt::entity entity,
                           entt::entity self);
[[nodiscard]] std::string ObjectName(const std::optional<creature_tree::Belief>& belief);
/// Where a creature at a point would fish: the shoal of the nearest fish farm (the farm itself without one), going as
/// near as it is tall but at least 15 m, and where it would stand to fish for itself; none without a farm near
[[nodiscard]] std::optional<creature_plan_actions::Situation::Fishing> FishingFor(const ecs::Registry& registry,
                                                                                  entt::entity creature, glm::vec2 position);
/// The number of the player a thing is of, none for a thing of no player. The things of a town are of its player; a
/// miracle is of the player of its caster; the land's other things are of the neutral player or of none.
[[nodiscard]] std::optional<uint32_t> BeliefPlayerOf(const ecs::Registry& registry, entt::entity thing);
/// Whether a creature kept within an area by its leash has a place farther from the area's centre than the area's radius
[[nodiscard]] bool OutsideConfinedArea(const ecs::Registry& registry, entt::entity creature,
                                       const map_coords::MapCoords& place);
/// The area that holds the creature as a new agenda for an action starts, and the place of the creature then. None for a
/// creature without a leash state or a place.
[[nodiscard]] std::optional<std::pair<creature_leash::AgendaArea, glm::vec2>>
AgendaAreaOf(const ecs::Registry& registry, entt::entity creature, std::string_view action, bool walksBack = false);
/// Whether the creature does not want an action now: the area that holds it refused a walk of the action's agenda,
/// and the area holds it still
[[nodiscard]] bool LeftAreaFor(const ecs::Registry& registry, entt::entity creature, const components::CreatureMindState& mind,
                               uint32_t action);
/// Where the creature believes that a thing is: where it last came across the thing, else where the thing is now
/// The belief is the belief of the creature about the thing, or null when it has none.
[[nodiscard]] std::optional<map_coords::MapCoords>
BelievedPlaceOf(const ecs::Registry& registry, const creature_object_beliefs::Belief* belief, entt::entity thing);
[[nodiscard]] std::optional<map_coords::MapCoords>
BelievedPlaceOf(const ecs::Registry& registry, const components::CreatureMindState& mind, entt::entity thing);
/// The thing that a creature acts on for a belief: the temple for the belief about the temple's heart, else the thing of
/// the belief. None for a temple that has a heart: the creature acts on it only through the belief about its heart.
[[nodiscard]] std::optional<entt::entity> ActedThingOf(const ecs::Registry& registry, entt::entity thing);
/// The thing that an agenda takes for the thing of a plan. A creature that looks at a temple looks at the temple's heart,
/// the thing that it believes in; every other agenda takes the thing of the plan.
[[nodiscard]] std::optional<uint32_t>
AgendaThingOf(const ecs::Registry& registry, const creature_plan_actions::Executor& executor, std::optional<uint32_t> thing);
/// The thing a creature at a point uses for an action, of a kind, other than the goal. It looks only through its
/// beliefs about things that are not whole things, the newest first: what it has not come across, it does not use.
[[nodiscard]] std::optional<creature_planner::UsedChoice> UsedFor(entt::entity creature, creature_plan_actions::Target use,
                                                                  glm::vec2 position, float distanceWeight,
                                                                  std::optional<uint32_t> goal);
/// Draws the lengths of a look, of a smile and of a sit from the random numbers of the game, as the agenda is made. A
/// sit that has no place to sit draws nothing.
void DrawAgendaLengths(const creature_plan_actions::Executor& executor, creature_plan_actions::Situation& situation,
                       const std::function<float(float)>& upTo);
/// Whether a villager does something that the creature finds interesting: its state teaches a skill that the creature
/// does not know. Nothing else is interesting that way.
[[nodiscard]] bool DoingSomethingInteresting(const ecs::Registry& registry, const components::CreatureMindState& mind,
                                             entt::entity object);
/// What a creature knows about a whole thing that it examines a desire about. Whether its player looks at it is the
/// same for each whole thing, thus the caller finds it one time.
[[nodiscard]] creature_activity::WholeFacts WholeFactsOf(const ecs::Registry& registry, entt::entity creature,
                                                         entt::entity whole, creature_activity::Whole kind,
                                                         const creature_mind_tables::Tables& tables, bool lookedAt);
/// The town that a building belongs to, none when it has none
[[nodiscard]] std::optional<entt::entity> TownOfBuilding(const ecs::Registry& registry, uint32_t townId);
/// The town of a village totem: the town of the village centre that it stands on
[[nodiscard]] std::optional<entt::entity> TownOfTotem(const ecs::Registry& registry, entt::entity totem);
/// Whether the creature found no way to a thing lately. The creature waits longer for a thing with the fixed mark.
[[nodiscard]] bool NoWayLately(const ecs::Registry& registry, const components::CreatureMindState& mind, entt::entity thing);
/// The belief is the belief of the creature about the thing, or null when it has none.
[[nodiscard]] bool NoWayLately(const ecs::Registry& registry, const components::CreatureMindState& mind,
                               const creature_object_beliefs::Belief* belief, entt::entity thing);
[[nodiscard]] bool LetsCreatureAct(const ecs::Registry& registry, entt::entity thing);
/// The town of a thing: the town of a villager, a building, a field or a fish farm, of a pile in a storage pit, or of
/// a village totem. None for any other thing. It finds what the thing belongs to only for a villager, a building or a
/// field, because for a tree that search goes through the forests of the land.
[[nodiscard]] std::optional<entt::entity> TownOfThing(const ecs::Registry& registry, entt::entity thing);
} // namespace openblack::ecs::systems::mind_detail
