/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// More of the game's sum orders and steps. Each expected value is written out in the order of the game's instructions,
// on values where a different order or step gives a different last bit.

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/ScreenPick.h"
#include "Common/X87.h"
#include "Common/X87Angle.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreaturePointing.h"
#include "Creature/LeashRope.h"
#include "Physics/DamageMesh.h"
#include "Physics/ObjectRules.h"

namespace x87 = openblack::x87;
using x87::SumOrder;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// A vector whose squares add to different floats in different orders
const glm::vec3 k_Odd {-12.43f, 20.68f, -15.84f};
/// The same parts with x and z swapped
const glm::vec3 k_OddSwapped {-15.84f, 20.68f, -12.43f};

float SquareSumZyx(const glm::vec3& v)
{
	return ((v.z * v.z) + (v.y * v.y)) + (v.x * v.x);
}

openblack::physics::damage::Corner CornerAt(glm::vec3 position)
{
	return {.position = position, .uv = glm::vec2(0.0f)};
}
} // namespace

TEST(GlmOrder2, AnEdgeOfThirtyTwoBySumsFromZIsNotTheLargestSizeClass)
{
	using namespace openblack::physics::damage;
	// The squares of this edge add to exactly 32 from z to x, and to just over 32 from x to z
	const glm::vec3 edge {1.69f, 3.84f, std::bit_cast<float>(0x4072D93Fu)};
	ASSERT_EQ(Bits(SquareSumZyx(edge)), 0x42000000u);
	ASSERT_GT(glm::dot(edge, edge), 32.0f);
	const auto triangle = MakeTriangle({CornerAt({0.0f, 0.0f, 0.0f}), CornerAt(edge), CornerAt({-10.0f, 0.0f, 10.0f})});
	EXPECT_EQ(triangle.sizeClass, 2);
}

TEST(GlmOrder2, HalvingTakesTheLongestSideBySumsFromZ)
{
	using namespace openblack::physics::damage;
	// From z to x the side from the last corner to the first is the longer; from x to z it is the first side
	ASSERT_LT(SquareSumZyx(k_Odd), SquareSumZyx(k_OddSwapped));
	ASSERT_GT(glm::dot(k_Odd, k_Odd), glm::dot(k_OddSwapped, k_OddSwapped));
	auto triangle = MakeTriangle({CornerAt({0.0f, 0.0f, 0.0f}), CornerAt(k_Odd), CornerAt(k_OddSwapped)});
	const auto halves = Halve(triangle);
	EXPECT_EQ(halves[0].corners[0].position, k_OddSwapped);
	EXPECT_EQ(halves[1].corners[1].position, glm::vec3(0.0f));
}

TEST(GlmOrder2, RopeTensionMeasuresTheFirstSegmentFromZ)
{
	using namespace openblack::leash_rope;
	Rope rope {.start = glm::vec3(1.0f, 2.0f, 3.0f), .slackLength = 41.0f * 20.0f, .maxLength = 41.0f * 40.0f};
	rope.nodes.front().position = rope.start + k_Odd;
	const float rest = rope.slackLength * k_RestShare;
	const float taut = rope.maxLength * k_RestShare;
	const float first = std::sqrt(SquareSumZyx(rope.nodes.front().position - rope.start));
	EXPECT_EQ(Bits(Tension(rope)), Bits((first - rest) / (taut - rest)));
}

TEST(GlmOrder2, TheHandAndTheLookWayGoThroughOneShareOfTheLength)
{
	using namespace openblack::creature_feedback;
	const glm::vec3 camera {100.0f, 50.0f, 25.0f};
	const float share = 3.0f / std::sqrt(SquareSumZyx(k_Odd));
	const auto point = PointOnSight(camera, k_Odd, 3.0f);
	EXPECT_EQ(point, camera + (k_Odd * share));
	EXPECT_EQ(PointOnSight(camera, glm::vec3(0.0f), 3.0f), camera);
	// Held, the creature looks at a point on the way less a share of its height, the way not normalised again
	const glm::vec3 hand {120.0f, 40.0f, 30.0f};
	const auto way = (camera + (k_Odd * (1.0f / std::sqrt(SquareSumZyx(k_Odd))))) - camera;
	const auto look = HeldLookPoint(hand, hand + glm::vec3(0.0f, 100.0f, 0.0f), camera, k_Odd, 10.0f);
	EXPECT_EQ(look, hand - (way * (10.0f * k_LooksNearerTheCamera)));
}

TEST(GlmOrder2, SceneryRadiusIsTheBoxHalfDiagonalFromZ)
{
	using namespace openblack::creature_obstacles;
	Thing thing {};
	thing.shape = Shape::Scenery;
	thing.avoidance = Avoidance::Always;
	thing.boxHalfSize = k_Odd;
	const auto circles = CirclesOf(thing, Walker {});
	ASSERT_EQ(circles.size(), 1u);
	EXPECT_EQ(Bits(circles[0].radius), Bits(std::sqrt(SquareSumZyx(k_Odd)) * 0.7f));
}

TEST(GlmOrder2, PickingSphereDistanceSumsYThenXThenZ)
{
	using namespace openblack::screen_pick;
	View view {};
	view.camera = glm::vec3(0.0f);
	view.near = 1.5f;
	const auto d = k_Odd + glm::vec3(0.0f, 2.0f, 0.0f);
	const float yxz = ((d.y * d.y) + (d.x * d.x)) + (d.z * d.z);
	EXPECT_EQ(Bits(SphereDistance(view, k_Odd, 2.0f, 4.0f)), Bits(std::sqrt(yxz) - (4.0f + 1.5f)));
}

TEST(GlmOrder2, MeshAndSkinIntersectionsMeasureTheSameWay)
{
	using namespace openblack::screen_pick;
	const std::array<glm::vec3, 3> corners {glm::vec3(-3.1f, 0.7f, 5.3f), glm::vec3(4.9f, 1.3f, 6.1f),
	                                        glm::vec3(0.3f, 7.9f, 4.7f)};
	const std::array<uint16_t, 3> indices {0, 1, 2};
	const glm::vec3 origin {0.2f, 0.3f, -2.0f};
	const glm::vec3 direction {0.05f, 0.33f, 1.0f};
	const auto mesh = NearestIntersection(corners, indices, origin, direction, false);
	const auto skin = NearestSkinIntersection(corners, indices, origin, direction, false);
	ASSERT_TRUE(mesh.has_value());
	ASSERT_TRUE(skin.has_value());
	EXPECT_EQ(Bits(mesh->distance), Bits(skin->distance));
	EXPECT_EQ(mesh->windingNormal, skin->windingNormal);
}

TEST(GlmOrder2, LocomotionHeadingIsTheGameHeadingMirrored)
{
	const glm::vec2 direction {0.3f, -0.7f};
	EXPECT_EQ(Bits(openblack::creature_locomotion::HeadingOf(direction)),
	          Bits(static_cast<float>(x87::GameHeadingOf(-direction.x, direction.y))));
	EXPECT_EQ(openblack::creature_locomotion::HeadingOf(glm::vec2(0.0f)), 0.0f);
}

TEST(GlmOrder2, KickTakesTheUpRowLessTheBackRow)
{
	const glm::vec2 forward {0.6f, -0.8f};
	const glm::vec2 lean {0.15f, -0.07f};
	const float scale = 1.3f;
	const float size = 1.7f;
	const float sine = forward.x;
	const float cosine = -forward.y;
	const float leaned = (-sine * lean.x) + (cosine * lean.y);
	const glm::vec3 d {scale * sine, scale - (scale * leaned), -(scale * cosine)};
	const float inverse = 1.0f / std::sqrt(((d.x * d.x) + (d.z * d.z)) + (d.y * d.y));
	const auto expected = (size * 45.0f) * (d * inverse);
	EXPECT_EQ(openblack::creature_object_actions::KickVelocity(forward, lean, scale, size), expected);
}

TEST(GlmOrder2, GetAwayPointUsesOneShareOfTheLength)
{
	const glm::vec3 thing {10.0f, 1.0f, 20.0f};
	const auto creature = thing + k_Odd;
	const auto away = creature - thing;
	const float share = 1.0f / std::sqrt(SquareSumZyx(away));
	EXPECT_EQ(openblack::creature_cast_moves::GetAwayPoint(creature, thing, 7.0f), ((away * share) * 7.0f) + creature);
	// Standing on the thing it goes along x
	EXPECT_EQ(openblack::creature_cast_moves::GetAwayPoint(thing, thing, 7.0f), thing + glm::vec3(7.0f, 0.0f, 0.0f));
}

TEST(GlmOrder2, PointingAimSumsZThenXThenY)
{
	using namespace openblack::creature_pointing;
	const glm::vec3 origin {0.0f, 0.0f, 0.0f};
	const glm::vec3 target {-0.9f, 0.4f, -1.3f};
	const float zxy = ((target.z * target.z) + (target.x * target.x)) + (target.y * target.y);
	const auto aim = AimAt(origin, glm::vec2(0.0f, -1.0f), target);
	EXPECT_EQ(Bits(aim.rise), Bits(target.y / std::sqrt(zxy)));
	EXPECT_EQ(Bits(k_AcrossLimit), 0x3F060A92u);
	EXPECT_EQ(Bits(k_AcrossScale), 0x3F747645u);
	EXPECT_EQ(Bits(k_TurnLimit), 0x3EF1463Bu);
}

TEST(GlmOrder2, AFelledTreeFallsAtTheGamesShareOfItsHalfHeight)
{
	using namespace openblack::physics::objects;
	const glm::vec2 away {3.0f, -4.0f};
	const double heading = x87::GameHeadingOf(away.x, away.y);
	const float halfHeight = 7.3f * 0.5f;
	const float speed = halfHeight * 0.4f;
	const float ahead = x87::Mul(x87::Sin(heading), speed);
	const float across = x87::Mul(x87::Cos(heading), speed);
	const auto fall = FellingOf(7.3f, away);
	EXPECT_EQ(Bits(fall.velocity.x), Bits(ahead));
	EXPECT_EQ(Bits(fall.velocity.z), Bits(-across));
	EXPECT_EQ(Bits(fall.spin.x), Bits(across * (1.0f / halfHeight)));
	EXPECT_EQ(Bits(fall.spin.z), Bits(ahead * (1.0f / halfHeight)));
}
