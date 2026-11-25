/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace openblack::graphics
{
class Texture2D;

/// A sprite that the frame shows, as the steps of the frame left it before the draw. The renderer fills these on the
/// game's thread, in the order of the draw. The pass of the sprites reads only these, not the world.
struct DrawnSprite
{
	const Texture2D* texture {nullptr};
	const Texture2D* alpha {nullptr};
	/// Where the sprite is drawn: its place, its turn and its size
	glm::mat4 model {1.0f};
	/// The point that the pass tests against its view
	glm::vec3 position {0.0f};
	/// The point that the sprite takes its place from among what blends
	glm::vec3 sortPoint {0.0f};
	/// The part of the texture: its size, then its lowest corner
	glm::vec4 sampleRect {0.0f};
	/// Whether the sprite turns to the camera, has an alpha of its own and adds to what is behind it, 1 or 0 each
	glm::vec4 params {0.0f};
	glm::vec4 tint {1.0f};
	bool additive {false};
	/// The main view shows the sprite
	bool inMainView {true};
	/// A reflection shows the sprite
	bool inReflection {true};

	/// Whether the view shows the sprite: a reflection or the main view
	[[nodiscard]] bool ShownIn(bool reflection) const { return reflection ? inReflection : inMainView; }
};

} // namespace openblack::graphics
