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
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/UnderwaterTap.h"
#include "Graphics/UnderwaterWake.h"

namespace openblack::ecs::systems
{

/// EXPERIMENT (not for merge): the things that move in the experiment's water and what they write into its field of
/// small waves (see underwater::wake): their wakes, the rings where things fall in or bob, the splashes, and the hulls
/// that push the water down. The renderer steps the field by the plan of each frame.
class WaterWakeSystemInterface
{
public:
	/// What writes into the field at its next steps: a thing at a point across the ground with a radius
	struct FieldWrite
	{
		glm::vec2 position {0.0f};
		glm::vec2 velocity {0.0f};
		float radius {1.0f};
		/// For a mover: how far it pushes the water down. For a ring: the depth of the hole that starts it.
		float depth {0.0f};
		/// The foam it makes
		float foam {0.0f};
	};

	/// A splash that shows now
	struct ActiveSplash
	{
		glm::vec3 position {0.0f};
		/// When it started, by the clock of the waves (UnderwaterSystemInterface::GetSeconds)
		float start {0.0f};
		graphics::underwater::wake::Splash splash;
	};

	/// EXPERIMENT (not for merge): a tap of the hand on the water that shows now
	struct ActiveTap
	{
		glm::vec3 position {0.0f};
		/// When it started, by the clock of the waves
		float start {0.0f};
		graphics::underwater::tap::TapSplash splash;
	};

	/// The field this frame
	struct Frame
	{
		graphics::underwater::wake::StepPlan plan;
		/// The low corner of the field across the ground
		glm::vec2 origin {0.0f};
		std::span<const FieldWrite> movers;
		/// The rings that start at the first step of this frame
		std::span<const FieldWrite> rings;
		std::span<const graphics::underwater::wake::Hull> hulls;
		std::span<const ActiveSplash> splashes;
		std::span<const ActiveTap> taps;
		/// Whether the field is flat: the water does not read it
		bool flat {true};
	};

	virtual ~WaterWakeSystemInterface() = default;

	/// Once a frame, after the view under the sea: finds the things in the water, how they move, the splashes they make
	/// and the field's plan for the frame. The field steps by the game time, so it stays still while the game is paused.
	virtual void Update(std::chrono::milliseconds frameGameTime) = 0;
	[[nodiscard]] virtual Frame GetFrame() const = 0;
	/// Throws the things near what the camera looks at (mobile objects, mobile statics, villagers and animals) high over
	/// the water there, so that they splash
	virtual void DropDebugThings() = 0;
	/// Drops the things near what the camera looks at from rest, from a height over the water, in a row across the
	/// focus, so that their splashes show side by side
	virtual void DropDebugRow(float height) = 0;
	/// The hand taps the water at a point on its surface, with a strength (see underwater::tap::SplashFor). `calm` is 0
	/// on the open sea and 1 on calm water.
	virtual void Tap(glm::vec3 position, float strength, float calm) = 0;
	/// Plays the explorers' boat film where it plays on the first land, so that the boat makes its wake
	virtual void PlayDebugBoat() = 0;
};

} // namespace openblack::ecs::systems
