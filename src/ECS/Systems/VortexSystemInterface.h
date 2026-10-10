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

#include <span>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The swirling vortices between the lands. Scripts open one where a land is left (it fades in), where the next land
/// is arrived at and at the volcano's crater (those start open), and later fade them out. While a land vortex opens it
/// levels the ground under it for good.
class VortexSystemInterface
{
public:
	/// What the land shows of an open vortex, on the one land block under its middle: a hole, through which the swirl
	/// under the land shows, and a ring laid over the land around it
	struct GroundMark
	{
		VortexType type;
		glm::vec2 centre;
		/// The land block it is laid on, by its place in the map's grid of blocks
		glm::ivec2 block;
		float baseScale;
		/// How opaque the hole's texture must be, 0 to 255, for the land to be drawn there
		uint8_t holeThreshold;
	};

	virtual ~VortexSystemInterface() = default;

	/// A vortex of a kind made at a point on the land, raised by an altitude above the ground there
	virtual entt::entity Create(glm::vec3 position, VortexType type, float altitude) = 0;
	/// A vortex starts to fade out, whatever it was doing, and goes when it has. Whether the thing was a vortex.
	virtual bool StartFadeOut(entt::entity vortex) = 0;
	/// How open a vortex is now, 0 to 1, eased in and out; 0 for anything that isn't one
	[[nodiscard]] virtual float GetOpenness(entt::entity vortex) const = 0;

	/// At the end of each game turn the vortices whose fade is over open fully or go, and the land vortices level the
	/// ground further as they open
	virtual void ProcessTurn() = 0;
	/// Once a frame, by the frame's game time (none while paused): each open vortex's swirl, kept just under the ground,
	/// and its effect over the land step with its openness, its glow takes its brightness, and its marks on the ground
	/// follow its openness
	virtual void UpdateFrame(float gameSeconds) = 0;
	/// The open vortices' marks on the ground as the last frame left them
	[[nodiscard]] virtual std::span<const GroundMark> GetGroundMarks() const = 0;
};

} // namespace openblack::ecs::systems
