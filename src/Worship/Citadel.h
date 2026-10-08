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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::map_coords
{
struct MapCoords;
} // namespace openblack::map_coords

// The citadel as the container of the worship sites: up to 6 sites, one per tribe,
// around the heart at heart angle + slot x 2 pi / 7. The citadel entity is the temple (components::Temple).

namespace openblack::worship::citadel
{
/// The player's citadel (its temple entity with a CitadelWorship), or entt::null
[[nodiscard]] entt::entity Of(PlayerNames player);

/// The six site slots of the player's citadel in slot order (as the guidance's worship site sounds walk them):
/// entt::null for an empty slot; all null when the player has no citadel
[[nodiscard]] std::array<entt::entity, 6> WorshipSitesOf(PlayerNames player);
/// The worship-strain sound fraction: written only by ProcessSpellIcons (the local player's citadel), saved / loaded
/// as 4 bytes. 0 when it is not a citadel.
[[nodiscard]] float StrainSoundFraction(entt::entity citadel);
/// The strain sound fraction as the guidance's worship site sounds read it: below 1 it is kept (a NaN too: the
/// compare is unordered), else 1
[[nodiscard]] float StrainSoundFractionAtMostOne(entt::entity citadel);

/// (The chant music passes the camera's MapCoords and 100.) Of the six slots in order, the site with dancers whose
/// dance centre is nearest, strictly nearer than the best (which starts at maxDistance); entt::null for none or when
/// it is not a citadel
[[nodiscard]] entt::entity FindNearestWorshipSite(entt::entity citadel, const map_coords::MapCoords& coords, float maxDistance);

/// The citadel's heart is built and its life > 0, as the guidance tests it before the heart beat plays
[[nodiscard]] bool HasLivingHeart(entt::entity citadel);

/// The citadel's worship part, from CitadelArchetype (the heart's Y angle from the script's rotation)
void Initialise(entt::entity temple, float heartYAngle);

/// FindOrCreateWorshipSite(town), and the town added to the site if it is not there yet
entt::entity AddTown(entt::entity citadel, entt::entity town);
/// Nothing when the citadel may not make sites or the town may not have one (town::IsAllowedToCreateWorshipSite);
/// else the tribe's site
entt::entity FindOrCreateWorshipSite(entt::entity citadel, entt::entity town);
/// FindTribeWorshipSite, else a new site (the free slot nearest the nearest town of that tribe, else nearest the
/// citadel)
entt::entity FindOrCreateWorshipSite(entt::entity citadel, Tribe tribe);
/// The citadel's site of that tribe, or entt::null
[[nodiscard]] entt::entity FindTribeWorshipSite(entt::entity citadel, Tribe tribe);

/// Every site's ProcessSpellIcons, then the strain sound fraction of the local player's citadel toward the largest
/// strain (0.001 x ms per turn)
void ProcessSpellIcons(entt::entity citadel);

/// CREATE_WORSHIP_SITE: the tribe's site (FindOrCreateWorshipSite, no town check; none -> null); the first town of
/// the player of that tribe: AddTown if missing, and when the town has a building site of it the site is fully built
/// and the building site removed; the site. No such town: fully built and null
entt::entity CreateBuiltWorshipSite(entt::entity citadel, Tribe tribe);

/// (A heart made at full life, a heart built, SET_CAN_BUILD_WORSHIPSITE): for each town of the player,
/// FindOrCreateWorshipSite(town) and the town added to the site when missing; a site not built and repaired gets the
/// town's building site (a new one when it has none) with the desire boost
void OpenWorshipSites(entt::entity citadel, float boost = 0.0f);

/// GET_SPELL_ICON_IN_TEMPLE: the site icon of that magic in any of the sites
[[nodiscard]] entt::entity GetSpellIcon(entt::entity citadel, MagicType type);

/// The citadel's heart (openblack: the citadel's own entity when it is a CitadelHeart), or entt::null
[[nodiscard]] entt::entity HeartOf(entt::entity citadel);
/// Every turn, before the player's towns: the heart's model takes its percentage built; reaching 1 it leaves and
/// enters the map cells again. (pending) the alignment colour and flock part
void Process(entt::entity citadel);
/// The heart's drawn percentage, clamped to 0..1: below 1 the partly built temple (with the inner walls 1.0 m in:
/// physics::PartialBuild into components::DrawMesh), else the whole model
void SetHeartDrawPercent(entt::entity heart, float percent);
/// After abodes::Built: life 1.0, the heart's building site out of every town of the player, OpenWorshipSites(citadel,
/// 0), and for the local player outside a script's wide screen the temple built music (no script music playing, not
/// land 1) and an instant save (not ported)
void HeartBuilt(entt::entity heart);
/// After abodes::Built: the site's building site out of every town of its player
void WorshipSiteBuilt(entt::entity site);

/// SET_INTERFACE_CITADEL (414): the popped value (raw). A script reset sets it to 1
void SetInterfaceCitadel(uint32_t value);
[[nodiscard]] uint32_t InterfaceCitadel();
/// Back to 1, as a script reset does. (pending) openblack's caller of the script reset (a new land's script)
void ResetInterfaceCitadel();
/// Always tappable in a multiplayer game, else when SET_INTERFACE_CITADEL's value != 0
[[nodiscard]] bool EntranceValidToTap(entt::entity entrance);
/// Its heart of the local player and the tapping interface the local one -> go inside the citadel (openblack: the
/// temple interior's Activate). Returns 1
uint32_t EntranceTap(entt::entity entrance, bool myInterface);
/// Whether the entrance is in the hand's pick: its mesh is never drawn, only collided once its temple is fully built.
/// (approximate) the game collides every entrance of a player once one of that player's temples has been drawn whole
/// on the current island; openblack reads the entrance's own heart's draw percent
[[nodiscard]] bool EntranceCollides(entt::entity entrance);
/// Whether the object is a citadel entrance that can be tapped now: the press on it taps it, like an abode
[[nodiscard]] bool IsEntranceValidToTap(entt::entity object);
/// What the hand over a temple's entrance shows (the over-object tooltip, in the influence)
enum class EntranceToolTip : uint8_t
{
	NotEntrance, ///< not an entrance, or its heart is missing or not built: the tooltip goes on as over any object
	Enter,       ///< its heart is built and the player's: "Enter Temple" on the action button
	Nothing,     ///< its heart is built and another player's: no tooltip at all
};
/// The entrance's tooltip for the player's hand. The script's lock (SET_INTERFACE_CITADEL) does not change it
[[nodiscard]] EntranceToolTip EntranceToolTipFor(entt::entity object, PlayerNames player);

/// After the land's features script: for every player with a citadel, every town of the player without a worship
/// site -> AddTown
void PostLoadCleanup();
} // namespace openblack::worship::citadel
