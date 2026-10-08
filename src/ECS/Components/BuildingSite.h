/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// The building site of a building (standard or workshop), one per entity. Made for a building when it starts to be
/// built; kept by the town's list (Town::buildingSites, head first) and by the building (Abode::buildingSite).
/// ecs::building_sites is its only writer.
/// Not kept: the original's base object (only its being-deleted flag, `beingDeleted`), the global list of sites
/// (new sites at its head; openblack walks the components), the scaffolds "on the way" (nothing uses them).
/// The citadel heart's site also carries components::CitadelBuildingSite
struct BuildingSite
{
	/// the ring of builder positions: 128 entries (the builder positions, the nearest edge and the builders' arrival
	/// all use 128)
	static constexpr size_t k_RingSize = 128;

	/// the root building; null once the site is deleted
	entt::entity root {entt::null};
	/// the builders, the head first (a new builder goes at the head; removing a builder takes every entry of the
	/// villager). The count is builders.size()
	std::vector<entt::entity> builders;
	/// the scaffolds it was made from, the head first (a new scaffold goes at the head; removing a scaffold takes
	/// every entry of it). Entities with
	/// components::Scaffold (ecs::scaffolds)
	std::vector<entt::entity> scaffolds;
	/// world positions in metres
	std::array<glm::vec3, k_RingSize> ring {};
	/// the number of builders, ++ when one is added and -- once when one is removed (also when the villager is not in
	/// the list: it can go negative, as in the original); NOT reset when the site is deleted
	int32_t builderCount {0};
	/// a repair site (set when it is made for a building being repaired)
	bool isRepairSite {false};
	/// the desire boost (BUILD_BUILDING's desire x 5, when building a planned building is forced)
	float desireBoost {0.0f};
	/// the repair base (at creation the life, or 1.1 x life - 0.1 for a built building without a DestructionMesh; a
	/// damaged abode writes 1.1 x life - 0.1); read for the percentage repaired since the damage
	float repairBase {0.0f};
	/// a standard site's wood pile
	entt::entity woodPile {entt::null};
	/// being deleted (not available): the site's deletion tests it first and sets it at its end. (approximate)
	/// openblack sets it first thing (the builders' exits during the deletion may come back to this site); the entity
	/// itself goes through ecs::ToBeDeleted (UNAVAILABLE and the dead list, or at once)
	bool beingDeleted {false};
	/// The site's class: a standard site (most buildings) or a workshop site (the workshop's: no pile of its own, so
	/// `woodPile` stays null; its pile is the workshop's). Set by
	/// building_sites::Create
	enum class Kind : uint8_t
	{
		Standard,
		Workshop,
	};
	Kind kind {Kind::Standard};
};

/// The citadel heart's site, a BuildingSite whose own pile (`woodPile`) is not used: six wood piles 22 m out
/// instead
struct CitadelBuildingSite
{
	static constexpr size_t k_Piles = 6;
	/// the piles (slot i at the i-th WOOD resource position); emptied only by the site's process and by taking a pot
	/// off the structure (deleting the site keeps them, setting its wood pile does nothing)
	std::array<entt::entity, k_Piles> piles {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	/// (openblack) the site's deletion unlinked each pile from it: their structure part no longer reaches this site
	/// (building_sites::SiteOfPile)
	bool pilesUnlinked {false};
};

} // namespace openblack::ecs::components
