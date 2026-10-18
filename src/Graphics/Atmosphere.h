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

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

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

/// The radius of the ground and of the top of the air, in kilometres: the Earth's mean radius (a circumference of about
/// 40030 kilometres), and the top of the air at the line of space, 100 kilometres up. Over the air are the high layers
/// with no light of their own in this model, the aurora from about 100 to 300 kilometres, and space.
inline constexpr float k_GroundRadius = 6371.0f;
inline constexpr float k_TopRadius = 6471.0f;
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
/// The light that the air scatters more than once after sunset comes from the table of Hillaire (2020). The
/// table holds, for each height and each angle of the light, the light of all the scattering after the first. While the
/// light is over k_MultipleDayHigh (as a sine), the sky keeps the day's simple share above, and under k_MultipleDayLow
/// it has the table's light only. Thus the day's sky and the sunset do not change, and the low air in the earth's shadow
/// at dusk does not glow.
inline constexpr float k_MultipleDayLow = -0.1f;
inline constexpr float k_MultipleDayHigh = -0.02f;
/// The size of the table on the graphics card (across: the cosine of the light from -1 to 1; down: the height from 0 to
/// the top of the air), the directions round each point and the steps along each direction
inline constexpr uint16_t k_MultipleWidth = 32;
inline constexpr uint16_t k_MultipleHeight = 32;
inline constexpr int k_MultipleDirections = 64;
inline constexpr int k_MultipleSteps = 20;
/// The size of the same table that the processor makes for its own sky, and its directions and steps
inline constexpr uint16_t k_MultipleCpuWidth = 16;
inline constexpr uint16_t k_MultipleCpuHeight = 16;
inline constexpr int k_MultipleCpuDirections = 32;
inline constexpr int k_MultipleCpuSteps = 16;
/// How much of the light the ground under the air sends back: mostly sea, with some land
inline constexpr float k_GroundAlbedo = 0.1f;
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
struct MultipleTable;
struct Air
{
	float turbidity {1.0f};
	/// The table of the light that the air scatters more than once, made for this air. Without it the air has the day's
	/// simple share at all angles of the light.
	std::shared_ptr<const MultipleTable> multiple;
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

/// The table of the light that the air scatters more than once (Hillaire 2020), for each height and each angle of the
/// light. Each texel is the light, for each steradian, that reaches a point after two or more scatterings, for a light of
/// strength 1. It sums the second scattering from all directions round the point, the light from the ground included,
/// and then all the scatterings after it as a series.
struct MultipleTable
{
	uint16_t width {0};
	uint16_t height {0};
	std::vector<glm::vec3> texels;
	/// The light at a height in kilometres and the cosine of the light from straight up, between the texels
	[[nodiscard]] glm::vec3 At(float altitude, float lightMu) const;
};

/// The light of the table at one height and one cosine of the light, summed over `directions` directions with `steps`
/// steps each. `through` gives the part of each colour that gets through from a height to the top along a cosine.
using ThroughFunction = std::function<glm::vec3(float altitude, float mu)>;
[[nodiscard]] glm::vec3 MultipleScatteringAt(float altitude, float lightMu, const Air& air, int directions, int steps,
                                             const ThroughFunction& through);

/// The height in kilometres and the cosine of the light at the middle of a texel of the table
[[nodiscard]] HeightAngle FromMultipleTexel(int column, int row, uint16_t width, uint16_t height);

/// Makes the table for an air, with the size, directions and steps that the processor uses
[[nodiscard]] std::shared_ptr<const MultipleTable> MakeMultipleTable(const Air& air);

/// The light that the air scatters more than once at a height with the light at a cosine: the day's simple share while
/// the light is high, and the table's light while it is low (see k_MultipleDayLow)
[[nodiscard]] glm::vec3 MultipleScattering(float altitude, float lightMu, const Air& air);

/// The day's simple share of the light that the air scatters more than once
[[nodiscard]] glm::vec3 DayMultipleScattering(float altitude, float lightMu, const Air& air);

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

/// The mean of the sky's light round the horizon, for a light of strength 1. `horizonY` is the y of the direction to
/// the sea's horizon, which goes down under the eye's level as the eye climbs (WorldHorizonY).
[[nodiscard]] glm::vec3 HorizonRadiance(glm::vec3 towardsLight, float altitude, const Air& air, float horizonY = 0.0f);

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

/// The share of its colour that the sky keeps on the screen, from the sine of the air's sun over the horizon. By day
/// all of it. While the sun sets, the sky goes part of the way to grey. In the blue hour, from about 3 to 9 degrees
/// under the horizon, the sun lights only the high air through the ozone, and the sky keeps most of its deep blue. In the
/// moon's light at night the eye sees the dim sky with less colour again.
[[nodiscard]] float SkyColourKept(float sunHeight);
/// How much of the colour that the day keeps, the blue hour keeps
inline constexpr float k_BlueHourColour = 0.7f;

/// The sky's colour as the eye sees it while the sun is low: the colour goes to a grey of the same luminance by the
/// share that SkyColourKept does not keep. `sunHeight` is the sine of the air's sun over the horizon.
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

/// The world is a ball with the radius of the Earth: 6371 kilometres times 949 units a kilometre (underwater::
/// k_UnitsPerMetre, from the villager's 1.661 units for 1.75 metres) is about 6.05 million units. The sky's textures use
/// the same radius, so the far sea goes down under the horizon at the same angle as the ground in the sky's texture.
/// The air's layers and the aurora are at their real heights over it. From 50 units up the horizon is 0.2 degrees under
/// the eye's level, from 30000 units up it is about 6 degrees, and from space the planet is a ball against the dark.
inline constexpr float k_WorldRadius = k_GroundRadius * k_UnitsPerKilometre;

/// The flat cap of the world: a disc on the sea's level that holds the whole map, so the land, the sea bed and all the
/// things on the map stay where they are and the coast and the near game do not change. Past the cap the world is a
/// sphere of k_WorldRadius. Low down the cap is round the eye, with the radius to the map's farthest corner. Between
/// k_CapFollowsEyeBelow and k_CapOnMapAbove units over the sea the cap moves to the map's middle and gets smaller, so
/// that from space the sea is the same ball as the air's planet under the map's middle.
struct WorldCap
{
	/// The eye across the ground less the cap's middle
	glm::vec2 eyeFromMiddle {0.0f};
	/// The radius of the cap
	float radius {0.0f};
};
inline constexpr float k_CapFollowsEyeBelow = 20000.0f;
inline constexpr float k_CapOnMapAbove = 200000.0f;

/// The flat cap for an eye at `eye` across the ground and `heightOverSea` units over the sea, for a map that is
/// `mapSize` units on each side. The cap moves by the eye's height over the ball under the map's middle, so an eye far
/// round the planet (under the sea's level) is over the ball too.
[[nodiscard]] WorldCap MapCap(glm::vec2 eye, float heightOverSea, float mapSize);

/// The height of the eye over the ball under the map's middle, with no loss of the small terms
[[nodiscard]] float HeightOverMapBall(glm::vec3 eye, float seaLevel, float mapSize);

/// The world in the upright plane through the eye along a direction on the ground. The ground is flat along the cap's
/// chord, from `chordFrom` to `chordTo` units along the plane from the eye. Past each end of the chord it is a circle
/// that is a section of the world's ball. A plane that does not cross the cap has no chord: both ends are at the point
/// nearest to the cap, and `pastSquared` is the square of the plane's distance past the cap's edge.
struct WorldSection
{
	float chordFrom {0.0f};
	float chordTo {0.0f};
	float pastSquared {0.0f};
};
[[nodiscard]] WorldSection SectionToward(const WorldCap& cap, glm::vec2 across);

/// How far a point of the world goes down at `fromEye` across the ground from the eye: none on the cap, and past it
/// the drop of the world's ball
[[nodiscard]] float WorldDrop(const WorldCap& cap, glm::vec2 fromEye);

/// The distance along a ray from the eye, `heightOverSea` units over the sea's level, to the curved sea. None when the
/// ray goes over the horizon and does not meet the sea, or when the eye is under the curved sea.
[[nodiscard]] std::optional<float> CurvedSeaDistance(const WorldCap& cap, float heightOverSea, glm::vec3 direction);

/// The air's own frame for an eye: `up` is the line from the middle of the world's ball (under the flat cap's middle)
/// through the eye, and `height` is the eye's height over the ball in world units. Low down the cap is round the eye,
/// so the line is straight up and the height is the height over the sea. The sky's textures work in this frame, so from
/// space the air's planet is the same ball as the sea.
struct AirFrame
{
	glm::vec3 up {0.0f, 1.0f, 0.0f};
	float height {0.0f};
};
[[nodiscard]] AirFrame AirFrameFor(const WorldCap& cap, float heightOverSea);

/// A direction turned by the smallest turn that makes `up` point straight up
[[nodiscard]] glm::vec3 TurnToUp(glm::vec3 up, glm::vec3 direction);

/// The place from which the directions to the game's sun and moon are found. The game's sun is a few kilometres away,
/// so from high up or far out its direction would turn round. Thus the place is the eye with its height at most
/// k_LightEyeHeight over the sea and inside k_LightEyeReach of the map's middle.
inline constexpr float k_LightEyeHeight = 2000.0f;
inline constexpr float k_LightEyeReach = 20000.0f;
[[nodiscard]] glm::vec3 LightEye(glm::vec3 eye, float seaLevel);

/// The aurora's oval is round the magnetic pole. The pole is on the island's line of longitude at 72 degrees north, 32
/// degrees from the island at 40 degrees north, so the island is at a magnetic latitude of 58 degrees. On most nights
/// the oval is at about 64 to 67 degrees and its curtains show low in the north. On strong nights the oval comes down
/// over the island.
inline constexpr float k_PoleFromIslandDegrees = 32.0f;

/// The direction of the magnetic pole from the planet's middle, in the world's directions (up at the island is y)
[[nodiscard]] glm::vec3 MagneticPole();

/// The magnetic latitude in degrees of a place, from its direction from the planet's middle
[[nodiscard]] float MagneticLatitude(glm::vec3 up);

/// How strong the aurora is on a night, 0 to 1, from the night's number. Most nights are weak, and some are strong.
[[nodiscard]] float AuroraStrength(int night);

/// The steps of the aurora's march along a ray (air_sky.sh), and the fewer steps in the faces of the far cube, which the
/// water shows blurred
inline constexpr float k_AuroraSteps = 40.0f;
inline constexpr float k_AuroraCubeSteps = 16.0f;

/// A strong aurora: its oval comes down over the island
inline constexpr float k_StrongAurora = 0.9f;

/// How strong the aurora is tonight: the chosen strength (from 0 to 1) when there is one, else the night's own
[[nodiscard]] float AuroraTonight(int night, std::optional<float> chosen);
/// The noctilucent clouds: a thin sheet of ice at the top of the mesosphere, about 83 kilometres up. They form only in
/// the cold summer mesopause, mostly from late May to late August in the north. They are rare at the island's latitude
/// of 40 degrees: on the nights of their season a share of at most k_NoctilucentPeakChance, the most near the start of
/// July. The sheet shows only in deep twilight, when the sun under the horizon lights it and the low air is dark.
inline constexpr float k_NoctilucentHeight = 83.0f;
inline constexpr float k_NoctilucentPeakChance = 0.15f;
inline constexpr int k_NoctilucentSeasonStart = 135;
inline constexpr int k_NoctilucentSeasonEnd = 235;

/// The day of the year, 0 for the first of January, of a date in seconds from 1970 with `passedDays` days added
[[nodiscard]] int DayOfYear(int64_t unixTime, int32_t passedDays);

/// The share of the nights on a day of the year that have noctilucent clouds: none out of their season, and
/// k_NoctilucentPeakChance at the middle of it
[[nodiscard]] float NoctilucentChance(int dayOfYear);

/// How strong the noctilucent clouds are on a night, 0 to 1, from the night's own number (its days from 1970) and its
/// day of the year. Most nights have none. A night with clouds has a strength from 0.5 to 1.
[[nodiscard]] float NoctilucentStrength(int64_t night, int dayOfYear);

/// The y of the direction from the eye to the curved sea's horizon, from the eye's height over the sea's level: near 0
/// low down, and under 0 as the eye climbs. It is the horizon in the plane at a right angle to the line from the cap's
/// middle.
[[nodiscard]] float WorldHorizonY(const WorldCap& cap, float heightOverSea);

} // namespace openblack::graphics::atmosphere
