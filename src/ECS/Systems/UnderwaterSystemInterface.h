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
#include <optional>
#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Seabed.h"
#include "Graphics/CloudLightning.h"
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
	/// Whether an effect is drawn: the effect turned off to measure it is not, nor what the quality tier leaves out
	[[nodiscard]] virtual bool IsEffectOn(graphics::underwater::Effect effect) const = 0;
	[[nodiscard]] virtual graphics::underwater::Quality GetQuality() const = 0;
	/// The next quality tier, High after Low
	virtual void CycleQuality() = 0;
	/// Turns the next effect off, and the one before on again, in turn
	virtual void CycleEffectOff() = 0;
	/// Turns the effect before off, and the one after on again: from none, one step reaches the last effect
	virtual void CycleEffectOffBack() = 0;
	/// The debug choice of the volumetric clouds' cover (volumetric_clouds::CoverChoice), 0 for the weather's own
	[[nodiscard]] virtual uint8_t GetCloudCover() const = 0;
	/// Moves on to the next choice of the clouds' cover, in turn
	virtual void CycleCloudCover() = 0;
	/// The debug choice of how strong the aurora is, 0 to 1 (atmosphere::AuroraTonight); none for the night's own
	/// strength
	[[nodiscard]] virtual std::optional<float> GetAuroraChoice() const = 0;
	virtual void SetAuroraChoice(std::optional<float> strength) = 0;
	/// Whether the aurora has its full march at the screen's full size, for a debug comparison with its pass at half
	/// size
	[[nodiscard]] virtual bool IsAuroraFullSize() const = 0;
	virtual void SetAuroraFullSize(bool fullSize) = 0;
	/// The debug choice of how strong the noctilucent clouds are, 0 to 1. Without it the night's own strength shows
	/// (graphics::atmosphere::NoctilucentStrength).
	[[nodiscard]] virtual std::optional<float> GetNoctilucentOverride() const = 0;
	virtual void SetNoctilucentOverride(std::optional<float> strength) = 0;
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
	/// The height of the sea now, with the tide: never higher than the usual height
	[[nodiscard]] virtual float GetSeaLevel() const = 0;
	/// How the sea moves now, from the weather
	[[nodiscard]] virtual const graphics::underwater::WaveState& GetWaves() const = 0;
	/// The clock the waves move by, in seconds
	[[nodiscard]] virtual float GetSeconds() const = 0;
	/// Where the things that float on the sea are now, at most a few, the nearest to the camera first
	[[nodiscard]] virtual std::span<const glm::vec3> GetFloaters() const = 0;
	/// A thing that floats on the sea, drawn on the waves: lifted to the sea's height with the tide and the waves, and
	/// tilted with them. Things that do not float keep their model.
	[[nodiscard]] virtual glm::mat4 FloatOnWaves(const glm::mat4& model) const = 0;
	/// Brings a storm over the camera for a short time, to see the waves in it
	virtual void StartDebugStorm() = 0;
	/// Drops the mobile objects near what the camera looks at into the physics, so that those over the sea float
	virtual void DropDebugFloaters() = 0;
	/// The sea bed of the open land; empty before a land is open
	[[nodiscard]] virtual const seabed::Seabed& GetSeabed() const = 0;
	/// The reef on the sea bed, made with it
	[[nodiscard]] virtual std::span<const seabed::ReefPlant> GetReef() const = 0;
	/// The waterspouts over the open sea this frame (none unless a strong storm blows)
	[[nodiscard]] virtual std::span<const graphics::underwater::Waterspout> GetWaterspouts() const = 0;
	/// The rogue wave on the open sea this frame, or none (only in a strong storm)
	[[nodiscard]] virtual const std::optional<graphics::underwater::RogueWave>& GetRogueWave() const = 0;
	/// The last strike of the storm's lightning, as the sea sees it
	[[nodiscard]] virtual const graphics::underwater::SeaStrike& GetSeaStrike() const = 0;
	/// How brightly the lightning lights the sea now, 0 to 1: x the flash of the sky over the camera, y the light of the
	/// bolt where it struck
	[[nodiscard]] virtual glm::vec2 GetSeaFlash() const = 0;
	/// The flashes of the storms' lightning that light the clouds from inside now
	[[nodiscard]] virtual std::span<const graphics::cloud_lightning::FlashEvent> GetCloudFlashes() const = 0;
	/// Where a strike of a storm comes down while the view is on: under the core of one of the storm's thunderstorm
	/// cells (graphics::cumulonimbus), at the height of the bottom of the cell. `wanted` is the place that the storm
	/// chose. None while the view is off, or when the storm has no cell.
	[[nodiscard]] virtual std::optional<glm::vec3> StormStrikePlace(uint32_t stormSerial, glm::vec2 wanted) const = 0;
	/// The sea's currents now (see underwater::CurrentAt)
	[[nodiscard]] virtual const graphics::underwater::CurrentState& GetCurrent() const = 0;
	/// A number that changes each time the sea bed is made again
	[[nodiscard]] virtual uint32_t GetSeabedRevision() const = 0;
	/// The lowest the camera can go at a point, given the land's height there: the sea bed under the sea while the view
	/// is on, else the land
	[[nodiscard]] virtual float FloorAt(glm::vec2 point, float landHeight) const = 0;
};

} // namespace openblack::ecs::systems
