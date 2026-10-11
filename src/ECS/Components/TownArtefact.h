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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A rock, static or dead tree a player put down gently by a town or a worship site: an artefact, which the town's people
/// dance round. Its worth grows as they dance in its owner's town, and a town that isn't its owner's gives the owner
/// belief while it dances there. A worthy artefact glows, and a worthier one shows its owner's symbol.
struct TownArtefact
{
	/// The town and the worship site it last joined. Both are kept when it is taken out of them (into a hand), so that
	/// it impresses neither again when it is put back.
	entt::entity town {entt::null};
	entt::entity site {entt::null};
	/// Whether it is among its town's artefacts, and among its site's, now; a hand taking it takes it out of both
	bool inTown {false};
	bool inSite {false};
	/// When it joined its town's artefacts: a town looks after its newest first
	uint64_t joined {0};
	/// The player who made it an artefact, or last took it
	std::optional<PlayerNames> player;
	/// Its worth: how much it impresses villagers when it is made, raised as people dance round it
	float value {0.0f};

	/// Its dance, once a villager has wanted to join one, and whether its town has seen it start since
	entt::entity dance {entt::null};
	bool dancing {false};
	/// The turn its town saw its dance start
	uint32_t danceStarted {0};

	/// The magic effect over it, once it is worth enough, while it lasts
	uint32_t effect {0};
	/// Its owner's symbol, made with the effect, and the game milliseconds it has been drawn for (its glows turn only
	/// while it is drawn)
	bool symbol {false};
	float symbolMilliseconds {0.0f};
};

/// A town's last turns of giving each player belief for the artefacts of theirs its people dance round
struct TownArtefactGifts
{
	std::array<uint32_t, static_cast<size_t>(PlayerNames::_COUNT)> lastGift {};
};

} // namespace openblack::ecs::components
