/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/HandFaces.h"
#include "Graphics/ModelLight.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
// A fake triangle, seen from a camera that looks along +z. Its corners go clockwise on the screen, as the outer face of
// an L3D mesh does.
constexpr std::array<glm::vec3, 3> k_Triangle {glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 1.0f, 5.0f),
                                               glm::vec3(1.0f, 0.0f, 5.0f)};
constexpr std::array<uint16_t, 3> k_Corners {0, 1, 2};
const glm::mat3 k_Mirror {glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

// Whether the corners, placed by the matrix, go clockwise on a screen whose x goes right and y goes up
bool Clockwise(const glm::mat3& place, const std::array<uint16_t, 3>& corners)
{
	const auto a = place * k_Triangle.at(corners[0]);
	const auto b = place * k_Triangle.at(corners[1]);
	const auto c = place * k_Triangle.at(corners[2]);
	return ((b.x - a.x) * (c.y - a.y)) - ((c.x - a.x) * (b.y - a.y)) < 0.0f;
}

// Whether a draw keeps the triangle: a usual model keeps the clockwise faces, a draw with turned faces the others
bool Kept(const glm::mat3& place, const std::array<uint16_t, 3>& corners, bool facesTurned)
{
	return Clockwise(place, corners) != facesTurned;
}
} // namespace

// An unmirrored hand keeps its corners
TEST(HandFaces, TheLeftHandKeepsItsCorners)
{
	EXPECT_EQ(hand_faces::Corners(k_Corners, false), k_Corners);
}

// The game changes the first two corners of each triangle of a mirrored hand
TEST(HandFaces, TheRightHandChangesTheFirstTwoCorners)
{
	EXPECT_EQ(hand_faces::Corners(k_Corners, true), (std::array<uint16_t, 3> {1, 0, 2}));
}

// The mirror turns a face round on the screen, and the changed corners turn it back
TEST(HandFaces, TheRightHandShowsTheOuterFaces)
{
	EXPECT_TRUE(Clockwise(glm::mat3(1.0f), k_Corners));
	EXPECT_FALSE(Clockwise(k_Mirror, k_Corners));
	EXPECT_TRUE(Clockwise(k_Mirror, hand_faces::Corners(k_Corners, true)));
}

// Turned faces on the file's corners keep the same triangles as the game's changed corners, in each view
TEST(HandFaces, TurnedFacesKeepWhatTheChangedCornersKeep)
{
	for (const bool mirroredHand : {false, true})
	{
		const auto place = mirroredHand ? k_Mirror : glm::mat3(1.0f);
		const bool game = Kept(place, hand_faces::Corners(k_Corners, mirroredHand), false);
		EXPECT_EQ(Kept(place, k_Corners, hand_faces::FacesTurned(false, mirroredHand)), game);
		EXPECT_TRUE(game);
	}
}

// The sea's mirrored view turns the faces round, and a mirrored hand turns them back
TEST(HandFaces, TheSeaTurnsTheFacesOfTheLeftHandOnly)
{
	EXPECT_FALSE(hand_faces::FacesTurned(false, false));
	EXPECT_TRUE(hand_faces::FacesTurned(false, true));
	EXPECT_TRUE(hand_faces::FacesTurned(true, false));
	EXPECT_FALSE(hand_faces::FacesTurned(true, true));
}

// The light meets the normal in the space of the bone, through the inverse of the mirrored matrix, as the game does. So a
// face of the mirrored hand that looks at the light is lit, and its normal is not changed.
TEST(HandFaces, TheMirroredHandIsLitOnTheSideThatFacesTheLight)
{
	const glm::vec3 origin(10.0f, 2.0f, -3.0f);
	const glm::vec3 light = origin + glm::vec3(-4.0f, 0.0f, 0.0f);
	const glm::vec3 normal(1.0f, 0.0f, 0.0f);
	// The mirror moves the normal to -x in the world, which looks at the light
	const auto local = model_light::LocalDirection(light, k_Mirror[0], k_Mirror[1], k_Mirror[2], origin);
	EXPECT_FLOAT_EQ(local.x, 1.0f);
	EXPECT_GT(model_light::PosedFacing(normal, local), 0.0f);
	// The other side does not look at the light
	EXPECT_LT(model_light::PosedFacing(-normal, local), 0.0f);
}
