/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Debug/GesturesModel.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/GestureTemplates.h"

using namespace openblack;
using namespace openblack::debug::gestures_window;
using namespace openblack::magic::gestures;

namespace
{
constexpr float k_Ratio = 4.0f / 3.0f;

/// A made-up template: key points at the corners of a unit polyline, with their headings and turns
GestureData TemplateOf(Gesture gesture, const std::vector<glm::vec2>& corners)
{
	GestureData tpl;
	tpl.SetToZero();
	tpl.gesture = gesture;
	tpl.positionMode = 2;
	tpl.checkDirection = true;
	float previous = 0.0f;
	for (size_t k = 0; k < corners.size(); ++k)
	{
		KeySample s;
		s.x = corners[k].x;
		s.z = corners[k].y;
		if (k + 1 < corners.size())
		{
			const auto d = corners[k + 1] - corners[k];
			const float heading = Atan2Positive(d.x, d.y);
			s.direction = Octant(heading);
			s.turn = k == 0 ? 0.0f : WrapDifference(previous, heading);
			previous = heading;
		}
		tpl.Append(s);
	}
	return tpl;
}

std::vector<GestureData> MadeUpTemplates()
{
	return {
	    TemplateOf(k_Circle, {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0.05f}}),
	    TemplateOf(k_Spiral, {{0, 1}, {0.5f, 0}, {1, 1}}),
	};
}
} // namespace

TEST(GesturesMenu, Names)
{
	EXPECT_EQ(GestureName(k_None), "NONE");
	EXPECT_EQ(GestureName(k_Circle), "CIRCLE");
	EXPECT_EQ(GestureName(k_RShape), "R_SHAPE");
	EXPECT_EQ(GestureName(23), "SQUARE_WAVE");
	EXPECT_EQ(GestureName(24), "?");
}

TEST(GesturesMenu, KeyPoints)
{
	EXPECT_EQ(KeyPointOf(0), KeyPoint::None);
	EXPECT_EQ(KeyPointOf(Sample::k_Start), KeyPoint::Start);
	EXPECT_EQ(KeyPointOf(Sample::k_Corner), KeyPoint::Corner);
	EXPECT_EQ(KeyPointOf(Sample::k_Anchor), KeyPoint::Anchor);
	EXPECT_EQ(KeyPointOf(Sample::k_End), KeyPoint::End);
	// A corner that is also the newest sample shows as a corner
	EXPECT_EQ(KeyPointOf(Sample::k_Corner | Sample::k_End), KeyPoint::Corner);
}

TEST(GesturesMenu, LookingFor)
{
	EXPECT_TRUE(LookingForName(0).empty());
	EXPECT_EQ(LookingForName(0xC), "a circle");
	EXPECT_EQ(LookingForName(0xD), "to power down");
	EXPECT_EQ(LookingForName(0xE), "to power up to level 0");
	EXPECT_EQ(LookingForName(0xF), "to power up to level 1");
	EXPECT_EQ(LookingForName(5), "type 5");
}

TEST(GesturesMenu, FirstTemplate)
{
	const auto templates = MadeUpTemplates();
	EXPECT_EQ(FirstTemplate(templates, k_Circle), &templates[0]);
	EXPECT_EQ(FirstTemplate(templates, k_Spiral), &templates[1]);
	EXPECT_EQ(FirstTemplate(templates, k_Scribble), nullptr);
}

TEST(GesturesMenu, TemplateStrokeIsCentred)
{
	const auto stroke = TemplateStroke(MadeUpTemplates()[0], 200.0f, {400.0f, 300.0f}, k_Ratio);
	ASSERT_EQ(stroke.size(), 5u);
	EXPECT_EQ(stroke[0], glm::vec2(300.0f, 300.0f - 100.0f / k_Ratio));
	EXPECT_EQ(stroke[2], glm::vec2(500.0f, 300.0f + 100.0f / k_Ratio));
}

TEST(GesturesMenu, MousePositionsFollowTheStroke)
{
	EXPECT_TRUE(MousePositions({}).empty());
	const std::vector<glm::vec2> line {{0.0f, 0.0f}, {100.0f, 0.0f}};
	const auto pixels = MousePositions(line);
	EXPECT_EQ(pixels.front(), glm::ivec2(0, 0));
	EXPECT_EQ(pixels.back(), glm::ivec2(100, 0));
	// At least five pixels apart: 100 px in 5 px steps, both ends included
	EXPECT_EQ(pixels.size(), 21u);
}

TEST(GesturesMenu, ADrawnTemplateMatchesItsGestureOnly)
{
	const auto templates = MadeUpTemplates();
	const auto stroke = TemplateStroke(templates[0], 200.0f, {400.0f, 300.0f}, k_Ratio);
	GestureSystem system;
	for (const auto& pixel : MousePositions(stroke))
	{
		system.AddSample(glm::vec3(pixel.x, 0.0f, pixel.y), pixel);
	}
	const auto matches = MatchesNow(templates, BuildFromSystem(system, k_Ratio), k_Ratio);
	ASSERT_EQ(matches.size(), 1u);
	EXPECT_EQ(matches[0].gesture, k_Circle);
	EXPECT_FALSE(matches[0].mirrored);
	EXPECT_EQ(matches[0].templateIndex, 0);
}
