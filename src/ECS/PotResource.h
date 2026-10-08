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

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// Resources put down on the land: AddResourceToPos and the pile helpers it uses. Shared by the hand (a
// hand pot let go of) and the miracles (SpellResource's grains). Wiki: docs/bw1-notes/miracles.md, "Food and wood".

namespace openblack::ecs::pot_resource
{

/// The interface that puts the resource down (none for the neutral / script player)
struct Dropper
{
	bool hasInterface {false};                    ///< There is an interface
	PlayerNames player {PlayerNames::PLAYER_ONE}; ///< Its player
	bool isMyInterface {false};                   ///< It is the local player's interface
	/// The player who owned what the interface last picked up, recorded at the pick-up; none when nothing recorded one
	/// (no pick-up yet, or a fish farm without a town). A storage pit's creature deed tells leaving from stealing by it
	std::optional<PlayerNames> sourceOwner {};
};

/// Out of bounds nothing. The 3x3 cells around pos (a spiral) are searched, fixed objects then mobile ones, and every
/// same-resource store or pot whose fire centre is within Get2DRadius x RadiusMultiplierForApplyingPotToPos of pos
/// takes what it accepts. What is left, on land, makes a new MagicFood / MagicWood pile with the pile sound, poisoned
/// and SetSpeedUp(speedUp). Returns amount - left, what went into the stores and pots already there (a new pile's part
/// is not counted, as in the original). `newPile`, when given, gets the new pile (entt::null when none was made).
uint32_t AddResourceToPos(const glm::vec3& position, const Dropper& dropper, ResourceType type, uint32_t amount, bool poisoned,
                          bool speedUp, entt::entity* newPile = nullptr);

/// A pot or a pile takes a resource: a pot of a storage pit gives it to the pit, with the interface that put it down
/// and the poison, any other takes it itself (AddToPotDirect: the pile sound, the cap at maxAmountInPot, poisoned, the size).
/// Returns what was taken; 0 for an object that is not a pot.
uint32_t PotStructureAddResource(entt::entity object, ResourceType type, uint32_t amount, bool poisoned = false,
                                 const Dropper& dropper = {});
/// A storage pit reached by a resource put down at a point (AddResourceToPos) takes it: the pit's AddResource with the
/// interface that put it down and the poison (its town's desire, alignment and belief, and the deed at its end), then,
/// with an interface, the pit tells the creature what was done, whatever it took. Returns what it took.
uint32_t StoragePitTakesPutDownResource(entt::entity store, ResourceType type, uint32_t amount, const Dropper& dropper,
                                        bool poisoned = false);
/// The pot or pile itself takes it, whatever structure it
/// is part of (the pile sound but for the hand's pots, the cap at maxAmountInPot when nextPotForResource < 19, poisoned,
/// the size). Returns what was taken; 0 for an object that is not a pot.
uint32_t AddToPotDirect(entt::entity object, ResourceType type, uint32_t amount, bool poisoned = false);

/// The pile sound at pos, by type and amount (food < 200: G_PileFoodSmall_01..06 (77 + t % 6), else
/// G_PileFood_01/02 (75 + (t & 1)); wood < 200: G_PileWoodSmall_01..06 (92 + t % 6), else G_PileWood_01..06 (86 + t % 6))
void PlayPileSound(entt::entity pile, const glm::vec3& position, ResourceType type, uint32_t amount);
/// The InGame.sad sample PlayPileSound picks for a random value t
[[nodiscard]] int PileSoundSample(ResourceType type, uint32_t amount, uint32_t t);

/// How far a food pile is raised (ecs::object::PileFoodProportionRaised): p = amount / maxInPot, 0 when not
/// positive (an empty pile is 0), else 0.05 + 0.95 min(p, 1); then 1 - (1 - p)^2, clamped to 0..1
[[nodiscard]] float PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot);

/// The 2D radius (ecs::object::Get2DRadius): scale x the larger half extent of the mesh, x the proportion raised for
/// a food pile; wood piles, pots and stores use the plain one
[[nodiscard]] float Get2DRadius(entt::entity object);

/// The radius multiplier for putting a resource down: a pot 2, anything else (a worship site too) 1.2
[[nodiscard]] float RadiusMultiplierForApplyingPotToPos(entt::entity object);

/// IsWater (the cell's water bit; out of the map or without a land block true) and IsDryLand (the cell's altitude
/// byte >= 4; out of the map or without a block false)
[[nodiscard]] bool IsWater(const glm::vec3& position);
[[nodiscard]] bool IsDryLand(const glm::vec3& position);

/// The flag, and for a food pile on switching on the PILEFOOD_SPEEDUP spot visual (46, scale 1, for
/// ever, on the pile); off closes it
void SetSpeedUp(entt::entity pile, bool on);

} // namespace openblack::ecs::pot_resource
