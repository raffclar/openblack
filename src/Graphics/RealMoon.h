/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// EXPERIMENT (not for merge): a moon that looks like the real moon, for the water experiment. The original game draws
/// its moon as a large half sphere with a flat texture. This moon is a lit ball about half a degree across:
/// - a map of the real moon's surface, with the same face always to the eye, and its north to the north of the sky;
/// - the sun lights it from the true direction, so the phase and the line between day and night come from the sky;
/// - the light from the earth makes its dark side faintly visible;
/// - near the horizon the air makes it dimmer and redder, and a soft halo shows round it in haze.
/// The shaders vs_real_moon.sc and fs_real_moon.sc use the same rules; the tests check these.
///
/// The map is the colour map of the Lunar Reconnaissance Orbiter Camera from the NASA Scientific Visualization
/// Studio "CGI Moon Kit" (lroc_color_poles_1k), which is in the public domain. The repository keeps the source image
/// in assets/experiment/moon_lroc_color_poles_1k.jpg; the game reads a raw copy of it (k_MapPath).
namespace openblack::graphics::real_moon
{

/// The map of the moon's surface: 512 x 512 bytes of red, green and blue, the 2 to 1 map stretched to a square. The
/// left and right edges are 180 degrees west and east, the middle column is 0 degrees, and the top row is the north.
inline constexpr std::string_view k_MapPath = "E:/openblack/experiments/underwater/moon_albedo.raw";
inline constexpr entt::hashed_string k_MapId = entt::hashed_string("experiment/real_moon_map");

/// The moon's size across as the eye sees it, in degrees
inline constexpr float k_ApparentDegrees = 0.52f;
/// The size of the square that the shader draws, as a number of the moon's radius from its middle: the halo fills it
inline constexpr float k_QuadRadii = 12.0f;
/// The brightness of the map's light at the full moon, so that the middle of the full moon is near white
inline constexpr float k_Gain = 1.4f;
/// The light from the earth on the dark side at a new moon, as a share of the sun's light
inline constexpr float k_Earthshine = 0.035f;
/// How much of each colour the clear air takes out for each length of air straight up (red, green and blue)
inline constexpr glm::vec3 k_Extinction {0.10f, 0.17f, 0.32f};
/// The eye sees a light that is this power of its real brightness, so a low moon is dim but still visible
inline constexpr float k_EyeResponse = 0.4f;

/// The radius of the moon at a distance from the eye
[[nodiscard]] float Radius(float distance);

/// The axes of the moon's face as the eye sees it:
/// - `toEye` from the moon to the eye;
/// - `north` to the north of the sky across the face, so the moon's north is up for the real latitude;
/// - `east` across the face to the moon's own east, which the eye sees on the west side of the sky.
struct Face
{
	glm::vec3 east;
	glm::vec3 north;
	glm::vec3 toEye;
};
[[nodiscard]] Face FaceAxes(glm::vec3 towardsMoon);

/// The direction of the north pole of the sky, at the latitude of the world
[[nodiscard]] glm::vec3 SkyNorthPole();

/// The place on the map of a point of the moon's surface, from its direction from the moon's middle. The point that
/// faces the eye is the middle of the map.
[[nodiscard]] glm::vec2 MapPoint(glm::vec3 normal, const Face& face);

/// The light that the moon's surface sends back towards the eye, for a surface that the sun lights at the cosine
/// `towardsSun` and the eye sees at the cosine `towardsEye`. The moon's dust sends the light back almost the same at
/// all angles (Lommel and Seeliger), so the full moon looks flat and not like a ball. It is 1 at the full moon's
/// middle.
[[nodiscard]] float SurfaceLight(float towardsSun, float towardsEye);

/// The light of the earth on the moon's dark side, from the cosine of the angle between the moon and the sun as the eye
/// sees it: most at a new moon (the earth is full for the moon), none at a full moon
[[nodiscard]] float EarthLight(float sunMoonCosine);

/// The part of each colour of the moon's light that comes through the air, from the sine of its angle over the
/// horizon: all of it straight up, less and redder near the horizon. The brightness is as the eye sees it
/// (k_EyeResponse); the colour is the real one.
[[nodiscard]] glm::vec3 AirTint(float elevationSine);

/// The strength of the halo round the moon for a moon light of 1 straight up
inline constexpr float k_Halo = 0.08f;

/// The strength of the halo round the moon, from the sine of its angle over the horizon and the strength of its light:
/// the air near the horizon holds more haze, so the halo is stronger there
[[nodiscard]] float HaloStrength(float elevationSine, float moonLight);

/// How much the moon hides the sky behind it at full night: most of the stars. The faint glow of the air in front of
/// it stays.
inline constexpr float k_NightCover = 0.7f;

/// How much the moon hides the sky behind it, from the type of sky (0 night, 1 dusk, 2 day). By night it hides most
/// of the stars. From dusk on, the light of the sky is in front of the moon, so only the moon's light adds to the
/// sky.
[[nodiscard]] float Cover(float skyType);

} // namespace openblack::graphics::real_moon
