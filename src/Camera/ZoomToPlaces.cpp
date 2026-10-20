/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ZoomToPlaces.h"

#include <cmath>

#include "3D/MapCoords.h"
#include "ScriptCameraModel.h"

namespace openblack::zoom_to
{

namespace
{
/// The width of a block of land in metres
constexpr float k_BlockSize = 160.0f;
constexpr float k_FixedPerMetre = 65536.0f;
constexpr float k_Tenth = std::bit_cast<float>(0x3DCCCCCDu);

/// The heading of the camera, as the keys keep it for the flight
[[nodiscard]] float HeadingOf(const CameraView& view)
{
	return script_camera::HeadingAndPitchFromPoints(view.origin, view.focus).heading;
}

/// The view from a distance and pitch at a heading, that looks at the point over the place
[[nodiscard]] CameraView ViewOver(glm::vec2 place, float distance, float heading, float pitch, const Altitude& altitude)
{
	const auto focus = FocusOver(place, altitude);
	return {.origin = script_camera::PointFromDistanceHeadingAndPitch(focus, distance, heading, pitch), .focus = focus};
}
} // namespace

int32_t MapCoordOf(float metres)
{
	return map_coords::FtoL((metres * k_FixedPerMetre) * k_Tenth);
}

glm::vec3 FocusOver(glm::vec2 place, const Altitude& altitude)
{
	return {place.x, altitude(MapCoordOf(place.x), MapCoordOf(place.y)) + k_FocusAboveGround, place.y};
}

glm::vec3 AlongView(const CameraView& view, float distance)
{
	auto direction = view.focus - view.origin;
	if (direction.x != 0.0f || direction.y != 0.0f || direction.z != 0.0f)
	{
		// The squares add z first, then y, then x
		const float length =
		    std::sqrt(((direction.z * direction.z) + (direction.y * direction.y)) + (direction.x * direction.x));
		const float share = 1.0f / length;
		direction = {direction.x * share, direction.y * share, direction.z * share};
	}
	return {(direction.x * distance) + view.origin.x, (direction.y * distance) + view.origin.y,
	        (direction.z * distance) + view.origin.z};
}

glm::vec2 IslandCentre(glm::u16vec2 firstBlock, glm::u16vec2 lastBlock)
{
	const auto metres = [](uint16_t block) { return static_cast<float>(block) * k_BlockSize; };
	return {(metres(lastBlock.x) + metres(firstBlock.x)) * 0.5f, (metres(lastBlock.y) + metres(firstBlock.y)) * 0.5f};
}

Press TempleKey(uint32_t now, const CameraView& current, std::optional<glm::vec2> temple, glm::vec2 islandCentre,
                std::optional<float> clickDistance)
{
	// Without a temple the key goes over the realm, but a single tap still gives the pleasing view
	return {.now = now,
	        .current = current,
	        .place = temple.value_or(islandCentre),
	        .distance = temple.has_value() ? k_TempleDistance : k_RealmDistance,
	        .pitch = temple.has_value() ? k_TemplePitch : k_RealmPitch,
	        .tapAllowed = true,
	        .clickDistance = clickDistance};
}

Press RealmKey(uint32_t now, const CameraView& current, glm::vec2 islandCentre)
{
	return {.now = now,
	        .current = current,
	        .place = islandCentre,
	        .distance = k_RealmDistance,
	        .pitch = k_RealmPitch,
	        .tapAllowed = false,
	        .clickDistance = std::nullopt};
}

std::optional<Flight> ZoomToPlaces::Act(const Press& press, const Altitude& altitude)
{
	if (press.tapAllowed && IsSingleTap(press.now))
	{
		// A single tap looks at the land ahead, from a pleasing distance and pitch. With a fight in view it does
		// nothing, but it still counts as a tap.
		std::optional<Flight> flight;
		if (press.clickDistance.has_value())
		{
			const auto ahead = AlongView(press.current, *press.clickDistance);
			flight =
			    Flight {.view = ViewOver({ahead.x, ahead.z}, k_TapDistance, HeadingOf(press.current), k_TapPitch, altitude),
			            .rise = k_TapRise,
			            .toPlace = false};
		}
		_state.lastTap = press.now;
		return flight;
	}

	std::optional<Flight> flight;
	if (_state.flownTo)
	{
		// The camera flies back if it still looks near the place
		const float acrossX = press.place.x - press.current.focus.x;
		const float acrossZ = press.place.y - press.current.focus.z;
		if (!(k_ReturnRadius * k_ReturnRadius < (acrossZ * acrossZ) + (acrossX * acrossX)))
		{
			flight = Flight {.view = _state.returnTo, .rise = k_PlaceRise, .toPlace = true};
		}
		_state.flownTo = false;
	}
	if (!flight.has_value())
	{
		_state.returnTo = press.current;
		flight = Flight {.view = ViewOver(press.place, press.distance, HeadingOf(press.current), press.pitch, altitude),
		                 .rise = k_PlaceRise,
		                 .toPlace = true};
		_state.flownTo = true;
	}
	_state.lastTap = 0;
	return flight;
}

} // namespace openblack::zoom_to
