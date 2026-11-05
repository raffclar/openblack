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
/// What an action does to a desire when it is complete, by the game's action table; nothing for an action that is not in
/// the table or lessens no desire
[[nodiscard]] std::optional<creature_desires::ActionLessening> LesseningOf(std::string_view action);
/// Having done an action, the desire it satisfies is less, by the game's action table: multiplied by the action's
/// multiplier, no lower than the species' floor for desires
void Lessen(entt::entity creature, creature_desires::Desires& desires, std::string_view action);
/// The least a creature's desires are lessened to by its actions, or wanted while another is made dominant, by its
/// species
[[nodiscard]] float DesireFloorOf(entt::entity creature);
/// The traits, counts and body values behind the sources that a script or a spell set take the new values at once
void TakeWrittenSources(entt::entity entity, components::CreatureMindState& mind);
/// Having done an action, its body pays for it
void BodyPaysFor(entt::entity creature, std::string_view action);
/// Both, as a step of an agenda that no plan gave does when it takes effect
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

// What the files of the mind system share about the body, the desires and the senses of a creature
/// Fainting from what its body lacks, the advisor says why first
void TellFainting(entt::entity creature);
/// The desires of the body, from the game's table of desires
[[nodiscard]] std::array<bool, creature_desires::k_DesireCount> BodyDesires();
/// The desires each stage of growing up brings and takes away, from the game's tables
[[nodiscard]] std::vector<creature_desires::PhaseDesires> Phases();
/// One turn of looking about: the head turns to what the creature watches, or ahead
[[nodiscard]] glm::vec3 LookAbout(const components::Creature& creature, const components::Transform& transform,
                                  const components::CreatureMindState& mind);
/// The look of a creature that walks to a place in this turn, or none near the place
[[nodiscard]] std::optional<glm::vec3> LookWhileGoing(entt::entity entity, const components::Creature& creature,
                                                      const components::Transform& transform,
                                                      components::CreatureMindState& mind, const glm::vec3& place);
/// Running away, the seconds that a creature of a species looks back at what it runs from, and the seconds that it then
/// looks about
[[nodiscard]] float RunLookBackSeconds(CreatureType species);
[[nodiscard]] float RunLookAheadSeconds(CreatureType species);
[[nodiscard]] std::optional<glm::vec3> GazePoint(const ecs::Registry& registry, entt::entity creature,
                                                 const creature_mind::Gaze& gaze, bool moving);
/// The point that the head of a creature in a fight or knocked out looks at, each turn. Before the duel, it looks at the
/// top of its opponent, but not while it taunts or points at the arena. Then, in the duel and after it, it looks at what
/// it watches, or ahead. In the duel, the fight stance turns the head to the head of the opponent, and the moves keep the
/// head straight.
[[nodiscard]] glm::vec3 FightLook(const ecs::Registry& registry, entt::entity entity, const components::Creature& creature,
                                  const components::Transform& transform, const components::CreatureMindState& mind);
/// Where a script has the creature keep its head turned: a thing where it stands, or a point. The focus is let go once
/// the thing is gone or the script has let the creature go.
[[nodiscard]] std::optional<glm::vec3> ScriptFocusPoint(const ecs::Registry& registry, entt::entity creature,
                                                        components::CreatureMindState& mind);
/// Whether the thing the creature's step waits for is back on the map: out of any hand, flight or tornado. None when the
/// thing is gone, and when the step waits for nothing.
[[nodiscard]] std::optional<bool> WaitedForInMap(const ecs::Registry& registry, const creature_mind::IdleMind& idle);
/// Whether the creature that the creature's step waits to be still is on its way somewhere or turning. A thing that is
/// gone, or is not a creature, does not move.
[[nodiscard]] bool WaitedForMoving(const ecs::Registry& registry, const creature_mind::IdleMind& idle);
/// Whether the creature that a step waits to be free plays an action or moves
[[nodiscard]] bool WaitedForBusy(const ecs::Registry& registry, const creature_mind::IdleMind& idle);
/// Whether the thing a step watches fly is still really in the air: in the physics, and moving fast or clear of the land
/// The villager a step follows to watch, as the creature sees it, while that step is next or under way
[[nodiscard]] std::optional<creature_mind::Senses::FollowedView>
FollowedViewOf(const ecs::Registry& registry, const glm::vec3& from, const components::CreatureMindState& mind);
/// Whether the living object of the step that makes a thing still is in its state of flying
[[nodiscard]] bool StillObjectFlies(const ecs::Registry& registry, const creature_mind::IdleMind& idle);
/// A living thing starts to die at once, without the work of a death: a villager or an animal
void MakeStill(const ecs::Registry& registry, entt::entity thing);
[[nodiscard]] bool WatchedIsInTheAir(const ecs::Registry& registry, const creature_mind::IdleMind& idle);
/// Its player's camera as a creature watching it sees it, only while its step watches the camera; none for a creature
/// without a player
[[nodiscard]] std::optional<creature_mind::Senses::CameraView>
CameraViewOf(const ecs::Registry& registry, entt::entity creature, const creature_mind::IdleMind& idle);
/// The point of focus of the camera of a creature's player, on the land, while its step follows the camera; and whether
/// the point is less than a sixth of a turn from the way its body faces, or in its own map cell. None for a creature
/// without a player.
[[nodiscard]] std::pair<std::optional<glm::vec3>, bool> CameraFocusOf(const ecs::Registry& registry, entt::entity creature,
                                                                      const creature_mind::IdleMind& idle);
/// The player's hand as a creature watching it sees it, only while its step watches the hand; none for a creature
/// without a player
[[nodiscard]] std::optional<creature_mind::Senses::CameraView> HandViewOf(const ecs::Registry& registry, entt::entity creature,
                                                                          const creature_mind::IdleMind& idle);
/// The player's hand as a creature that follows the hand sees it, only while its step follows the hand. If the hand is off
/// the land, the last position of the hand counts.
[[nodiscard]] std::optional<creature_player_attention::FollowHandView>
FollowHandViewOf(const ecs::Registry& registry, entt::entity creature, const components::CreatureMindState& mind,
                 bool bodyPlaying);
/// The player as a creature that watches it sees it, only while its step watches the player
[[nodiscard]] std::optional<creature_player_attention::WatchPlayerView>
WatchPlayerViewOf(const ecs::Registry& registry, entt::entity creature, const components::CreatureMindState& mind,
                  bool bodyPlaying);
/// Lets go of the miracle the creature holds, as it stops what it was doing
void ReleaseCast(entt::entity creature);
/// The developer log of the clips notes that the body refused a clip, because it played another action
void NoteRefused(entt::entity creature, creature_animation_log::Layer layer, size_t clip, size_t playing);
/// What, besides why, picks the faces the creature pulls
[[nodiscard]] creature_face::Feelings FeelingsOf(const components::CreatureMindState& mind);
/// True when the creature has all that its mind needs to think
[[nodiscard]] bool Thinks(const ecs::Registry& registry, entt::entity creature);

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
