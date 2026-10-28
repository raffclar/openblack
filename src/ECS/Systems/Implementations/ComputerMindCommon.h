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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Registry.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// What the rules of each part of the mind of a computer god, and the hand of the god, look up in the game's world.
/// A helper that only one part uses stays in the file of that part.
namespace openblack::ecs::systems::computer_world
{

struct RuleContext;

/// The registry of the world
[[nodiscard]] ecs::Registry& Entities();
/// The map position of a thing that is still in the world
[[nodiscard]] std::optional<map_coords::MapCoords> CoordsOfThing(entt::entity thing);
/// The plain magic type of a miracle seed, the one with no power-up
[[nodiscard]] MagicType PlainType(SpellSeedType seed);
/// The worship sites around the player's temple, in the order of their places; none where a place is empty
[[nodiscard]] std::array<entt::entity, components::CitadelWorship::k_Places> SitesOf(PlayerNames player);
/// The icons of a worship site, in their order
[[nodiscard]] std::vector<entt::entity> IconsOf(entt::entity site);
/// The first icon of a site that holds the first seed of the magic type
[[nodiscard]] entt::entity SiteIconFor(entt::entity site, MagicType type);
/// The site has its building done and dancers at its altar
[[nodiscard]] bool BuiltWithDancers(entt::entity site);
/// The site has its life full again
[[nodiscard]] bool Repaired(entt::entity site);
/// What the god knows of an icon and its site
[[nodiscard]] computer_mind_rules::IconFacts FactsOf(entt::entity icon);
/// The towns of the player, in the order that the player gained them
[[nodiscard]] std::vector<entt::entity> TownsOf(PlayerNames player);
/// The storage pit of the town that works, none when it has none
[[nodiscard]] std::optional<entt::entity> WorkingPitOf(entt::entity town);
/// Where the town keeps food or wood: its storage pit that works, else its temporary pile, which is made if missing
entt::entity StoreOf(entt::entity town, ResourceType resource);
/// The left hand of a god, made when the god first needs it. A god holds things in its left hand.
entt::entity HandOf(PlayerNames player);
/// What the hand of a god holds, none when the god has no hands or they hold nothing
[[nodiscard]] std::optional<entt::entity> HeldBy(PlayerNames player);
/// The town that has the number
[[nodiscard]] entt::entity TownNumbered(int number);

/// The god reports where its hand and its camera are, as a player of a network game reports them. Only a god that has
/// hands reports.
void ReportHand(PlayerNames player, glm::vec3 hand, glm::vec3 camera);

/// How much the god wants a place: all inside its influence
[[nodiscard]] float PlacePull(const RuleContext& context, const map_coords::MapCoords& place);
/// The share of the miracle's charge that the god can have, with its icon. The request counts one more time.
float ChargeLevel(RuleContext& context, MagicType type, entt::entity& icon);
/// The hand goes to the icon, or makes the miracle's gesture when it is far, before the worshippers chant
void ReachIcon(RuleContext& context, entt::entity icon, MagicType type);

} // namespace openblack::ecs::systems::computer_world
