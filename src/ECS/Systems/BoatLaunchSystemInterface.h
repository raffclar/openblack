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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "ECS/BoatLaunchRules.h"

namespace openblack::particles::draw
{
struct Frame;
}

namespace openblack::ecs::systems
{

/// The explorers' boat film, which the script of the Land 1 silver scroll of the explorers plays as they leave: the
/// sailors push their boat down the beach into the sea with spray and sound, then it sails away to the south west for a
/// minute with its crew and cargo on deck and its wake behind it (see boat_launch). Only one plays at a time.
class BoatLaunchSystemInterface
{
public:
	/// Where the film is played: the dry dock the boat is pushed from, and where its sailing starts
	struct Places
	{
		glm::vec3 dock {boat_launch::k_LaunchPlace};
		glm::vec3 sailingStart {boat_launch::k_SailingStart};
	};

	/// What the inspector shows
	struct State
	{
		bool playing {false};
		boat_launch::Clocks clocks;
		Places places;
		/// Where the boat is drawn, while it is
		std::optional<glm::mat4> boat;
		/// The things drawn in the film now: the boat with the sailors or with its deck
		uint32_t parts {0};
	};

	virtual ~BoatLaunchSystemInterface() = default;

	/// The film starts again from the launch, at the explorers' beach on Land 1, or elsewhere for the testbed
	virtual void Play() = 0;
	virtual void Play(const Places& places) = 0;
	/// The film goes at once with everything in it, as a new land opens
	virtual void Stop() = 0;
	/// Each drawn frame, by its game milliseconds: the film moves on, its sounds and spray come at their times, and its
	/// things are placed and posed
	virtual void Update(uint32_t milliseconds) = 0;
	/// The boat's wake drawn this frame, added to the particles' frame
	virtual void CollectDrawFrame(particles::draw::Frame& frame) const = 0;

	[[nodiscard]] virtual bool IsPlaying() const = 0;
	[[nodiscard]] virtual State GetState() const = 0;
};

} // namespace openblack::ecs::systems
