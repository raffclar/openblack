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
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Which of the land's objects the game draws on a frame, and so which of them play on in their clips. The game keeps a
// list of the objects on the land's blocks that are in view and near, made again every few turns or when the blocks in
// view change. On a frame where the camera has moved it draws every object in the list and notes which were on screen;
// on a frame where it stands still it draws only those. Things that only move as they are drawn (the scripts' gates)
// freeze while they aren't. Pure functions and a small clock, tested on their own.

namespace openblack::object_draw_list
{

/// A land block's objects are in the list when the block's middle is nearer the eye than this
constexpr float k_Range = 700.0f;
/// The list is made again once more than this many turns have passed since it was last made
constexpr uint32_t k_RebuildTurns = 10;
/// The camera has moved when its eye or what it looks at has gone further than this from where they were when it last
/// moved, as a squared distance
constexpr float k_MovedSquared = 1.0f;
/// A land block is 160 units a side
constexpr float k_BlockSize = 160.0f;
/// A land cell's altitude in units of height
constexpr float k_AltitudeUnit = 0.67f;

/// A land block as the list sees it: where its corner of least x and z is, and the highest altitude of its cells. With
/// the land's reflection drawn in the sea, the block reaches as far below the water as it stands above it.
struct Block
{
	glm::vec2 corner {0.0f};
	uint8_t highestAltitude {0};
	bool reflected {true};
};

/// Whether any of a block can be seen: false only when all eight corners of the box over it lie beyond the same side
/// of the view, or all lie behind the near plane
[[nodiscard]] bool BlockInView(const glm::mat4& viewProjection, float nearPlane, const Block& block);
/// How far the eye is from the block's middle: half way up its box, or at the water's level with the reflection drawn
[[nodiscard]] float BlockDistance(glm::vec3 eye, const Block& block);
/// Whether a block's objects go in the list
[[nodiscard]] bool BlockListed(const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye, const Block& block);

/// How the camera sees the screen, for telling what is on it
struct View
{
	glm::mat4 viewProjection {1.0f};
	float nearPlane {1.0f};
	/// The projection's scale of height, one over the tangent of half the field of view up and down
	float focalHeight {1.0f};
	/// The screen's width over its height
	float aspect {1.0f};
	glm::vec3 eye {0.0f};
};
/// Whether an object's sphere shows on the screen, as the game tests an object it draws: not wholly behind the near
/// plane, and the eye inside it or the square its size covers on the screen overlapping the screen. The square's side
/// is scaled by the screen's width both ways, so it reaches further up and down than across.
/// `origin` is where the object stands, `centre` the middle of its model's box and `radius` the box's half diagonal,
/// both placed and sized as the object is.
[[nodiscard]] bool SphereOnScreen(const View& view, glm::vec3 origin, glm::vec3 centre, float radius);

/// What a frame draws of the list
struct Frame
{
	/// The list was made again this frame
	bool rebuilt {false};
	/// Every object in the list is drawn, and which are on screen noted; otherwise only those last on screen
	bool drawAll {false};
};
/// Whether an object is drawn on a frame: it has to be in the list, and be on screen when last noted unless the frame
/// draws them all
[[nodiscard]] constexpr bool Drawn(const Frame& frame, bool listed, bool lastOnScreen)
{
	return listed && (frame.drawAll || lastOnScreen);
}

/// When the list is made again and whether the camera has moved, frame by frame
class Clock
{
public:
	/// The next frame, at a game turn, with the camera's eye and what it looks at. Forced makes the list again, as a
	/// new land or a change in the blocks in view does.
	[[nodiscard]] Frame Next(uint32_t turn, glm::vec3 eye, glm::vec3 focus, bool forced);
	/// A new land makes the list again on its first frame
	void Reset();

private:
	std::optional<uint32_t> _rebuiltTurn;
	glm::vec3 _eye {0.0f};
	glm::vec3 _focus {0.0f};
};

} // namespace openblack::object_draw_list
