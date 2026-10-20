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

#include <bit>
#include <functional>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::zoom_to
{

/// Where the camera is and what it looks at
struct CameraView
{
	glm::vec3 origin;
	glm::vec3 focus;
};

/// The temple key flies to the temple from this far, and looks down this steeply
inline constexpr float k_TempleDistance = 130.0f;
inline constexpr float k_TemplePitch = std::bit_cast<float>(0x3F490FD8u);
/// The realm key flies over the middle of the island from this far, and looks down this steeply
inline constexpr float k_RealmDistance = 2200.0f;
inline constexpr float k_RealmPitch = std::bit_cast<float>(0x3EC90FDBu);
/// A single tap of the temple key puts the camera this far from the point that it looks at, this steeply
inline constexpr float k_TapDistance = 50.0f;
inline constexpr float k_TapPitch = std::bit_cast<float>(0x3F060A90u);
/// The camera looks at a point this far above the land
inline constexpr float k_FocusAboveGround = 3.0f;
/// A tap that comes more than this many milliseconds after the last single tap is a single tap
inline constexpr int32_t k_DoubleTapMilliseconds = 500;
/// While the camera looks within this far of the place that it flew to, the key flies it back
inline constexpr float k_ReturnRadius = 100.0f;
/// How much the half way point of a flight to a place, or back, rises for each metre across
inline constexpr float k_PlaceRise = std::bit_cast<float>(0x3ECCCCCDu);
/// The flight to the view of a single tap does not rise
inline constexpr float k_TapRise = 0.0f;

/// The land's altitude at a map position (16.16 fixed point)
using Altitude = std::function<float(int32_t mapX, int32_t mapZ)>;

/// A metre value as a map position the way the keys make it: times 65536, times a tenth, truncated
[[nodiscard]] int32_t MapCoordOf(float metres);
/// The point that the camera looks at over a place: the land's altitude there, a little higher
[[nodiscard]] glm::vec3 FocusOver(glm::vec2 place, const Altitude& altitude);
/// The point a distance from the camera along its line of sight. A camera that looks at itself has no line, and
/// the point stays at the camera.
[[nodiscard]] glm::vec3 AlongView(const CameraView& view, float distance);
/// The middle of the island: half way between the first and the last block that holds land, on each axis
[[nodiscard]] glm::vec2 IslandCentre(glm::u16vec2 firstBlock, glm::u16vec2 lastBlock);

/// A flight that a key starts
struct Flight
{
	CameraView view;
	float rise;
	/// The flight goes to a place or back from it, not to the view of a single tap
	bool toPlace;
};

/// One press of the temple key or the realm key
struct Press
{
	/// The time of the press, in milliseconds
	uint32_t now;
	CameraView current;
	/// The place to fly to (x, z): the temple, or the middle of the island
	glm::vec2 place;
	float distance;
	float pitch;
	/// Only the temple key has a single tap
	bool tapAllowed;
	/// How far ahead the world camera finds the land. Without it (a fight in view) a single tap starts no flight.
	std::optional<float> clickDistance;
};

/// The temple key pressed: the temple, or the middle of the island without a temple
[[nodiscard]] Press TempleKey(uint32_t now, const CameraView& current, std::optional<glm::vec2> temple, glm::vec2 islandCentre,
                              std::optional<float> clickDistance);
/// The realm key pressed
[[nodiscard]] Press RealmKey(uint32_t now, const CameraView& current, glm::vec2 islandCentre);

/// The temple and realm keys. A single tap of the temple key gives a pleasing view of the land ahead. A double tap
/// flies to the temple, or over the realm without a temple. The realm key flies over the middle of the island. When
/// the camera still looks at that place, the next press of either key flies back to the last view.
class ZoomToPlaces
{
public:
	/// What the keys keep between presses
	struct State
	{
		/// The time of the last single tap, 0 after a flight to a place or back
		uint32_t lastTap {0};
		/// The camera flew to a place and can fly back
		bool flownTo {false};
		/// The view to fly back to
		CameraView returnTo {};
	};

	ZoomToPlaces() = default;
	explicit ZoomToPlaces(const State& state)
	    : _state(state)
	{
	}

	/// The flight that the press starts, if any
	[[nodiscard]] std::optional<Flight> Act(const Press& press, const Altitude& altitude);

	/// Whether a tap of the temple key at a time is a single tap
	[[nodiscard]] bool IsSingleTap(uint32_t now) const noexcept
	{
		return static_cast<int32_t>(now - _state.lastTap) > k_DoubleTapMilliseconds;
	}
	[[nodiscard]] const State& GetState() const noexcept { return _state; }

private:
	State _state;
};

} // namespace openblack::zoom_to
