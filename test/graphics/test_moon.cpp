/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Graphics/Moon.h"

namespace moon = openblack::graphics::moon;

namespace
{
/// 6 January 2000, a new moon, in days since 1970
constexpr int64_t k_NewMoonDay = 0x2AD2;
} // namespace

TEST(Moon, ShowsAroundMidnight)
{
	const auto midnight = moon::Place(0.0f);
	ASSERT_TRUE(midnight.has_value());
	EXPECT_FLOAT_EQ(midnight->offset.x, 4000.0f);
	EXPECT_FLOAT_EQ(midnight->offset.y, 950.0f);
	EXPECT_NEAR(midnight->offset.z, 0.0f, 1e-3f);
	EXPECT_FLOAT_EQ(midnight->alpha, 200.0f);

	EXPECT_FALSE(moon::Place(6.0f).has_value());
	EXPECT_FALSE(moon::Place(12.0f).has_value());
	EXPECT_TRUE(moon::Place(23.0f).has_value());
}

TEST(Moon, PhaseFollowsTheRealMoon)
{
	// A whole turn at a new moon, then half way through a moon month of about 29.5 days
	EXPECT_NEAR(moon::Phase(k_NewMoonDay * 86400), 6.2831855f, 1e-5f);
	EXPECT_NEAR(moon::Phase((k_NewMoonDay + 15) * 86400 + 3600), (1.0f - (15.0f * 0.03386318f)) * 6.2831855f, 1e-4f);
}

TEST(Moon, FacesTheCamera)
{
	const glm::vec3 eye {0.0f, 100.0f, 0.0f};
	const auto view = glm::lookAt(eye, glm::vec3(4000.0f, 1000.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::vec3 position = eye + glm::vec3(4000.0f, 950.0f, 0.0f);
	const auto basis = moon::Basis(view, glm::inverse(view), position);
	// Four times the mesh's size, its axes square to each other, its third along the line from the camera
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_NEAR(glm::length(basis[i]), 4.0f, 1e-4f);
	}
	EXPECT_NEAR(glm::dot(basis[0], basis[1]), 0.0f, 1e-3f);
	EXPECT_NEAR(glm::dot(glm::normalize(basis[2]), glm::normalize(position - eye)), 1.0f, 1e-4f);

	const auto model = moon::Model(basis, position, 0.0f);
	EXPECT_NEAR(glm::length(glm::vec3(model[0])), 4.0f * 0.65f, 1e-4f);
	EXPECT_EQ(glm::vec3(model[3]), position);
}

TEST(Moon, GlowIsASquareAboutTheMoon)
{
	const glm::mat3 basis(4.0f);
	const auto glow = moon::MakeGlow(basis, glm::vec3(0.0f));
	EXPECT_EQ(glow.corners[0], glm::vec3(-2000.0f, -2000.0f, 0.0f));
	EXPECT_EQ(glow.corners[3], glm::vec3(2000.0f, 2000.0f, 0.0f));
	EXPECT_EQ(moon::GlowColour(glm::vec3(0.6f, 0.5f, 0.4f)), glm::vec3(0.1f, 0.1f, 0.1f));
}

TEST(Moon, TheSeaShowsTheGlowOfACopyMirroredThroughSeaLevel)
{
	const glm::vec3 eye {0.0f, 100.0f, 0.0f};
	const auto view = glm::lookAt(eye, glm::vec3(4000.0f, 1000.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const auto inverseView = glm::inverse(view);
	const glm::vec3 position = eye + glm::vec3(4000.0f, 950.0f, 0.0f);
	const auto sea = moon::SeaGlow(view, inverseView, position);

	// Mirrored back through sea level, the corners are the glow of a copy below the sea, 1050 down, facing the camera
	const glm::vec3 copy {position.x, -position.y, position.z};
	const auto expected = moon::MakeGlow(moon::Basis(view, inverseView, copy), copy);
	for (size_t i = 0; i < sea.corners.size(); ++i)
	{
		const glm::vec3 mirrored {sea.corners.at(i).x, -sea.corners.at(i).y, sea.corners.at(i).z};
		EXPECT_EQ(mirrored, expected.corners.at(i));
		EXPECT_EQ(sea.uvs.at(i), expected.uvs.at(i));
	}
	// The copy's glow faces the camera from below: square to the line from the camera to the copy
	const auto across = expected.corners[1] - expected.corners[0];
	const auto up = expected.corners[2] - expected.corners[0];
	const auto towardsCopy = glm::normalize(copy - eye);
	EXPECT_NEAR(glm::dot(glm::normalize(across), towardsCopy), 0.0f, 1e-4f);
	EXPECT_NEAR(glm::dot(glm::normalize(up), towardsCopy), 0.0f, 1e-4f);
	// It is not merely the sky's glow mirrored, which would face a camera mirrored through the sea
	const auto skyGlow = moon::MakeGlow(moon::Basis(view, inverseView, position), position);
	EXPECT_NE(sea.corners[2], skyGlow.corners[2]);
}

TEST(Moon, ScriptsAreToldHowNewTheMoonIs)
{
	// 1 at either end of the moon month, 0 half way through it
	EXPECT_FLOAT_EQ(moon::ScriptPercentage(0.0f), 1.0f);
	EXPECT_NEAR(moon::ScriptPercentage(3.14159274f), 0.0f, 1e-6f);
	EXPECT_NEAR(moon::ScriptPercentage(6.2831855f), 1.0f, 1e-6f);
	EXPECT_NEAR(moon::ScriptPercentage(1.5707964f), 0.5f, 1e-6f);
	EXPECT_NEAR(moon::ScriptPercentage(4.712389f), 0.5f, 1e-6f);
}

TEST(Moon, MonthFractionCountsWholeDays)
{
	EXPECT_DOUBLE_EQ(moon::MonthFraction(k_NewMoonDay * 86400), 0.0);
	// Any time of the same day gives the same fraction
	EXPECT_DOUBLE_EQ(moon::MonthFraction((k_NewMoonDay + 10) * 86400),
	                 moon::MonthFraction((k_NewMoonDay + 10) * 86400 + 86399));
	EXPECT_NEAR(moon::MonthFraction((k_NewMoonDay + 10) * 86400), 10.0 * 0.03386318012808897, 1e-12);
	// The phase is made from the fraction at full precision: many months on, it is still a whole day's step
	const int64_t later = (k_NewMoonDay + 9773) * 86400;
	const double fraction = moon::MonthFraction(later);
	EXPECT_NEAR(moon::Phase(later), static_cast<float>((1.0 - fraction) * 6.2831854820251465), 1e-6f);
}

TEST(Moon, DateAtFractionFindsTheNearestDay)
{
	const int64_t start = (k_NewMoonDay + 100) * 86400;
	for (const double fraction : {0.0, 0.25, 0.5, 0.75})
	{
		const auto date = moon::DateAtFraction(start, fraction);
		// Within half a moon month, at noon, and within half a day's step of the point asked for
		EXPECT_LE(std::llabs(date - start), 16 * 86400);
		EXPECT_EQ(date % 86400, 43200);
		const double apart = std::abs(moon::MonthFraction(date) - fraction);
		EXPECT_LE(std::min(apart, 1.0 - apart), 0.5 * 0.0338632);
	}
}

TEST(Moon, SkyAnglesFromTheCamera)
{
	// At midnight it is due east, about 13 degrees up
	const auto midnight = moon::Angles(moon::Offset(0.0f));
	EXPECT_NEAR(midnight.azimuth, 90.0f, 1e-3f);
	EXPECT_NEAR(midnight.elevation, 13.36f, 0.01f);
	// In the evening it is south of east, and in the morning north of east
	EXPECT_GT(moon::Angles(moon::Offset(20.0f)).azimuth, 90.0f);
	EXPECT_LT(moon::Angles(moon::Offset(4.0f)).azimuth, 90.0f);
}
