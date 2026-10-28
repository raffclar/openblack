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

#include <memory>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/DanceMoves.h"
#include "Enums.h"

namespace openblack::dance
{
struct DanceFile;
}

namespace openblack::ecs::systems
{

/// The dances: made by the scripts, the towns and the worship sites, each going on a turn at a time
class DanceSystemInterface
{
public:
	virtual ~DanceSystemInterface() = default;

	/// A dance of a row of the dances' table about a place, for a thing, dancing `duration` turns at a time (none for
	/// no end); none when the row is out of the table or its file can't be read. Worship sites', towns' and scripts'
	/// dances are all made here. A dance that is not a worship site's or a temple's gets a footpath from the storage pit
	/// of the town given, or of the town nearest it when none is given.
	virtual entt::entity Create(DanceInfo type, const glm::vec3& place, entt::entity owner, uint32_t duration,
	                            bool madeByScript, entt::entity town) = 0;
	/// A dance goes, its dancers going back to deciding what to do
	virtual void Destroy(entt::entity dance) = 0;
	/// The file of a row of the dances' table; none when the row is out of the table or its file can't be read
	[[nodiscard]] virtual std::shared_ptr<const dance::DanceFile> FileOf(DanceInfo /*type*/) { return nullptr; }
	/// Each turn, after the players and before the living: each dance's turn
	virtual void ProcessTurn() = 0;
	/// Each frame that the camera reads the keys: a grip of the land ends the camera of a dance
	virtual void ProcessCameraKeys(bool /*gripsLand*/) {}
	/// Each turn, when the camera does its checks: the camera of a dance that ended, or whose dance went, gives the
	/// camera back to the camera that it took over from
	virtual void CheckCamera() {}
	/// The shapes the dances' groups stand in, with those that the game reads from its data folder
	[[nodiscard]] virtual const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& Shapes()
	{
		return dance_moves::Shapes();
	}
};

} // namespace openblack::ecs::systems
