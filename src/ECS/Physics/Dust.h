/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/GraphicsHandle.h"

namespace openblack::ecs::physics
{
/// One puff as the renderer draws it (openblack, for the draw snapshot; components::Sprite + Transform before): a
/// camera-facing sprite with angle 0 and no origin (Renderer.cpp drawSprite), normal alpha blending
struct DustParticleDraw
{
	graphics::TextureHandle texture; ///< blobsa.raw
	glm::vec3 position;
	float halfSize;     ///< the quad's half width and half height (the Transform's scale x / y before)
	glm::vec2 uvMin;    ///< the cell's corner in the 8 x 8 sheet
	glm::vec2 uvExtent; ///< one cell, 1 / 8
	glm::vec4 tint;     ///< the colour premultiplied by its alpha
};

/// The dust puffs of the physics (fragments and the ground impacts of AttemptToAddSoundEvent): smoke cells of
/// data\blobs.raw (rows 2-3 of the 8 x 8 sheet), a camera-facing alpha-blended quad of constant colour that grows in
/// 0.125 s and shrinks to nothing at 1 s, moving at its velocity with no gravity. Game time: they stop while paused.
/// They are no entities (the original's are particles, not game objects): the renderer draws Snapshot's list.
class Dust
{
public:
	/// argb as in the original (0xAARRGGBB); size is the quad's half size at its largest.
	static void Emit(glm::vec3 at, glm::vec3 velocity, uint32_t argb, float size);
	/// The velocity of a ground impact's puffs: (LocalRand(201) - 100) x 0.02 on each axis, +-2 units per second, drawn
	/// z, y, x.
	[[nodiscard]] static glm::vec3 RandomVelocity();
	/// The same on the synced stream, a fragment's puffs: GameRand(201).
	[[nodiscard]] static glm::vec3 SyncedRandomVelocity();
	static void Update(float seconds);
	static void Clear();
	/// `out` is cleared and gets every live puff in emission order, as Update left it (a puff emitted since has its
	/// first cell and size 0, as its Sprite had)
	static void Snapshot(std::vector<DustParticleDraw>& out);
	Dust() = delete;
};
} // namespace openblack::ecs::physics
