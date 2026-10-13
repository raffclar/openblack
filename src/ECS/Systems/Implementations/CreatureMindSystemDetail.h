/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <utility>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
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
}
namespace openblack::creature_mind_tables
{
struct Tables;
}

/// What the creature mind system's files share: how a species' desires start, and finding food, water and things to
/// hurl at about a creature
namespace openblack::ecs::systems::mind_detail
{
[[nodiscard]] std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> SetupFor(CreatureType species);
[[nodiscard]] std::optional<float> FoodValueOf(entt::entity entity);
[[nodiscard]] std::optional<creature_mind::Wants::WaterSpot> NearestWater(glm::vec2 from);
[[nodiscard]] std::optional<glm::vec2> NearestHurlTarget(ecs::Registry& registry, glm::vec2 from);
/// What a creature knows of a thing, as far as the world here tells it
[[nodiscard]] std::optional<creature_tree::Belief> BeliefOf(const ecs::Registry& registry, entt::entity entity,
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
                             const glm::vec3& place, float distanceWeight);
/// How useful the creature has learnt a food is to eat, 0.1 when nothing is known
[[nodiscard]] float FoodUsefulness(const ecs::Registry& registry, const components::CreatureMindState& mind, entt::entity self,
                                   entt::entity food);
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
} // namespace openblack::ecs::systems::mind_detail
