/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The release prediction's physics part (src/ECS/Physics/FromHand.h PredictRelease) against the original. The expected
// bit patterns come from an independent float32 model of the FPU at 24-bit precision, written operation by operation
// from the original: PhysicsBody::HandAngularMomentum,
// PhysicsBody::DrawOrigin (the prediction's drawn origin) and from_hand::CapReleaseVelocity.
// PhysicsBody::ObjectOrigin is checked against the draw's origin.

#include <cstdint>

#include <array>
#include <bit>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsBody.h"

using openblack::ecs::physics::PhysicsBody;
using openblack::ecs::physics::PhysicsData;
namespace from_hand = openblack::ecs::physics::from_hand;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}
} // namespace

TEST(ReleasePrediction, HandAngularMomentum)
{
	// the body's tensor, row major (M0..M8), and the hand's vector h
	const std::array<std::array<float, 3>, 3> inertia = {
	    {{2.25f, -0.375f, 0.8125f}, {-0.4375f, 3.0625f, -0.15625f}, {0.8125f, 0.5f, 1.6875f}}};
	const auto l = PhysicsBody::HandAngularMomentum(inertia, glm::vec3(0.3f, -1.7f, 2.9f));
	// this port's sign: the opposite of the original's stored value (its torque is F x r)
	EXPECT_EQ(Bits(l.x), 0xC071999Au);
	EXPECT_EQ(Bits(l.y), 0x4077999Au);
	EXPECT_EQ(Bits(l.z), 0xC0ACE667u);
}

TEST(ReleasePrediction, DrawOrigin)
{
	glm::mat3 rows;
	rows[0] = glm::vec3(0.6f, 0.0f, -0.8f);
	rows[1] = glm::vec3(0.0f, 1.0f, 0.0f);
	rows[2] = glm::vec3(0.8f, 0.0f, 0.6f);
	const auto origin =
	    PhysicsBody::DrawOrigin(rows, glm::vec3(1810.5f, 54.25f, 2640.75f), 1.7f, glm::vec3(0.21f, -0.13f, 0.37f));
	EXPECT_EQ(Bits(origin.x), 0x44E2390Bu);
	EXPECT_EQ(Bits(origin.y), 0x4259E24Eu);
	EXPECT_EQ(Bits(origin.z), 0x45250A88u);
}

TEST(ReleasePrediction, CapReleaseVelocity)
{
	struct Case
	{
		glm::vec3 in;
		std::array<uint32_t, 3> out;
		bool fast;
	};
	const std::array<Case, 4> cases = {{
	    {glm::vec3(130.0f, -40.0f, 55.5f), {0x42DB773Eu, 0xC2070E4Du, 0x423B63D8u}, true}, // capped to 124
	    {glm::vec3(0.5f, 0.6f, 0.4f), {0x3F000000u, 0x3F19999Au, 0x3ECCCCCDu}, false},     // l2 0.77: not fast
	    {glm::vec3(0.0f), {0u, 0u, 0u}, false},                                            // the zero test
	    {glm::vec3(80.0f, 70.0f, -60.0f), {0x42A00000u, 0x428C0000u, 0xC2700000u}, true},  // l2 14900 < 15376
	}};
	for (const auto& c : cases)
	{
		const auto capped = from_hand::CapReleaseVelocity(c.in);
		EXPECT_EQ(Bits(capped.velocity.x), c.out[0]);
		EXPECT_EQ(Bits(capped.velocity.y), c.out[1]);
		EXPECT_EQ(Bits(capped.velocity.z), c.out[2]);
		EXPECT_EQ(capped.fast, c.fast);
	}
}

TEST(ReleasePrediction, ObjectOriginIsTheBodysOrigin)
{
	// ObjectOrigin and the prediction draw (DrawOrigin) take the same point in different orders
	constexpr std::array<glm::vec3, 6> k_Local = {
	    glm::vec3(1.3f, 0.2f, -0.7f), glm::vec3(-0.9f, 0.6f, 0.4f),   glm::vec3(0.1f, -1.1f, 0.8f),
	    glm::vec3(0.5f, 0.9f, 1.2f),  glm::vec3(-0.4f, -0.3f, -1.0f), glm::vec3(0.7f, 1.4f, -0.2f),
	};
	constexpr std::array<std::array<uint32_t, 3>, 4> k_Triangles = {{{0, 1, 2}, {0, 2, 3}, {1, 4, 5}, {3, 4, 5}}};
	constexpr PhysicsData k_Data = {0.8f, 300.0f, 30.0f, 0.5f, 0.3f, 0.1f};
	const glm::mat3 rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), 0.7f, glm::normalize(glm::vec3(0.3f, 1.0f, -0.5f))));
	PhysicsBody body;
	body.Initialise(1.7f, 1.0f);
	body.SetUpConstants(6.75f, k_Data, true);
	body.BuildShape(k_Local, k_Triangles, glm::vec3(0.21f, -0.13f, 0.37f), 1.5f, 1.0f, rotation,
	                glm::vec3(1810.5f, 54.25f, 2640.75f));
	const auto a = body.ObjectOrigin();
	const auto b = body.DrawOrigin(body.Rotation(), body.Centre());
	EXPECT_NEAR(a.x, b.x, 1e-3f);
	EXPECT_NEAR(a.y, b.y, 1e-3f);
	EXPECT_NEAR(a.z, b.z, 1e-3f);
}
