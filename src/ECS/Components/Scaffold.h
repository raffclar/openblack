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

#include <entt/entity/entity.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/Mesh.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// A building that a held scaffold offers to the town near it. The building stands where the scaffold is put down.
struct ScaffoldOffer
{
	AbodeInfo info {AbodeInfo::None};
	AbodeNumber number {AbodeNumber::Invalid};
	/// The model of the building. Two offers with the same model show the same phantom building.
	MeshId mesh {};
	/// The turn of the building about the up axis, in radians
	float yAngle {0.0f};
	float scale {1.0f};
};

/// The phantom building that a held scaffold shows: the building that it offers now, and the offer that comes next. The
/// phantom grows to its full size when it starts to show, and becomes small again before the next offer shows.
struct ScaffoldPhantom
{
	/// The offer that the phantom shows, none when the phantom shows no building
	std::optional<ScaffoldOffer> shown;
	/// The offer that shows when the phantom is small again, none when no offer comes
	std::optional<ScaffoldOffer> next;
	/// The milliseconds until the phantom has its full size, or until it is small again
	int32_t changeMs {0};
	/// The phantom grows, or has its full size. When false, the phantom becomes small, or shows no building.
	bool grows {false};
	/// The turn of the phantom about the up axis, in radians. The phantom turns a little each game turn.
	float yAngle {0.0f};
	/// The size of the phantom at its full size
	float scale {0.0f};
	/// The scaffold looked for an offer since the hand last moved
	bool looked {false};
	/// The game turns since the scaffold last showed that it has no offer, 0 when it did not show it recently. The count
	/// stops at 0 after 15 turns.
	uint8_t noOfferTurns {0};
	/// The offer is a village centre that starts a new town
	bool foundsTown {false};
	/// The phantom does not turn and keeps the turn of the offer
	bool fixedAngle {false};
};

/// A scaffold: a wooden frame that a workshop makes and the hand carries to a town. Its scale gives how many scaffolds it
/// is worth.
struct Scaffold
{
	/// The town that the scaffold was made for
	entt::entity town {entt::null};
	/// The workshop that counts the scaffold as its own, none when no workshop counts it
	entt::entity workshop {entt::null};
	/// Its place in the yard of its workshop, 0 to 2
	uint8_t place {0};
	/// The game turn when the scaffold came to rest after a hand let it go. It is 0 when no hand let it go since it was
	/// made or since a hand last took it.
	uint32_t putDownTurn {0};
	/// The abode number of the building that a script gave the scaffold, none when no script gave one
	std::optional<AbodeNumber> scriptBuilding;
	/// A script marked the scaffold to destroy the things in its way when a hand lets it go
	bool destroysThingsInWay {false};
	/// The hand let the scaffold go onto the land, not by force: the scaffold comes down gently
	bool gentle {false};
	/// The player whose hand holds the scaffold or let it go last, until it comes to rest
	std::optional<PlayerNames> handPlayer;
	/// The phantom building that the scaffold shows while a hand holds it
	ScaffoldPhantom phantom;
	/// The building whose site the scaffold is on, none when the scaffold is on no site
	entt::entity site {entt::null};
	/// The copy that shows the phantom building, none until the scaffold first shows a phantom
	entt::entity ghost {entt::null};
};

/// The see-through copy that shows the phantom building of a scaffold. It is only a picture: the game turns make it
/// with the phantom and remove it with the scaffold, and only the frames change it.
struct ScaffoldGhost
{
	/// The scaffold whose phantom the copy shows
	entt::entity scaffold {entt::null};
	/// Its alpha, 0 to 255
	uint8_t alpha {0};
	/// The model that the copy shows in the frames, none when it shows nothing. The copy keeps its model here and has no
	/// Mesh: the frames must not add or remove rows of a component that the game turns use.
	std::optional<Mesh> model;
};

/// The size that a scaffold on a building site shows in the frames, which becomes small as the building grows
struct ScaffoldDrawnScale
{
	float scale {1.0f};
};

} // namespace openblack::ecs::components
