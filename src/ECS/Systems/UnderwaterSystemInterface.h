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

#include <glm/vec2.hpp>

#include "3D/Seabed.h"
#include "Graphics/Underwater.h"

namespace openblack::ecs::systems
{

/// EXPERIMENT (not for merge): the view under the sea. It keeps the sea bed of the land that is open and lets the
/// camera go down to it.
class UnderwaterSystemInterface
{
public:
	virtual ~UnderwaterSystemInterface() = default;

	/// Whether the camera can go under the sea and the underwater view shows
	[[nodiscard]] virtual bool IsEnabled() const = 0;
	virtual void SetEnabled(bool enabled) = 0;
	/// The effect turned off to measure its cost, or none
	[[nodiscard]] virtual graphics::underwater::Effect GetEffectOff() const = 0;
	/// Turns the next effect off, and the one before on again, in turn
	virtual void CycleEffectOff() = 0;
	/// Starts a debug tour of the camera: by the shore of the first land (see underwater::TourAt), or out along the sea
	/// bed (see underwater::FarTourGround)
	virtual void StartTour(graphics::underwater::Tour tour) = 0;
	/// Once a frame, with the frame's real time: makes the sea bed again when another land is open, finds how deep the
	/// camera is, and turns the land's sounds and the music down while it is under the sea
	virtual void Update(std::chrono::milliseconds frameRealTime) = 0;
	/// How far under the sea's surface the camera is, 0 above it or while the view is off
	[[nodiscard]] virtual float GetEyeDepth() const = 0;
	/// Whether the camera is under the sea's surface and the view is on
	[[nodiscard]] virtual bool IsEyeUnder() const = 0;
	/// The sea bed of the open land; empty before a land is open
	[[nodiscard]] virtual const seabed::Seabed& GetSeabed() const = 0;
	/// A number that changes each time the sea bed is made again
	[[nodiscard]] virtual uint32_t GetSeabedRevision() const = 0;
	/// The lowest the camera can go at a point, given the land's height there: the sea bed under the sea while the view
	/// is on, else the land
	[[nodiscard]] virtual float FloorAt(glm::vec2 point, float landHeight) const = 0;
};

} // namespace openblack::ecs::systems
