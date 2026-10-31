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

#include <chrono>
#include <span>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/StormClouds.h"
#include "ECS/Components/Mist.h"

namespace openblack::ecs::systems
{

/// One of the clouds of a storm (see storm_clouds), a puff of mist that wanders over the storm. Only the frames make,
/// move and draw it, so it is not a part of the world. The alpha of the mist shows how strong the storm is. The colour
/// of the land, tinted by the shade of the storm, gives the colour of the cloud.
struct StormCloud
{
	/// The storm that the cloud is a part of. The cloud goes when the storm goes.
	entt::entity storm {entt::null};
	/// The position of the cloud in the order in which the clouds of its storm gathered
	uint8_t index {0};
	storm_clouds::Drift drift;
	/// The size of the cloud as a part of the reach of the storm
	float sizeFactor {0.0f};
	/// Where the frames draw the cloud
	glm::vec3 position {0.0f};
	components::Mist mist {};
	/// The shade of the storm, from 0 up: how much darker the cloud is than the colour of the land
	float shade {0.0f};
	/// How bright the last flash of lightning of the storm makes the cloud, 0 to 255
	uint8_t flashGrey {0};
};

/// The frames of the clouds of the sky (components::Cloud, things of the drawn registry) and the clouds of the storms.
/// Only a computer that draws the world has this system.
class CloudSystemInterface
{
public:
	virtual ~CloudSystemInterface() = default;
	/// Removes the clouds of the storms of the last land. The land start makes the sky's clouds (CloudArchetype).
	virtual void Reset() = 0;
	/// Moves the clouds with the wind by the game time that has passed, which stops while the game is paused
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// Every storm gathers its own clouds, one a frame up to its count, and they wander over it with the frames drawn;
	/// those of storms that have gone go with them. The game turns do not read or change these clouds.
	virtual void UpdateStormClouds(std::chrono::milliseconds frameRealTime) = 0;
	/// Moves the animation of the mist of each storm cloud on by the game time of the frame, as for every other mist
	virtual void AnimateStormClouds(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// The clouds of all the storms, for the draw
	[[nodiscard]] virtual std::span<const StormCloud> GetStormClouds() const = 0;
};

} // namespace openblack::ecs::systems
