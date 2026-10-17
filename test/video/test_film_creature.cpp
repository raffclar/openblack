/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The falling spell film's creature: its clock, when it shows, its colours, the camera path and the camera

#include <cstdint>
#include <cstring>

#include <array>
#include <vector>

#include <CM2File.h>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/FilmCameraPath.h"
#include "Creature/CreatureLayers.h"
#include "Video/FilmCreature.h"

using namespace openblack;

namespace
{
/// A point whose camera looks along +z from a position, with its axes right, up and back as given
cam::CM2Point PointAt(std::array<float, 3> position, std::array<float, 9> axes)
{
	cam::CM2Point point {};
	point.position = position;
	point.focus = {position[0], position[1], position[2] + 1.0f};
	for (size_t i = 0; i < axes.size(); ++i)
	{
		point.frame.at(i) = axes.at(i);
	}
	point.frame[9] = position[0];
	point.frame[10] = position[1];
	point.frame[11] = position[2];
	return point;
}

constexpr std::array<float, 9> k_LooksAlongZ {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f};

/// The bytes of a .cm2 file with these points over a duration
std::vector<uint8_t> Bytes(uint32_t duration, const std::vector<cam::CM2Point>& points)
{
	std::vector<uint8_t> bytes(12 + points.size() * sizeof(cam::CM2Point));
	const std::array<uint32_t, 3> header {static_cast<uint32_t>(bytes.size()), duration, static_cast<uint32_t>(points.size())};
	std::memcpy(bytes.data(), header.data(), sizeof(header));
	std::memcpy(bytes.data() + 12, points.data(), points.size() * sizeof(cam::CM2Point));
	return bytes;
}
} // namespace

TEST(FilmCreature, ThePoseCountsFromTheStartOfTheActionAndOnlyMovesOn)
{
	film_creature::Clock clock;
	// The film's first frame takes the time back to the start of the film
	EXPECT_EQ(film_creature::Step(clock, 0), std::nullopt);
	EXPECT_EQ(film_creature::Step(clock, 41), 41);
	EXPECT_EQ(film_creature::Step(clock, 83), 42);
	EXPECT_EQ(film_creature::Step(clock, 83), std::nullopt);
	EXPECT_EQ(film_creature::Step(clock, 125), 42);

	// A first frame later than 100 ms into the film counts from 100 ms
	film_creature::Clock fresh;
	EXPECT_EQ(film_creature::Step(fresh, 125), 25);
}

TEST(FilmCreature, ItIsNotShownInTheMiddleOfTheFilm)
{
	EXPECT_TRUE(film_creature::Shown(0));
	EXPECT_TRUE(film_creature::Shown(19549));
	EXPECT_FALSE(film_creature::Shown(19550));
	EXPECT_FALSE(film_creature::Shown(27350));
	EXPECT_TRUE(film_creature::Shown(27351));
}

TEST(FilmCreature, ItsBrightnessRisesTwiceThenStaysAtNothing)
{
	EXPECT_EQ(film_creature::Brightness(0), 0);
	EXPECT_EQ(film_creature::Brightness(76), 0);
	EXPECT_EQ(film_creature::Brightness(77), 1);
	EXPECT_EQ(film_creature::Brightness(9725), 127);
	EXPECT_EQ(film_creature::Brightness(19449), 254);
	EXPECT_EQ(film_creature::Brightness(19450), 0);
	EXPECT_EQ(film_creature::Brightness(19500), 1);
	EXPECT_EQ(film_creature::Brightness(27349), 254);
	EXPECT_EQ(film_creature::Brightness(27350), 0);
	EXPECT_EQ(film_creature::Brightness(40000), 0);
}

TEST(FilmCreature, ItGlowsWhiteThenOrangeOverAFlickeringDarkRed)
{
	// No brightness: black, with white added
	const auto dark = film_creature::LookAt(0, 31, 7, 3);
	EXPECT_EQ(dark.colour, 0xFF000000u);
	EXPECT_EQ(dark.added, 0xFFFFFFFFu);
	EXPECT_EQ(film_creature::LookAt(1, 0, 0, 0).added, 0xFFFEFEFEu);
	// Half way: each channel of the flicker scaled by the brightness of 256
	const auto half = film_creature::LookAt(128, 31, 7, 3);
	EXPECT_EQ(half.colour, 0xFF1F0701u);
	EXPECT_EQ(half.added, 0xFFBF9F7Fu);
	const auto other = film_creature::LookAt(200, 31, 7, 3);
	EXPECT_EQ(other.colour, 0xFF310B02u);
	EXPECT_EQ(other.added, 0xFF9B6937u);
	// Full brightness: dark red, with orange added
	const auto full = film_creature::LookAt(255, 0, 0, 0);
	EXPECT_EQ(full.colour, 0xFF1F0700u);
	EXPECT_EQ(full.added, 0xFF804000u);
	EXPECT_EQ(film_creature::LookAt(255, 31, 7, 3).colour, 0xFF3E0E02u);
}

TEST(FilmCreature, TheCameraPathFileHasEighteenFloatsAPoint)
{
	const auto bytes = Bytes(1000, {PointAt({1.0f, 2.0f, 3.0f}, k_LooksAlongZ), PointAt({4.0f, 5.0f, 6.0f}, k_LooksAlongZ)});
	const auto path = cam::ReadCM2(bytes);
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(path->durationMilliseconds, 1000u);
	ASSERT_EQ(path->points.size(), 2u);
	EXPECT_EQ(path->points[1].position, (std::array {4.0f, 5.0f, 6.0f}));
	EXPECT_EQ(path->points[1].frame[8], -1.0f);
	EXPECT_EQ(path->points[1].frame[11], 6.0f);

	// Too short for the points it counts, or with none
	auto cut = bytes;
	cut.pop_back();
	EXPECT_FALSE(cam::ReadCM2(cut).has_value());
	EXPECT_FALSE(cam::ReadCM2(Bytes(1000, {})).has_value());
}

TEST(FilmCreature, TheCameraPathBlendsTheTwoPointsAboutATime)
{
	const auto file =
	    cam::ReadCM2(Bytes(1000, {PointAt({0.0f, 0.0f, 0.0f}, k_LooksAlongZ), PointAt({10.0f, 0.0f, 0.0f}, k_LooksAlongZ),
	                              PointAt({10.0f, 20.0f, 0.0f}, k_LooksAlongZ)}));
	ASSERT_TRUE(file.has_value());
	const FilmCameraPath path(*file);
	EXPECT_EQ(path.SampleAt(0).position, glm::vec3(0.0f));
	EXPECT_EQ(path.SampleAt(250).position, glm::vec3(5.0f, 0.0f, 0.0f));
	EXPECT_EQ(path.SampleAt(500).position, glm::vec3(10.0f, 0.0f, 0.0f));
	EXPECT_EQ(path.SampleAt(750).position, glm::vec3(10.0f, 10.0f, 0.0f));
	EXPECT_EQ(path.SampleAt(750).frame[10], 10.0f);
	// Past the end it takes the last millisecond
	EXPECT_EQ(path.SampleAt(5000).position, path.SampleAt(999).position);
}

TEST(FilmCreature, TheCameraLooksAlongItsPathsAxesFromItsScaledPosition)
{
	const auto point = FilmCameraPath::Point {.position = {10.0f, 20.0f, -5.0f},
	                                          .focus = {10.0f, 20.0f, -4.0f},
	                                          .frame = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f}};
	const auto view = film_creature::View(point);
	const auto expected =
	    glm::lookAtLH(glm::vec3(8.0f, 16.0f, -4.0f), glm::vec3(8.0f, 16.0f, -3.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			EXPECT_NEAR(view[i][j], expected[i][j], 2e-4f) << i << ", " << j;
		}
	}

	// The camera turned half round about the vertical, looking back along -z
	const auto turned = film_creature::View(FilmCameraPath::Point {
	    .position = {0.0f, 0.0f, 10.0f}, .focus = {}, .frame = {-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}});
	const auto origin = turned * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	EXPECT_NEAR(origin.x, 0.0f, 1e-5f);
	EXPECT_NEAR(origin.z, 8.0f, 1e-4f);
}

TEST(FilmCreature, TheCameraKeepsItsAxesUnitLong)
{
	// A blend of two frames is shorter than unit: the view's rows are made unit long again
	const auto view = film_creature::View(
	    FilmCameraPath::Point {.position = {}, .focus = {}, .frame = {0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, -0.5f}});
	EXPECT_NEAR(view[0][0], 1.0f, 1e-3f);
	EXPECT_NEAR(view[1][1], 1.0f, 1e-3f);
	EXPECT_NEAR(view[2][2], 1.0f, 1e-3f);
}

TEST(FilmCreature, TheCameraSeesAQuarterTurnAcrossAFourByThreeScreen)
{
	const auto world = glm::perspectiveLH_ZO(1.0f, 2.0f, 1.0f, 1000.0f);
	const auto projection = film_creature::Projection(world);
	EXPECT_FLOAT_EQ(projection[0][0], 2.4142137f);
	EXPECT_FLOAT_EQ(projection[1][1], 2.4142137f * 4.0f / 3.0f);
	// The depth stays the world camera's
	EXPECT_EQ(projection[2], world[2]);
	EXPECT_EQ(projection[3], world[3]);
}

TEST(FilmCreature, ABodyInRealTimeIgnoresTheCreaturesPace)
{
	auto body = creature_layers::PlayOnce({}, film_creature::k_Action, false);
	ASSERT_TRUE(body.has_value());
	body->realTime = true;
	const auto later = creature_layers::AdvanceBody(*body, 160.0f, 10000, 100.0f);
	EXPECT_EQ(later.timeMs, 100.0f);
	body->realTime = false;
	EXPECT_EQ(creature_layers::AdvanceBody(*body, 160.0f, 10000, 100.0f).timeMs, 160.0f);
}
