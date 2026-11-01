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
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Vortex.h"
#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"

namespace openblack::particles
{
struct CarriedObject;
struct RestoredEffect;
} // namespace openblack::particles

namespace openblack::ecs::systems
{
class ObjectDrawListSystemInterface;

/// The swirling vortices between the lands. Scripts open one where a land is left (it fades in), where the next land
/// is arrived at and at the volcano's crater (those start open), and later fade them out. While a land vortex opens it
/// levels the ground under it for good.
class VortexSystemInterface: public SavedStateInterface
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

	/// How a thing a vortex starts to draw in starts: where it stands, turned, the velocity it is taken with and whether
	/// it can't be destroyed
	struct Taken
	{
		glm::vec3 position {0.0f};
		glm::mat3 rotation {1.0f};
		glm::vec3 velocity {0.0f};
		bool indestructible {false};
	};

	/// A vortex as a saved game of the game keeps it
	struct RestoredVortex
	{
		/// The point on the land under its middle
		glm::vec3 position {0.0f};
		VortexType type {VortexType::In};
		VortexStateType state {VortexStateType::Active};
		float altitude {0.0f};
		/// The heights of the square that it levels, as they were before it levelled them, and their average
		std::vector<uint8_t> groundHeights;
		float groundAverage {0.0f};
		/// The saved state of the effect that draws things in, which continues from it; none to start a new effect
		std::shared_ptr<const particles::RestoredEffect> mover;
		/// The text that it wrote to the crossing file, none when it writes no crossing file
		std::optional<std::vector<uint8_t>> crossing;
	};

	/// A vortex of a kind made at a point on the land, raised by an altitude above the ground there
	virtual entt::entity Create(glm::vec3 position, VortexType type, float altitude) = 0;
	/// A vortex made again from a saved game of the game. Its state starts again at this turn, as the clocks of its
	/// state start again in the game's load. It levels the land again from the saved heights.
	virtual entt::entity Restore(const RestoredVortex& saved) = 0;
	/// A vortex starts to fade out, whatever it was doing, and goes when it has. Whether the thing was a vortex.
	virtual bool StartFadeOut(entt::entity vortex) = 0;
	/// A vortex that leads into the land is told whom those who come out join and where its animals gather; any other
	/// vortex takes no notice. Whether the thing was a vortex.
	virtual bool SetArrival(entt::entity vortex, const components::Vortex::Arrival& arrival) = 0;
	/// How open a vortex is now, 0 to 1, eased in and out; 0 for anything that isn't one
	[[nodiscard]] virtual float GetOpenness(entt::entity vortex) const = 0;

	/// At the end of each game turn the vortices whose fade is over open fully or go, and the land vortices level the
	/// ground further as they open
	virtual void ProcessTurn() = 0;
	/// Once a frame, after a frame of this real and game time (no game time while paused): each open vortex's swirl,
	/// kept just under the ground, steps with its openness, and its marks on the ground follow its openness. Every vortex
	/// does this, drawn or not. The swirl steps no more often than 60 times a second, each step by all the game time
	/// since the last, and only in a frame that draws the land.
	virtual void UpdateFrame(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime,
	                         bool landDrawn) = 0;
	/// Once a frame, after the land's draw list is made: each open vortex that the list draws steps its effect over the
	/// land with its openness, and its glow takes its brightness. A vortex that the list does not draw shows neither.
	/// The effect steps no more often than 60 times a second, each step by all the game time since the last, and only in
	/// a frame that draws the land.
	virtual void UpdateDrawnFrame(ObjectDrawListSystemInterface& drawList, std::chrono::milliseconds frameRealTime,
	                              std::chrono::milliseconds frameGameTime, bool landDrawn) = 0;
	/// A vortex's particle starts drawing in a thing the vortex handed it: it leaves the land and its own doings and,
	/// unless it can't be destroyed, its town or its young, and is written to the crossing file for the next land. None
	/// when it can't be carried.
	virtual std::optional<Taken> EnterVortex(const std::shared_ptr<particles::CarriedObject>& carried) = 0;
	/// A thing flying in the physics struck a vortex: an open vortex out of the land takes it as it flies, a creature
	/// only as it would draw one in from the land
	virtual void StruckBy(entt::entity vortex, entt::entity hitter, const components::DrawnIntoVortex::Flight& flight) = 0;
	/// A thing that can't be destroyed comes back out of a vortex's middle, thrown out at an angle across the land at a
	/// scale of the usual speeds
	virtual void ThrowOutOfVortex(entt::entity object, float angle, float scale) = 0;
	/// Whether a vortex into the land threw the thing out and still keeps it (a villager it threw flies with the
	/// vortex's own flailing)
	[[nodiscard]] virtual bool IsThrownOutByAVortex(entt::entity thing) const = 0;

	/// The open vortices' marks on the ground as the last frame left them
	[[nodiscard]] virtual std::span<const GroundMark> GetGroundMarks() const = 0;
};

} // namespace openblack::ecs::systems
