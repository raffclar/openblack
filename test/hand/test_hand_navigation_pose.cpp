/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/HandNavigationPose.h"

using namespace openblack;
using namespace openblack::hand_navigation_pose;
namespace tricon = camera_drag::tricon;

TEST(HandNavigationPose, GrippingTheLandShowsTheGrip)
{
	EXPECT_EQ(WhileDragging(true, tricon::k_Rotate), Pose::Grip);
	EXPECT_EQ(WhileDragging(true, 0), Pose::Grip);
}

TEST(HandNavigationPose, DraggingByTheEdgeShowsTurningTiltingOrZooming)
{
	EXPECT_EQ(WhileDragging(false, tricon::k_Rotate | tricon::k_Pitch), Pose::Rotate);
	EXPECT_EQ(WhileDragging(false, tricon::k_Pitch), Pose::Pitch);
	EXPECT_EQ(WhileDragging(false, tricon::k_Zoom | tricon::k_Pitch), Pose::Zoom);
	EXPECT_EQ(WhileDragging(false, 0), Pose::Idle);
	// Without the camera help's turning, there is no turning pose
	EXPECT_EQ(WhileDragging(false, tricon::k_Rotate, camera_drag::feature::k_Pitch), Pose::Idle);
}

TEST(HandNavigationPose, HoveringAtTheEdgeOffersTurning)
{
	EXPECT_EQ(WhileHovering(tricon::k_Idle), std::nullopt);
	EXPECT_EQ(WhileHovering(tricon::k_Idle | tricon::k_Rotate | tricon::k_Turning), Pose::Rotate);
	// Turning wins over tilting at the very bottom and the top
	EXPECT_EQ(WhileHovering(tricon::k_Idle | tricon::k_Rotate | tricon::k_Pitch), Pose::Rotate);
	EXPECT_EQ(WhileHovering(tricon::k_Pitch), Pose::Pitch);
}

TEST(HandNavigationPose, StandsUpTowardsTheFocus)
{
	// The hand leans up by three tenths of the distance to the focus
	const auto up = UpTowardsFocus({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	EXPECT_NEAR(up.x, 10.0f / glm::length(glm::vec3(10.0f, 3.0f, 0.0f)), 1e-6f);
	EXPECT_NEAR(up.y, 3.0f / glm::length(glm::vec3(10.0f, 3.0f, 0.0f)), 1e-6f);
	EXPECT_EQ(up.z, 0.0f);
	// At the focus there is no way to stand
	EXPECT_EQ(UpTowardsFocus({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}), glm::vec3(0.0f));
}

namespace
{
struct FocusCase
{
	std::array<uint32_t, 3> from;
	std::array<uint32_t, 3> focus;
	std::array<uint32_t, 3> up;
};

/// Made in float steps in the game's order: the lean of three tenths, the squares added y, x, z
constexpr std::array<FocusCase, 5> k_FocusCases {{
    {{0xC4302AD0, 0x423504DE, 0x4416EF3A}, {0xC4D5C828, 0x42565A51, 0xC4064FA3}, {0xBF21E151, 0x3E959575, 0xBF37AD25}},
    {{0xC4DD0023, 0x43183B10, 0xC4E7408E}, {0xC384B569, 0x40DF8990, 0xC4CCA4BC}, {0x3F7845D6, 0x3E4E481A, 0x3E0CB099}},
    {{0xC396F62D, 0x43780E3E, 0xC4BC1959}, {0xC48A616A, 0x427AF929, 0x44DFDABF}, {0xBE6BFD85, 0x3E747234, 0x3F717F29}},
    {{0x439A34B6, 0x42EE021F, 0x44EE20A7}, {0xC4E2B56B, 0x42ABB196, 0xC4526406}, {0xBF164E3C, 0x3E8ED03D, 0xBF4289E7}},
    {{0xC4B1DF59, 0x420D59C7, 0xC43F84A7}, {0x449E102D, 0x419094C3, 0x43A33349}, {0x3F63858F, 0x3E9070BD, 0x3EB8FA1A}},
}};

glm::vec3 Vec(const std::array<uint32_t, 3>& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}
} // namespace

TEST(HandNavigationPose, TheUpTowardsTheFocusIsTheGamesBitForBit)
{
	for (const auto& c : k_FocusCases)
	{
		const auto up = UpTowardsFocus(Vec(c.from), Vec(c.focus));
		EXPECT_EQ(std::bit_cast<uint32_t>(up.x), c.up[0]);
		EXPECT_EQ(std::bit_cast<uint32_t>(up.y), c.up[1]);
		EXPECT_EQ(std::bit_cast<uint32_t>(up.z), c.up[2]);
	}
}
