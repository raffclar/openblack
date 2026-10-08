/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The camera-facing modes of src/3D/Billboard.h against the original's formulas: the sprite (screen and horizontal),
// the moon, the bubble, the full particle sprite, UR_OrientSpriteWithVelocity and the yaw of the town centre's
// particle effects.

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/Billboard.h"

using namespace openblack::graphics;

namespace
{
constexpr float k_Epsilon = 1e-5f;

/// A camera at the origin looking down +z (lookAtLH): right +x, up +y, the view the identity
billboard::CameraFrame StraightFrame()
{
	billboard::CameraFrame frame;
	frame.nearZ = 1.0f;
	return frame;
}

void ExpectNear(const glm::vec3& a, const glm::vec3& b, float epsilon = k_Epsilon)
{
	EXPECT_NEAR(a.x, b.x, epsilon);
	EXPECT_NEAR(a.y, b.y, epsilon);
	EXPECT_NEAR(a.z, b.z, epsilon);
}
} // namespace

TEST(Billboard, screenQuadTurnsClockwiseAndSubtractsTheOrigin)
{
	const auto frame = StraightFrame();
	billboard::Sprite sprite;
	sprite.position = glm::vec3(0.0f, 0.0f, 10.0f);
	sprite.size = 1.0f;
	sprite.height = 2.0f;
	// no turn: v0 top left, v1 top right, v2 bottom right, v3 bottom left; y = +-h s
	auto quad = billboard::Screen(sprite, frame);
	ExpectNear(quad.corners[0], glm::vec3(-1.0f, 2.0f, 10.0f));
	ExpectNear(quad.corners[1], glm::vec3(1.0f, 2.0f, 10.0f));
	ExpectNear(quad.corners[2], glm::vec3(1.0f, -2.0f, 10.0f));
	ExpectNear(quad.corners[3], glm::vec3(-1.0f, -2.0f, 10.0f));
	// local x goes to (cos, -sin) on the screen; a quarter turn sends it down the screen
	sprite.angle = std::numbers::pi_v<float> * 0.5f;
	quad = billboard::Screen(sprite, frame);
	ExpectNear(quad.corners[1] - quad.corners[0], glm::vec3(0.0f, -2.0f, 0.0f));
	ExpectNear(quad.corners[0] - quad.corners[3], glm::vec3(4.0f, 0.0f, 0.0f)); // local y -> (sin, cos)
	// the fire's flames: oy = -2 size is subtracted, so the base is on the point
	sprite.angle = 0.0f;
	sprite.origin = glm::vec2(0.0f, -2.0f);
	quad = billboard::Screen(sprite, frame);
	EXPECT_NEAR(quad.corners[0].y, 4.0f, k_Epsilon);
	EXPECT_NEAR(quad.corners[3].y, 0.0f, k_Epsilon);
}

TEST(Billboard, screenQuadNearTestAndCells)
{
	const auto frame = StraightFrame();
	billboard::Sprite sprite;
	sprite.position = glm::vec3(0.0f, 0.0f, 1.0f); // at the near plane: not drawn
	EXPECT_FALSE(billboard::SpriteQuad(sprite, frame).has_value());
	sprite.position.z = 1.5f;
	ASSERT_TRUE(billboard::SpriteQuad(sprite, frame).has_value());
	// the horizontal sprite has no near test
	sprite.position.z = -5.0f;
	sprite.horizontal = true;
	EXPECT_TRUE(billboard::SpriteQuad(sprite, frame).has_value());
	// cell 9 of 8 per row: column 1, row 1
	const auto uv = billboard::CellUv(9, 8);
	EXPECT_FLOAT_EQ(uv[0].x, 0.125f);
	EXPECT_FLOAT_EQ(uv[0].y, 0.125f);
	EXPECT_FLOAT_EQ(uv[1].x, 0.25f);
	EXPECT_FLOAT_EQ(uv[2].y, 0.25f);
	EXPECT_FLOAT_EQ(uv[3].x, 0.125f);
	// only the low 6 bits are the cell
	EXPECT_FLOAT_EQ(billboard::CellUv(64 + 9, 8)[0].x, 0.125f);
}

TEST(Billboard, horizontalQuadIsRyOfTheAngle)
{
	billboard::Sprite sprite;
	sprite.position = glm::vec3(5.0f, 1.0f, 5.0f);
	sprite.size = 1.0f;
	sprite.height = 3.0f;
	sprite.horizontal = true;
	// a quarter turn: local x -> (0, 0, 1), local z -> (-1, 0, 0); v0 = (-s, -hs)
	sprite.angle = std::numbers::pi_v<float> * 0.5f;
	const auto quad = billboard::Horizontal(sprite);
	ExpectNear(quad.corners[0], glm::vec3(5.0f + 3.0f, 1.0f, 5.0f - 1.0f));
	ExpectNear(quad.corners[1], glm::vec3(5.0f + 3.0f, 1.0f, 5.0f + 1.0f));
	ExpectNear(quad.corners[2], glm::vec3(5.0f - 3.0f, 1.0f, 5.0f + 1.0f));
}

TEST(Billboard, spriteShaderModelIsTheScreenQuad)
{
	// vs_sprite: translation + u_invView (model (x, y, 0, 0)) on the plane -1..1, v = 0 at the top
	billboard::CameraFrame frame;
	frame.right = glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f));
	frame.up = glm::vec3(0.0f, 1.0f, 0.0f);
	frame.forward = glm::cross(frame.right, frame.up) * -1.0f;
	billboard::Sprite sprite;
	sprite.position = glm::vec3(3.0f, 4.0f, 5.0f);
	sprite.size = 2.0f;
	sprite.angle = 0.7f;
	const auto quad = billboard::Screen(sprite, frame);
	const auto model = billboard::ScreenSpriteModel(sprite.position, glm::vec2(sprite.size), sprite.angle);
	const glm::mat3 invView(frame.right, frame.up, frame.forward);
	const auto corner = [&](float x, float y) {
		return glm::vec3(model[3]) + invView * glm::vec3(model * glm::vec4(x, y, 0.0f, 0.0f));
	};
	ExpectNear(corner(-1.0f, 1.0f), quad.corners[0], 1e-4f);
	ExpectNear(corner(1.0f, 1.0f), quad.corners[1], 1e-4f);
	ExpectNear(corner(1.0f, -1.0f), quad.corners[2], 1e-4f);
}

TEST(Billboard, moonFacesTheEye)
{
	const glm::mat4 identity(1.0f);
	// straight ahead: the view's own axes x 4
	auto basis = billboard::MoonBasis(identity, identity, glm::vec3(0.0f, 0.0f, 100.0f));
	ExpectNear(basis[0], glm::vec3(4.0f, 0.0f, 0.0f));
	ExpectNear(basis[1], glm::vec3(0.0f, 4.0f, 0.0f));
	ExpectNear(basis[2], glm::vec3(0.0f, 0.0f, 4.0f));
	// off to the right: +Z points from the eye to the moon, X stays horizontal and square to it
	basis = billboard::MoonBasis(identity, identity, glm::vec3(100.0f, 0.0f, 100.0f));
	ExpectNear(glm::normalize(basis[2]), glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f)));
	EXPECT_NEAR(basis[0].y, 0.0f, k_Epsilon);
	EXPECT_NEAR(glm::dot(basis[0], basis[2]), 0.0f, 1e-4f);
	// the halo: v0 = p - 500 (r0 + r1) has the UV (0.25, 0.25)
	basis = billboard::MoonBasis(identity, identity, glm::vec3(0.0f, 0.0f, 100.0f));
	const auto halo = billboard::MoonHalo(basis, glm::vec3(0.0f, 0.0f, 100.0f));
	ExpectNear(halo.corners[0], glm::vec3(-2000.0f, -2000.0f, 100.0f), 1e-2f);
	ExpectNear(halo.corners[3], glm::vec3(2000.0f, 2000.0f, 100.0f), 1e-2f);
	EXPECT_FLOAT_EQ(halo.uv[0].y, 0.25f);
	EXPECT_FLOAT_EQ(halo.uv[2].y, 0.49375f);
	// the mesh: a -0.1309 turn sends local X towards +Y (glm::rotate(+7.5 deg, Z)), x 0.65
	const auto model = billboard::MoonModel(basis, glm::vec3(0.0f, 0.0f, 100.0f), -std::numbers::pi_v<float>);
	EXPECT_NEAR(model[0].y, 4.0f * 0.65f * std::sin(0.1308997f), 1e-4f);
	EXPECT_NEAR(model[0].x, 4.0f * 0.65f * std::cos(0.1308997f), 1e-4f);
	// the turn about Y by phase + pi, with phase + pi = pi / 2: r0' = r2 (glm::rotate(-(phase + pi), Y))
	const auto turned = billboard::MoonModel(glm::mat3(1.0f), glm::vec3(0.0f), -std::numbers::pi_v<float> * 0.5f);
	EXPECT_NEAR(turned[0].z, 0.65f, 1e-4f);
	EXPECT_NEAR(turned[2].x, -0.65f * std::cos(0.1308997f), 1e-4f);
}

TEST(Billboard, bubbleLooksAtTheEye)
{
	// local +Y (= -D) towards the eye, about the box centre
	const glm::vec3 position(10.0f, 0.0f, 0.0f);
	const glm::vec3 centre(0.0f, 1.0f, 0.0f);
	const glm::vec3 eye(10.0f, 1.0f, -20.0f);
	auto look = billboard::LookAtCentre(position, centre, 1.0f, eye);
	ExpectNear(look.axes[1], glm::vec3(0.0f, 0.0f, -1.0f));
	ExpectNear(look.axes[2], glm::vec3(0.0f, 1.0f, 0.0f));
	// the centre stays where it is
	ExpectNear(look.axes * centre + position + look.offset, position + centre);
	// straight above: x is pushed to -1e-4, so the bubble still has a frame
	look = billboard::LookAtCentre(position, centre, 1.0f, position + centre + glm::vec3(0.0f, 30.0f, 0.0f));
	EXPECT_NEAR(look.axes[1].y, 1.0f, 1e-4f);
	EXPECT_NEAR(glm::length(look.axes[2]), 1.0f, 1e-4f);
}

TEST(Billboard, particleSpriteModesAndAngles)
{
	// the full sprite: +Y towards the eye, Z horizontal, x the scale
	const glm::vec3 p(0.0f);
	const glm::vec3 eye(3.0f, 4.0f, 0.0f);
	const auto axes = billboard::FullSprite(p, eye, 2.0f);
	ExpectNear(glm::normalize(axes[1]), glm::normalize(eye - p));
	EXPECT_NEAR(axes[2].y, 0.0f, k_Epsilon);
	EXPECT_NEAR(glm::length(axes[0]), 2.0f, 1e-4f);
	// the town centre's effects: local +Z points from the eye to the object in XZ
	const auto yaw = billboard::YawToEye(glm::vec3(0.0f), glm::vec3(0.0f, 5.0f, -10.0f));
	ExpectNear(yaw[2], glm::vec3(0.0f, 0.0f, 1.0f));
	// UR_OrientSpriteWithVelocity: a velocity up the screen gives angle 0 (the sprite's +y along it), to the right pi/2
	const glm::vec3 right(1.0f, 0.0f, 0.0f);
	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	EXPECT_NEAR(billboard::ScreenVelocity(up, right, up), 0.0f, k_Epsilon);
	const float angle = billboard::ScreenVelocity(right, right, up);
	EXPECT_NEAR(angle, std::numbers::pi_v<float> * 0.5f, k_Epsilon);
	billboard::Sprite sprite;
	sprite.position = glm::vec3(0.0f, 0.0f, 10.0f);
	sprite.angle = angle;
	const auto quad = billboard::Screen(sprite, StraightFrame());
	ExpectNear(quad.corners[0] - quad.corners[3], glm::vec3(2.0f, 0.0f, 0.0f)); // local +y along the velocity
}

TEST(Billboard, mistShrinksWithoutClamp)
{
	// size / (1 + (k - 1)(1 - |d.y| / |d|)), also under 1 m from the eye
	EXPECT_NEAR(billboard::MistShrunkSize(1.0f, 3.0f, glm::vec3(0.0f, 0.3f, 0.4f)), 1.0f / 1.8f, k_Epsilon);
	EXPECT_NEAR(billboard::MistShrunkSize(2.0f, 3.0f, glm::vec3(0.0f, 0.0f, 50.0f)), 2.0f / 3.0f, k_Epsilon);
	EXPECT_NEAR(billboard::MistShrunkSize(2.0f, 3.0f, glm::vec3(0.0f, -50.0f, 0.0f)), 2.0f, k_Epsilon);
	// (inferred) d = 0 keeps the size
	EXPECT_EQ(billboard::MistShrunkSize(2.0f, 3.0f, glm::vec3(0.0f)), 2.0f);
}

TEST(Billboard, ribbonSideAndHalfWidth)
{
	// side = (eye - joint) x segment, normalised; the vertices are joint +- side x scale
	// (the joint's PSR scale)
	const auto side = billboard::RibbonSide(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
	ASSERT_TRUE(side.has_value());
	EXPECT_NEAR(glm::distance(*side, glm::cross(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(1.0f, 0.0f, 0.0f)) / 10.0f), 0.0f,
	            k_Epsilon);
	EXPECT_EQ(billboard::RibbonHalfWidth(2.5f), 2.5f);
}
