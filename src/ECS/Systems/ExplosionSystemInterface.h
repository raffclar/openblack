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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Blast/DustPuff.h"
#include "Blast/RootsPile.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Systems/SavedStateInterface.h"

namespace openblack::particles::draw
{
struct Frame;
}

namespace openblack::ecs::systems
{

/// What explosions leave after them besides their particles: the heap of rubble on the land with its puff of dust, and the
/// camera shaking as they land near it
class ExplosionSystemInterface: public SavedStateInterface
{
public:
	virtual ~ExplosionSystemInterface() = default;

	/// A pile of roots at a point, turned and sized as given, with its puff of dust: the rubble of a blast, and the hole
	/// of a tree that the hand pulls free. Only this computer draws the pile (see roots_pile).
	virtual void AddRubble(const glm::vec3& centre, const glm::mat3& rotation, float scale) = 0;
	/// A puff of smoke or dust of a size at a point, its kind's colour unless given one (0xRRGGBB): the dust a tree taking
	/// root again throws up, the smoke a dead body leaves as it goes
	virtual void AddSmoke(const glm::vec3& centre, float size, dust_puff::Kind kind,
	                      std::optional<uint32_t> colour = std::nullopt) = 0;
	/// The camera shakes while it is in a radius of a point. The strength decreases to zero over the whole milliseconds
	virtual void AddShake(const glm::vec3& position, float radius, float strength, int32_t milliseconds, bool verticalOnly) = 0;
	/// Whether any shake is going on, near the camera or not
	[[nodiscard]] virtual bool IsShaking() const = 0;
	/// Once a frame of the game's time: the dust flies and the camera shakes
	virtual void Update(float milliseconds) = 0;
	/// Once a frame that draws the land, before the draw: the piles of roots fade and go by the game time of the frame
	virtual void AgeRootsPiles(std::chrono::milliseconds gameTime) = 0;
	/// The piles of roots that this frame draws, the oldest first
	[[nodiscard]] virtual std::span<const roots_pile::Pile> RootsPiles() const = 0;
	/// The dust drawn this frame, added to the particles' frame
	virtual void CollectDrawFrame(particles::draw::Frame& frame) const = 0;
	/// A ghost of a thing that has just gone flickers out where it stood. The ghosts are drawn only, and are no things of
	/// the world.
	virtual void AddGhost(const components::DestructionGhost& ghost) = 0;
	/// The ghosts that flicker out now
	[[nodiscard]] virtual std::span<const components::DestructionGhost> Ghosts() const = 0;
	/// A new land: no rubble, no dust, no shaking
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
