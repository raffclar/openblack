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
#include <optional>
#include <string_view>
#include <utility>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureDrink.h"
#include "Creature/CreatureIdleMind.h"
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
} // namespace openblack::ecs::systems::mind_detail
