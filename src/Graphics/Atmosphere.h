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

#include <span>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// EXPERIMENT (not for merge): the rules of the air's light, for the haze over the land and the sea. The original game
/// has a haze by distance only. This is a model of the real air (Rayleigh scattering by the air's molecules, Mie
/// scattering by its dust and salt, and the ozone's absorption), as in the sky models of Bruneton and Hillaire:
/// - the light of the sun or the moon goes through the air to each point and loses some of each colour on the way, so
///   it is warm and dim low down at dawn and dusk;
/// - each point of the air scatters some of that light towards the eye: blue more than red, and the dust most of all
///   towards the light;
/// - the sky is that light along the whole ray, and the haze over far land is the same light over the distance to it.
/// The shaders in atmosphere.sh use the same formulas; the tests check these. Distances are in kilometres, and the
/// planet is the real one.
namespace openblack::graphics::atmosphere
{

/// The radius of the ground and of the top of the air, in kilometres
inline constexpr float k_GroundRadius = 6360.0f;
inline constexpr float k_TopRadius = 6460.0f;
/// The air's molecules: what they scatter of each colour for each kilometre at the ground, and the height over which
/// they thin by e
inline constexpr glm::vec3 k_RayleighScattering {5.802e-3f, 13.558e-3f, 33.1e-3f};
inline constexpr float k_RayleighHeight = 8.0f;
/// The dust and salt in the air: what it scatters and takes out for each kilometre at the ground, the height over
/// which it thins by e, and how much of its light goes on forward
inline constexpr float k_MieScattering = 3.996e-3f;
inline constexpr float k_MieExtinction = 4.440e-3f;
inline constexpr float k_MieHeight = 1.2f;
inline constexpr float k_MieForward = 0.8f;
/// The ozone: what it takes out of each colour for each kilometre at its densest, the height of its densest part and
/// half the thickness of its layer
inline constexpr glm::vec3 k_OzoneAbsorption {0.650e-3f, 1.881e-3f, 0.085e-3f};
inline constexpr float k_OzoneMiddle = 25.0f;
inline constexpr float k_OzoneHalfWidth = 15.0f;
/// The share of the light that the air scatters more than once, as a part of the single scattering's light that
/// reaches a point. The sky's light in the shade and at dusk comes mostly from it.
inline constexpr float k_MultipleShare = 0.3f;
/// The haze over the land is this much denser than the real air at the same height. The far islands are a few
/// kilometres away, and the real air needs some tens of kilometres to fade a hill this much.
inline constexpr float k_HazeScale = 7.0f;
/// The extra haze is in the low air: over this height in kilometres it thins to a third, and high up the air is the real
/// air. The horizon from the land keeps its haze, and the sea from high up is clear through the thin air.
inline constexpr float k_HazeLayerHeight = 2.5f;
/// The world units in one kilometre (underwater::k_UnitsPerMetre, 0.949 units a metre)
inline constexpr float k_UnitsPerKilometre = 949.0f;

/// The textures of the air's light: the light that gets through the air to the top for each height and angle, and the
/// sky round the eye for each direction
inline constexpr uint16_t k_TransmittanceWidth = 256;
inline constexpr uint16_t k_TransmittanceHeight = 64;
inline constexpr uint16_t k_SkyViewWidth = 192;
inline constexpr uint16_t k_SkyViewHeight = 108;

/// The haze's grid that follows the view: cells across, cells down and slices into the view. Each slice lies in a
/// tile of one texture, k_AerialTilesAcross tiles across.
inline constexpr uint16_t k_AerialCells = 32;
inline constexpr uint16_t k_AerialSlices = 32;
inline constexpr uint16_t k_AerialTilesAcross = 8;
/// The nearest and the farthest distance that the haze's grid covers, in world units. The far end is past the
/// horizon from the highest view: the sea far out is in the haze too.
inline constexpr float k_AerialNearest = 8.0f;
inline constexpr float k_AerialFarthest = 24000.0f;

/// How much dust and salt the air holds, as a multiple of the clean air's: more over the sea, and more in rain and in
/// a storm
struct Air
{
	float turbidity {1.0f};
};

/// The air for how stormy the weather is (0 to 1) and how wet the land is (0 to 1)
[[nodiscard]] Air AirFor(float storm, float wetness);

/// What the air at a height scatters and takes out for each kilometre
struct Medium
{
	glm::vec3 rayleigh;
	float mie;
	glm::vec3 extinction;
};

/// The air at a height over the ground, in kilometres
[[nodiscard]] Medium MediumAt(float altitude, const Air& air);

/// The share of the molecules' light that goes at an angle to the light, for each steradian
[[nodiscard]] float RayleighPhase(float cosAngle);
/// The share of the dust's light that goes at an angle to the light, for each steradian (Cornette and Shanks)
[[nodiscard]] float MiePhase(float cosAngle, float forward);

/// How far a ray from a radius goes up before it leaves the top of the air; mu is the cosine of its angle from
/// straight up
[[nodiscard]] float DistanceToTop(float radius, float mu);
/// Whether a ray from a radius hits the ground
[[nodiscard]] bool HitsGround(float radius, float mu);

/// The part of each colour that gets through the air from a height to the top, along a ray at mu from straight up;
/// none where the ray hits the ground
[[nodiscard]] glm::vec3 Transmittance(float altitude, float mu, const Air& air);

/// Where a height and an angle are in the texture of the light that gets through (Bruneton's mapping), 0 to 1
[[nodiscard]] glm::vec2 TransmittanceUv(float altitude, float mu);
/// The height and the angle at a point of that texture
struct HeightAngle
{
	float altitude;
	float mu;
};
[[nodiscard]] HeightAngle FromTransmittanceUv(glm::vec2 uv);

/// Where a direction is in the texture of the sky round the eye (Hillaire's mapping), 0 to 1. viewMu is the cosine of
/// the direction's angle from straight up; lightCos is the cosine of the angle round the horizon between it and the
/// light. The horizon has more of the texture than the zenith.
[[nodiscard]] glm::vec2 SkyViewUv(float viewMu, float lightCos, float altitude);
/// The direction at a point of that texture: x the cosine from straight up, y the cosine round from the light
[[nodiscard]] glm::vec2 FromSkyViewUv(glm::vec2 uv, float altitude);

/// The distance of the middle of a slice of the haze's grid from the eye, in world units (farther apart far away)
[[nodiscard]] float AerialSliceDistance(float slice);
/// The slice at a distance from the eye, as a fraction
[[nodiscard]] float AerialSliceAt(float distance);

/// The sky's light from a direction, for a light of strength 1 from the direction `towardsLight`, seen from a height:
/// the light that the air scatters once and more than once along the ray
[[nodiscard]] glm::vec3 SkyRadiance(glm::vec3 view, glm::vec3 towardsLight, float altitude, const Air& air);

/// The haze over a distance along a ray from the eye: the light that the air adds and the part of the scene behind that
/// shows through
struct Haze
{
	glm::vec3 light;
	glm::vec3 through;
};
/// How much denser the haze is than the real air at a height in kilometres: k_HazeScale at the sea's level, near 1 high up
[[nodiscard]] float HazeScaleAt(float altitude);

/// The haze along a ray from a height over `distance` world units, at HazeScaleAt times the real air's density
[[nodiscard]] Haze HazeAlong(glm::vec3 view, glm::vec3 towardsLight, float altitude, float distance, const Air& air);

/// The light of the sun or the moon at the ground, as a part of its light at the top of the air, for the sine of its
/// height over the horizon. It is warm and dim low down, and none under the horizon.
[[nodiscard]] glm::vec3 LightAtGround(float elevationSine, const Air& air);

/// The mean of the sky's light round the horizon, for a light of strength 1
[[nodiscard]] glm::vec3 HorizonRadiance(glm::vec3 towardsLight, float altitude, const Air& air);

/// What the air's light is scaled by so that the horizon is as bright as `target` (the original game's haze colour
/// at this time of day): the ratio of their luminances
[[nodiscard]] float Exposure(glm::vec3 horizon, glm::vec3 target);

/// The share of the original game's haze colour that the horizon keeps, so that the sky's mood for each alignment and
/// time of day still shows
inline constexpr float k_OriginalHue = 0.35f;

/// What the air's light (for a light of strength 1) is scaled by on the screen, for each colour: the light's tint and
/// the exposure that makes the horizon as bright as `target`, with `originalShare` of the target's own colour
[[nodiscard]] glm::vec3 ScreenScale(glm::vec3 horizon, glm::vec3 target, glm::vec3 tint, float originalShare);

/// A colour's luminance
[[nodiscard]] float Luminance(glm::vec3 colour);

/// The sky's colour as the eye sees it while the sun is low: after sunset the air gives a deep, strong blue, but the eye
/// sees a dim sky with less colour. The colour goes part of the way to a grey of the same luminance, more while the
/// air's sun is low. `sunHeight` is the sine of the air's sun over the horizon.
[[nodiscard]] glm::vec3 DimSkyColour(glm::vec3 colour, float sunHeight);

/// A colour of the air's light, which the model gives in linear light, as the screen shows it. The game draws in the
/// colours of the screen, so a low sun's linear deep red shows as the orange that the eye sees.
[[nodiscard]] glm::vec3 OnScreen(glm::vec3 linear);

/// The light of the sky from above on the clouds. By day it is the original game's sky colour. While the air's sun is
/// low, that colour is the red of the horizon, so the light takes the colour of the air's upper sky (`airAbove`) at the
/// same luminance. `sunHeight` is the sine of the air's sun over the horizon.
[[nodiscard]] glm::vec3 CloudSkyLight(glm::vec3 original, glm::vec3 airAbove, float sunHeight);

/// The sun's real path over the island. The model puts the island at a latitude of 40 degrees north on the day of an
/// equinox, with noon at 12 of script time. The sun comes up in the east at 6, climbs to 50 degrees at noon in the south
/// and goes down in the west at 18. The original game keeps its sun low in the north west; the noon sun of this model
/// stands in the same direction across the land.
inline constexpr float k_LatitudeDegrees = 40.0f;
/// The angle of the sun north of the equator, in degrees: 0 on the day of an equinox
inline constexpr float k_DeclinationDegrees = 0.0f;
/// The directions of the world: north points away from the noon sun, and east is on the right of north
inline constexpr glm::vec3 k_North {0.70710678f, 0.0f, 0.70710678f};
inline constexpr glm::vec3 k_East {0.70710678f, 0.0f, -0.70710678f};
/// The distance of the sun from the middle of the world, as the original game's sun
inline constexpr float k_SunDistance = 42426.4f;
/// The distance of the moon from the eye, as the original game's moon at midnight
inline constexpr float k_MoonDistance = 4100.0f;

/// The direction from the ground towards the sun at an hour of script time (0 to 24), from the latitude, the
/// declination and the hour angle (15 degrees for each hour from noon)
[[nodiscard]] glm::vec3 SunTowards(float hour);

/// How strongly the sun shows, 0 to 255, from the sine of its angle over the horizon: none 1 degree under the horizon,
/// all at 4 degrees over it
[[nodiscard]] float SunStrength(float elevationSine);

/// The share of the moon in the air's light, from the sine of the sun's angle over the horizon: none until the sun is 2
/// degrees under the horizon, all when the sun is 8 degrees under it
[[nodiscard]] float AirMoonShare(float elevationSine);

/// The type of sky (0 night, 1 dusk, 2 day) from the sine of the sun's angle over the horizon, as the light of a real
/// sky changes:
/// - night when the sun is 18 degrees or more under the horizon, then towards dusk as the sun comes up to the horizon;
/// - dusk while the sun is up to 5 degrees over the horizon;
/// - towards day from 5 to 10 degrees, and day over 10 degrees.
[[nodiscard]] float SkyTypeFromSun(float elevationSine);

/// The direction of the air's light between the sun and the moon, which stand on the two sides of the sky. The angle
/// round the horizon and the angle over it go from one to the other in a line, so the light moves through the south.
/// On the way the light goes down to 10 degrees under the horizon, so the sky shows only a soft glow while it moves.
/// It does not go across the top of the sky.
[[nodiscard]] glm::vec3 AirLightTowards(glm::vec3 sun, glm::vec3 moon, float moonShare);

/// The angle between the earth's axis and the axis of its path round the sun, in degrees. The moon goes round the sky
/// near the path of the sun, so it climbs higher or lower than the sun.
inline constexpr float k_AxisTiltDegrees = 23.44f;

/// How far through its moon month the moon is, 0 at a new moon and 0.5 at a full moon. It counts the whole days of
/// the date as the game's moon does, then adds the days that passed in the game and the part of the day of the hour.
/// Thus the moon moves smoothly through the night and from one day of the game to the next.
[[nodiscard]] double MoonMonthFraction(int64_t unixTime, int32_t passedDays, float hour);

/// The phase of the moon from how far through its moon month it is, as the game's moon has it: a whole turn (2 pi) at
/// a new moon and half a turn at a full moon
[[nodiscard]] float MoonPhase(double monthFraction);

/// The direction from the ground towards the moon at an hour of script time (0 to 24), on the day of the equinox. The
/// moon is east of the sun by the part of its moon month that passed: next to the sun at a new moon, 90 degrees
/// away at a quarter and opposite at a full moon. It goes along the sun's yearly path, so its angle north of the
/// equator changes with the phase. Each day it is 12 degrees further east, so it rises about 50 minutes later.
[[nodiscard]] glm::vec3 MoonTowards(float hour, double monthFraction);

/// The part of the moon's light that comes through the air, from the sine of its angle over the horizon: all of it
/// straight up and less near the horizon, as the light goes through more air there. None under the horizon.
[[nodiscard]] float MoonThroughAir(float elevationSine);

/// The most light that the night sky (the stars and the glow of the air) gives the water and the land when the moon is
/// down, as a share of the light of a full moon high in the sky
inline constexpr float k_NightGlow = 0.12f;

/// The light of the night sky from the sine of the sun's angle over the horizon: none by day, then up to k_NightGlow
/// as the moon takes the air's light after sunset (AirMoonShare)
[[nodiscard]] float NightGlow(float sunElevationSine);

/// The direction that the air's light at night comes from. It is the moon while the moon is up. While the moon is
/// down, the direction goes to the point opposite the sun, so the night sky keeps its soft glow.
[[nodiscard]] glm::vec3 AirMoonTowards(glm::vec3 sunTowards, glm::vec3 moonTowards);

/// Turns the moon's axes about the line from the eye, so that the lit edge of the moon points to the sun across the
/// sky. The first axis goes across the moon's face and the third axis goes along the line from the eye. The axes keep
/// their lengths and their order. When the sun is in line with the moon, the axes do not change.
[[nodiscard]] glm::mat3 TurnLitEdgeTowards(const glm::mat3& basis, glm::vec3 towardsSun);

/// The number of days that start between two hours of script time from one frame to the next: 1 when the hour goes
/// past midnight forward, -1 when it goes past midnight back, otherwise 0. A negative earlier hour means none yet.
[[nodiscard]] int32_t DaysStarted(float earlierHour, float hour);

/// The share of a place's own colour that its haze and fog take
inline constexpr float k_PlaceShare = 0.3f;

/// The tint of the haze and the fog at a place, from the colours of the land round it (0 to 1): their mean colour at a
/// luminance of 1, with `share` of it kept and the rest white. Green fields give a green-grey haze, sand a warm one.
[[nodiscard]] glm::vec3 PlaceTint(std::span<const glm::vec3> colours, float share);

/// The height of the eye over the sea, in kilometres, from its height in world units over the sea's level
[[nodiscard]] float AltitudeOf(float heightOverSea);

} // namespace openblack::graphics::atmosphere
