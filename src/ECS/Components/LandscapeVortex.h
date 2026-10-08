/*******************************************************************************
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
#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::vortex_save
{
struct Reader;
}

namespace openblack::ecs::components
{
/// What the vortex's particle rule (UR_VortexAttract) pulls in, made when an In vortex takes an object.
/// Wiki: docs/bw1-notes/vortex.md.
struct VortexObjectInfo
{
	entt::entity object {entt::null};
	bool fromPhysics {false}; ///< it was flying (UR_VortexAttract first takes its body away)
	glm::vec3 velocity {0.0f};
	glm::mat3 rows {1.0f}; ///< the matrix's rows; the translation below
	glm::vec3 position {0.0f};
	bool scriptHeld {false}; ///< the object was script held (the fling at the end)
	entt::entity vortex {entt::null};
};

/// A landscape vortex (docs/bw1-notes/vortex.md): In, Out or Volcano, made with a radius of 50.0 (CHL CREATE
/// Vortex).
struct LandscapeVortex
{
	VortexType type {VortexType::In};
	int32_t info {0};                                  ///< the GVortexInfo row
	VortexStateType state {VortexStateType::Inactive}; ///< from the info at creation
	uint32_t stateTurn {0};                            ///< the turn the current fade started
	float radius {0.0f};                               ///< Create's 50.0, the In's suck radius
	glm::vec3 position {0.0f};                         ///< the position in metres
	/// the four particle systems of the info (one on the ground): psys ids, 0 for none. (pending) their creation
	/// through the snapshot
	std::array<uint32_t, 4> psys {};
	float alpha {0.0f};                  ///< the last fade value f drawn
	float landQ {0.0f};                  ///< the last q the land was flattened with
	std::vector<uint8_t> savedAltitudes; ///< the 11 x 11 original cell altitudes
	float meanAltitude {0.0f};           ///< their mean
	// In
	/// the vortex save "vortex.txt" (In writes, Out reads) through which the objects an In swallows come out of the
	/// next land's Out. (pending) the object writers and the reader: not ported, nothing is written; the Out goes
	/// straight to its new villagers
	bool saveFile {false};
	std::vector<VortexObjectInfo> queue; ///< what the particle system queues for UR_VortexAttract
	// Out
	entt::entity town {entt::null};        ///< GetNearestTown(pos, 200) when unset (SetTown)
	entt::entity flock {entt::null};       ///< the script's flock (SetFlockParams)
	entt::entity animalFlock {entt::null}; ///< the animals' Flock made at the first hand-over
	glm::vec3 flockPosition {0.0f};
	float flockA {0.0f}; ///< truncated into one of the animal Flock's parameters
	float flockB {0.0f}; ///< truncated into another of the animal Flock's parameters
	/// its reader of vortex.txt (made at construction; vortex_save::OpenReader), nullptr once read to the end or
	/// without a save folder
	std::shared_ptr<vortex_save::Reader> reader;
	std::vector<entt::entity> thrownVillagers; ///< kept alive: SetLife(1.0)
	uint32_t statObjects {0};                  ///< VortexStatType TotalObjects
	uint32_t statResources {0};                ///< food + wood of what came out
	uint32_t statVillagers {0};                ///< villagers out; up to 30 made new
};
} // namespace openblack::ecs::components
