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
#include <optional>
#include <span>

#include "Enums.h"

/// The rules of a town's graveyard and of the bodies of its dead. A town takes the first of its graveyards to work as
/// its own; every death of one of its people while it has one counts a burial there, and the graves shown on the
/// graveyard's model grow with the burials. A body lies less long when its town had a working graveyard. Free of the
/// game's state so they can be tested on their own.
namespace openblack::graveyard
{

/// A graveyard counts no more burials than this; past it nothing more changes, and it goes on working
inline constexpr uint32_t k_MostBurials = 50;

/// Each burial shows this much of a grave: a new grave appears each time the burials make up a whole one more
inline constexpr float k_GravePerBurial = 0.18f;

/// The burials counted once one more of the town's people has died: one more, until the graveyard is full
[[nodiscard]] constexpr uint32_t BurialsAfterDeath(uint32_t burials)
{
	return burials < k_MostBurials ? burials + 1 : burials;
}

/// How many of the graveyard model's graves are shown for its burials: none before its first, then at least one, and
/// one more for each whole grave the burials make up (nine when full). The model's graves are its parts numbered one
/// to nine; its part numbered ten is the scaffold of the graveyard going up.
[[nodiscard]] uint32_t GravesShown(uint32_t burials);

/// The turns a dead villager lies before it goes: the shorter time when the town it belonged to as it died had a
/// working graveyard
[[nodiscard]] constexpr uint32_t CorpseTurns(bool townHadWorkingGraveyard, uint32_t withGraveyard, uint32_t withoutGraveyard)
{
	return townHadWorkingGraveyard ? withGraveyard : withoutGraveyard;
}

/// Whether a building of this kind takes over as a town's graveyard when its graveyard goes. As in the original game
/// this is any civic building, not only another graveyard: the game tests the building's kind against the graveyard's
/// kind as a mask, which every civic building but the wonder shares.
[[nodiscard]] constexpr bool TakesOverAsGraveyard(AbodeType type)
{
	return (static_cast<uint32_t>(type) & static_cast<uint32_t>(AbodeType::Graveyard)) != 0;
}

/// One of a town's other buildings, in the town's order (the newest first), as a graveyard to take over from one going
struct OtherBuilding
{
	bool working {false};
	bool takesOver {false};
};

/// The town's graveyard once one of its graveyards goes: when the one going was the town's own, the first of its other
/// working buildings that takes over as one does so. Whichever went, the town forgets the graveyard it had, so losing a
/// spare graveyard leaves the town with none until another starts working, as in the original game. The index of the
/// one taking over.
[[nodiscard]] std::optional<size_t> GraveyardAfterLoss(bool lostWasTheTowns, std::span<const OtherBuilding> others);

/// The models a villager wears at its three distances (close, usual, far): the skeleton's at all three while it is a
/// skeleton, its own otherwise
template <typename Model>
[[nodiscard]] constexpr std::array<Model, 3> ModelsWorn(bool skeleton, Model skeletonModel,
                                                        const std::array<Model, 3>& ownModels)
{
	return skeleton ? std::array {skeletonModel, skeletonModel, skeletonModel} : ownModels;
}

} // namespace openblack::graveyard
