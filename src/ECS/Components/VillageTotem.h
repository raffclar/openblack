/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/VillageTotem.h"

namespace openblack::ecs::components
{

/// A town centre's totem, on its plinth's entity: the plinth and the icon standing on it rise together with the share of
/// the town's people who worship. The hand takes hold of it to set that share.
struct VillageTotem
{
	/// The town centre it stands on, and the entity of the icon on top: the town player's creature, or the hand
	entt::entity townCentre {entt::null};
	entt::entity icon {entt::null};
	/// Where the plinth stands with no rise
	float restY {0.0f};
	/// Its share and how it eases there
	village_totem::Ease ease;
	/// The share the hand holds it at; it is the share it was last set to while the hand doesn't hold it
	float held {0.0f};
	/// Whether the hand holds it
	bool gripped {false};
	/// The see-through second plinth and icon at the other share, while the two stand apart
	entt::entity ghost {entt::null};
	entt::entity ghostIcon {entt::null};
	/// How high it was last drawn risen, none until it is drawn after its icon is put on
	std::optional<float> lastShownRise;
	/// The next time it moves shows no tooltip: it was only just given its player's icon
	bool quietOnce {false};
};

} // namespace openblack::ecs::components
