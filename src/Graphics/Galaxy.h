/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// EXPERIMENT (not for merge): the rules of the Milky Way and the faint nebulae on the sky's sphere. The shader
// assets/shaders/galaxy.sh uses the same formulas. Directions on the sky's sphere use the frame of
// atmosphere::CelestialFrame: x towards a right ascension of 0, y towards 6 hours and z towards the north pole of the sky.

namespace openblack::graphics::galaxy
{

/// The turn from the sky's frame (J2000) to the galaxy's frame. The rows are the galaxy's axes in the sky's frame: x
/// towards the middle of the galaxy (right ascension 266.405, declination -28.936 degrees), y towards a galactic
/// longitude of 90 degrees and z towards the north pole of the galaxy (right ascension 192.859508, declination
/// 27.128336 degrees).
inline constexpr glm::mat3 k_SkyToGalaxy {-0.0548755604f, 0.4941094279f,  -0.8676661490f, //
                                          -0.8734370902f, -0.4448296300f, -0.1980763734f, //
                                          -0.4838350155f, 0.7469822445f,  0.4559837762f};

/// The luminance of the sky behind, on the screen, at and under which the Milky Way shows at its full strength. The
/// moonless sky of the night is darker than this.
inline constexpr float k_FullVisibilityLuminance = 0.025f;
/// The luminance of the sky behind, on the screen, at and over which the Milky Way does not show. The low
/// glow of the horizon and the twilight are brighter.
inline constexpr float k_NoVisibilityLuminance = 0.1f;

/// A direction on the sky's sphere in the galaxy's frame
[[nodiscard]] glm::vec3 GalacticDirection(glm::vec3 celestial);

/// The galactic longitude, from -180 to 180, and the galactic latitude, from -90 to 90, of a direction in the
/// galaxy's frame, in degrees
[[nodiscard]] glm::vec2 LongitudeLatitude(glm::vec3 galactic);

/// The smooth brightness of the stars of the galaxy at a longitude and latitude in degrees, before the dust: the disc
/// and the bulge round its middle. It is about 2 at the middle of the galaxy, near 0.2 on the band opposite the middle
/// and near 0 far from the band.
[[nodiscard]] float BandBrightness(glm::vec2 longitudeLatitude);

/// How much light the dark dust clouds stop at a longitude and latitude in degrees, as an optical depth: the thin lane
/// along the band, the Great Rift from Cygnus to Sagittarius, the Pipe, the clouds of Rho Ophiuchi, the Coalsack and
/// the Northern Coalsack. The shader adds a clumpy noise to it.
[[nodiscard]] float DustDepth(glm::vec2 longitudeLatitude);

/// The density of the faint stars in a direction on the sky's sphere, compared with the density far from the band:
/// about 1 off the band and up to about 3 to 4 on its bright star clouds
[[nodiscard]] float StarDensity(glm::vec3 celestial);

/// How much of the Milky Way and the nebulae shows in front of a sky of a luminance on the screen, 0 to 1. The light of
/// the twilight covers them.
[[nodiscard]] float Visibility(float skyLuminance);

/// How much of the Milky Way a full moon high in the sky covers
inline constexpr float k_MoonWash = 0.85f;

/// How much of the Milky Way and the nebulae the moon's light leaves, 0 to 1, from the moon's brightness by its phase (0
/// at a new moon, 1 at a full moon) and the sine of its height over the horizon. The moon's light on the air makes the
/// whole sky brighter, so it covers the faint band.
[[nodiscard]] float MoonVisibility(float moonBrightness, float moonSine);

} // namespace openblack::graphics::galaxy
