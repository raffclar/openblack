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

#include <entt/entity/entity.hpp>

#include "3D/TempleInteriorInterface.h"
#include "Enums.h"

namespace openblack::ecs::components
{
/// Which of a room's meshes a part is: the game keeps the room and its floor apart, as the main room reflects itself
/// in its floor
enum class TempleInteriorMesh
{
	Room,
	Floor,
	/// The creature's room's waterfall and pools, whose texture slides down them
	Water,
	/// The main room's pool, which the main room draws twice over itself, turned and shimmering
	Pool,
	Other,
};

struct TempleInteriorPart
{
	TempleRoom room;
	TempleInteriorMesh mesh {TempleInteriorMesh::Other};
};

struct Temple
{
	PlayerNames owner;
};

/// The building state of a CitadelPart (the CitadelHeart, a WorshipSite), as components::Abode keeps them for the
/// abodes (ecs::abodes and ecs::building_sites read both)
struct CitadelPartBuild
{
	/// bit 1: under construction (set at creation from underConstruction)
	static constexpr uint32_t k_UnderConstruction = 0x2;
	/// bit 2: not repaired (a rebuild plan's heart; cleared once repaired)
	static constexpr uint32_t k_NotRepaired = 0x4;
	/// bit 3: built (at creation without underConstruction, or once built)
	static constexpr uint32_t k_Built = 0x8;
	uint32_t buildFlags {k_Built};
	/// PercentBuilt: the creation's percent, 0 under construction
	float percentBuilt {1.0f};
	/// the building site (components::BuildingSite's entity), null without one
	entt::entity buildingSite {entt::null};
};

/// The CitadelHeart on the temple entity, with components::Temple and CitadelPartBuild. The first heart of a
/// player's citadel also carries the Citadel (components::CitadelWorship)
struct CitadelHeart
{
	/// The entity with the CitadelWorship
	entt::entity citadel {entt::null};
	/// The town of the plan it was made from; its own town is still none
	entt::entity town {entt::null};
	/// The CitadelEntrance
	entt::entity entrance {entt::null};
	/// The scale: the plan's (or 1.0 for CREATE_CITADEL); it only feeds the influence, the temple is drawn at scale 1
	float scale {1.0f};
	/// The drawn citadel's percent (set by the citadel's process): the partly built temple is drawn below 1
	/// (abodes::RedrawConstruction)
	float drawPercent {0.0f};
};

/// CitadelEntrance: the temple's door, tapped to go inside
struct CitadelEntrance
{
	entt::entity heart {entt::null};
};
} // namespace openblack::ecs::components
