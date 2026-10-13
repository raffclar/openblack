/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The swirling colours behind the front end's boxes, as behind the new player's box: data/textures/pin.raw (with its
/// alpha) laid twice over a 41 by 31 grid of points across the screen. Each layer's texture coordinates are pulled about
/// by eight swirls drifting round on angles: a point's coordinates are the sums, for the odd and the even swirls, of
/// (2 cos c + 3) * 3 / (its distance in grid squares from the swirl + 10), the swirl at ((sin a + 1) * 20, (cos b + 1)
/// * 20). The angles turn by their speeds times the frame's milliseconds times 1e-7 as each point of the grid is placed,
/// so they move on a little across the grid as well as from frame to frame.
namespace openblack::gui::menu_backdrop
{

/// The grid's points across and down
constexpr int k_Columns = 41;
constexpr int k_Rows = 31;
constexpr size_t k_Points = static_cast<size_t>(k_Columns) * k_Rows;
/// Two layers of eight swirls
constexpr int k_Layers = 2;
constexpr int k_SwirlsPerLayer = 8;
constexpr size_t k_Swirls = static_cast<size_t>(k_Layers) * k_SwirlsPerLayer;
/// The frame's milliseconds times this, the swirls' turn each time a point is placed
constexpr float k_TurnPerMillisecond = 1.0e-7f;

struct Swirl
{
	/// The three angles, in radians from -pi to pi once they have turned
	glm::vec3 angles {0.0f};
	/// How fast each turns
	glm::vec3 speeds {0.0f};
};

using Swirls = std::array<Swirl, k_Swirls>;
/// A layer's texture coordinates, column by column, each column from the top
using Layer = std::array<glm::vec2, k_Points>;

/// The swirls start at random: for each, its speeds from -1 to 1 and then its angles from -8 to 8, each three drawn last
/// first. random(a, b) is a number from a to b
[[nodiscard]] Swirls Start(const std::function<float(float, float)>& random);
/// An angle brought back by a turn once it has gone past a half turn either way
[[nodiscard]] float Wrap(float angle);
/// A layer's texture coordinates for this frame, its swirls turning on by turn = frame milliseconds * 1e-7 at each
/// point
void PlaceLayer(Swirls& swirls, int layer, float turn, std::span<glm::vec2, k_Points> coordinates);
/// Where a point of the grid is on a screen box of a size: across in fortieths, down in thirtieths, each in whole pixels
[[nodiscard]] glm::ivec2 PointOnScreen(int column, int row, glm::ivec2 size);

/// The black that covers the screen, the box and the pointer as the box comes up and after it goes: it starts at 999 ms
/// of black, which go down by each frame's milliseconds (to 0 at the least) while the box is up and up again once it
/// has gone. The black is 255ths of the milliseconds over 1000; none at 0
struct Fade
{
	int milliseconds {999};
	uint8_t alpha {255};
	/// The box was gone at the last frame
	bool boxGone {false};
};
/// A frame of the fade, with the box's state at its start
void StepFade(Fade& fade, bool boxGone, int frameMilliseconds);
/// The screen is done with once the box was gone at the last frame and the count has passed 999
[[nodiscard]] bool FadeOver(const Fade& fade);

/// The backdrop's texture is kept with 4 bits to each of its colours and its alpha, the low bits dropped: a level of 0
/// to 255 as it is drawn
[[nodiscard]] uint8_t SixteenBitLevel(uint8_t level);

/// The grid is placed once per screen refresh at most, 60 times a second, each time with all the time since the last
struct Pace
{
	float sinceStep {0.0f};
};
/// The milliseconds the swirls turn by this frame, none on a frame they don't
[[nodiscard]] std::optional<int> StepPace(Pace& pace, float frameMilliseconds);
/// A frame's milliseconds from the clock at the last frame the backdrop was drawn and now: none on its first frame
[[nodiscard]] float FrameMilliseconds(std::optional<uint32_t> last, uint32_t now);

} // namespace openblack::gui::menu_backdrop
