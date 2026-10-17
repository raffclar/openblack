/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/UnderwaterStreams.h"

namespace openblack::ecs::systems
{

/// EXPERIMENT (not for merge): the flowing water in the rivers of the open land, with the experiment's water on. It
/// finds the rivers and their flow when a land opens, moves the leaves and twigs that drift down them, and tells the
/// other parts of the experiment where the rivers' water is.
class StreamWaterSystemInterface
{
public:
	/// The water of a river at a point
	struct Water
	{
		/// The height of the water's surface
		float surface {0.0f};
		/// How deep the water is there
		float depth {0.0f};
		/// The water's velocity across the ground, in units a second
		glm::vec2 flow {0.0f};
	};

	virtual ~StreamWaterSystemInterface() = default;

	/// Once a frame, with the frame's real time: finds the rivers again when another land opens, and moves the drifters
	virtual void Update(std::chrono::milliseconds frameRealTime) = 0;
	/// The rivers of the open land, empty before a land is open
	[[nodiscard]] virtual const graphics::underwater::streams::Network& GetNetwork() const = 0;
	/// The rivers' flow map
	[[nodiscard]] virtual const graphics::underwater::streams::FlowMap& GetFlowMap() const = 0;
	/// A number that changes each time the rivers are found again
	[[nodiscard]] virtual uint32_t GetRevision() const = 0;
	/// The water of a river at a point, or none where no river runs. Only while the experiment's water is on.
	[[nodiscard]] virtual std::optional<Water> WaterAt(glm::vec2 point) const = 0;
	/// The leaves and twigs that drift down the rivers now
	[[nodiscard]] virtual std::span<const graphics::underwater::streams::Drifter> GetDrifters() const = 0;
	/// The height at which a drifter floats
	[[nodiscard]] virtual float DrifterHeight(const graphics::underwater::streams::Drifter& drifter) const = 0;
	/// How loud the rivers are at a point, 0 to 1 (see underwater::streams::Loudness), quieter high over the land; 0
	/// while the water is off
	[[nodiscard]] virtual float LoudnessAt(glm::vec3 point) const = 0;
};

} // namespace openblack::ecs::systems
