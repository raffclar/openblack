/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The SuperVillagers' eyes (ECS/SuperVillagerEyes.h): the blink, squint and glance step with scripted Random draws,
// the lids' angles, the mirror matrix, the shade of the eyes' normal table and the L3D byte patches made when the
// eyes are built (pure functions), the real CRT stream of Random through the eyes' step, and the list's bookkeeping
// when a host is destroyed while it is a SuperVillager (ECS/SuperVillager.h, a registry only: no files, no meshes).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstring>

#include <array>
#include <deque>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandomTesting.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"
#include "ECS/SuperVillagerEyes.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack::ecs::super_villager::eyes;

namespace
{
/// Random(a, b) answers in order, and the (a, b) it was asked
struct Script
{
	std::deque<float> answers;
	std::vector<std::pair<float, float>> asked;
	RandomFn Fn()
	{
		return [this](float a, float b) {
			asked.emplace_back(a, b);
			const float r = answers.empty() ? a : answers.front();
			if (!answers.empty())
			{
				answers.pop_front();
			}
			return r;
		};
	}
};
} // namespace

TEST(SuperVillagerEyes, FirstFrameBlinksAndPicksATarget)
{
	// the countdown is 0 at creation: 0 - 16 < 0 starts a blink at once; the shared target equals the glance (both 0)
	Timers timers;
	Shared shared;
	Script script {{2500.0f, 0.1f}, {}};
	Step(timers, shared, 16, script.Fn());
	EXPECT_TRUE(timers.blinking);
	EXPECT_EQ(timers.untilBlinkMs, 2500);
	EXPECT_FLOAT_EQ(timers.closure, 0.0f);
	EXPECT_FLOAT_EQ(shared.glanceTarget, 0.1f);
	EXPECT_FLOAT_EQ(timers.glance, 0.0f); // no move on the frame of a new target
	ASSERT_EQ(script.asked.size(), 2u);
	EXPECT_EQ(script.asked[0], std::make_pair(1000.0f, 5000.0f));
	EXPECT_EQ(script.asked[1], std::make_pair(-0.25f, 0.25f));
	EXPECT_EQ(shared.squintMs, 16u);
}

TEST(SuperVillagerEyes, PausedFirstFrameDoesNotBlink)
{
	// 0 - 0 = 0: not negative, no blink
	Timers timers;
	Shared shared;
	Script script {{0.2f}, {}};
	Step(timers, shared, 0, script.Fn());
	EXPECT_FALSE(timers.blinking);
}

TEST(SuperVillagerEyes, BlinkClosure)
{
	Shared shared;
	shared.glanceTarget = 1.0f; // far: no draw for the glance
	const auto closureAt = [&shared](int32_t t) {
		Timers timers;
		timers.blinking = true;
		timers.holdMs = 200;
		timers.blinkMs = t;
		Script script;
		Step(timers, shared, 0, script.Fn());
		return std::make_pair(timers.closure, timers.blinkMs);
	};
	EXPECT_FLOAT_EQ(closureAt(0).first, 0.0f);
	EXPECT_FLOAT_EQ(closureAt(16).first, 0.32f);
	EXPECT_FLOAT_EQ(closureAt(49).first, 0.98f);
	EXPECT_FLOAT_EQ(closureAt(50).first, 1.0f);
	EXPECT_FLOAT_EQ(closureAt(249).first, 1.0f);
	EXPECT_FLOAT_EQ(closureAt(250).first, 1.0f);
	EXPECT_FLOAT_EQ(closureAt(260).first, 0.8f);
	EXPECT_NEAR(closureAt(300).first, 0.0f, 1e-6f);
	// the time goes on by the frame's ms after the closure
	Timers timers;
	timers.blinking = true;
	timers.blinkMs = 30;
	Script script;
	Step(timers, shared, 16, script.Fn());
	EXPECT_EQ(timers.blinkMs, 46);
}

TEST(SuperVillagerEyes, BlinkEndsWithANewHold)
{
	Timers timers;
	timers.blinking = true;
	timers.holdMs = 200;
	timers.blinkMs = 301; // > hold + 100
	Shared shared;
	shared.glanceTarget = 1.0f;
	Script script {{150.7f}, {}};
	Step(timers, shared, 16, script.Fn());
	EXPECT_FALSE(timers.blinking);
	EXPECT_EQ(timers.holdMs, 150); // truncated
	EXPECT_EQ(timers.blinkMs, 0);
	EXPECT_FLOAT_EQ(timers.closure, 0.0f);
	ASSERT_EQ(script.asked.size(), 1u);
	EXPECT_EQ(script.asked[0], std::make_pair(100.0f, 200.0f));
	// the countdown did not run on that frame
	EXPECT_EQ(timers.untilBlinkMs, 0);
}

TEST(SuperVillagerEyes, Glance)
{
	Shared shared;
	shared.glanceTarget = 0.1f;
	Timers timers;
	Script script;
	Step(timers, shared, 16, script.Fn());
	EXPECT_FLOAT_EQ(timers.glance, (16.0f * 0.7f) * 0.001f); // 0.0112
	// clamped on the target, then a new one
	shared.glanceTarget = 0.005f;
	timers.glance = 0.0f;
	Step(timers, shared, 16, script.Fn());
	EXPECT_FLOAT_EQ(timers.glance, 0.005f);
	script.answers = {-0.2f};
	Step(timers, shared, 16, script.Fn());
	EXPECT_FLOAT_EQ(shared.glanceTarget, -0.2f);
	EXPECT_FLOAT_EQ(timers.glance, 0.005f);
	Step(timers, shared, 16, script.Fn());
	EXPECT_FLOAT_EQ(timers.glance, 0.005f - 0.0112f);
}

TEST(SuperVillagerEyes, SquintIsTheClosuresFloor)
{
	Timers timers;
	timers.untilBlinkMs = 1000;
	Shared shared;
	shared.glanceTarget = 1.0f;
	shared.squintMs = 290;
	Script script {{0.2f}, {}};
	Step(timers, shared, 16, script.Fn()); // 306 > 300
	EXPECT_EQ(shared.squintMs, 0u);        // reset to 0, not 306 - 300
	EXPECT_FLOAT_EQ(shared.squint, 0.2f);
	EXPECT_FLOAT_EQ(timers.closure, 0.2f);
	ASSERT_EQ(script.asked.size(), 1u);
	EXPECT_EQ(script.asked[0], std::make_pair(0.0f, 0.3f));
	// 300 itself does not draw (unsigned >)
	shared.squintMs = 284;
	Step(timers, shared, 16, script.Fn());
	EXPECT_EQ(shared.squintMs, 300u);
	EXPECT_EQ(script.asked.size(), 1u);
}

TEST(SuperVillagerEyes, LidAngles)
{
	const glm::mat4 eye(1.0f);
	const auto upper = Lid(eye, 1.0f, true);
	EXPECT_NEAR(upper[1].y, std::cos(0.47f), 1e-6f);
	EXPECT_NEAR(upper[1].z, -std::sin(0.47f), 1e-6f);
	EXPECT_NEAR(upper[2].y, std::sin(0.47f), 1e-6f);
	EXPECT_NEAR(upper[2].z, std::cos(0.47f), 1e-6f);
	EXPECT_EQ(upper[0], eye[0]);
	EXPECT_EQ(upper[3], eye[3]);
	const auto lower = Lid(eye, 1.0f, false);
	EXPECT_NEAR(lower[1].z, std::sin(0.35f), 1e-6f);
	EXPECT_NEAR(lower[2].y, -std::sin(0.35f), 1e-6f);
	const auto open = Lid(eye, 0.0f, true);
	EXPECT_EQ(open, eye);
}

TEST(SuperVillagerEyes, MirrorAndEyeMatrix)
{
	const auto r = Mirror();
	EXPECT_FLOAT_EQ(r[0][0], 1.0f);
	EXPECT_FLOAT_EQ(r[1][1], 1.0f);
	EXPECT_FLOAT_EQ(r[2][2], -1.0f);
	EXPECT_NEAR(r[0][2], 8.742278e-8f, 1e-12f);
	EXPECT_NEAR(r[2][0], 8.742278e-8f, 1e-12f);
	EXPECT_NEAR(glm::determinant(glm::mat3(r)), -1.0f, 1e-6f);
	// the man's right eye of nors_man.l3d: rows (0, 0, 1), (0.995, -0.096, 0), (0.096, 0.995, 0) and its position
	const std::array<float, 12> cells = {0.0f,   0.0f,   1.0f, 0.995f, -0.096f, 0.0f,
	                                     0.096f, 0.995f, 0.0f, 0.176f, 0.177f,  -0.055f};
	const auto e = EBoneMatrix(cells);
	EXPECT_EQ(glm::vec3(e[0]), glm::vec3(0.0f, 0.0f, 1.0f));
	EXPECT_EQ(glm::vec3(e[3]), glm::vec3(0.176f, 0.177f, -0.055f));
	// v R E bone: a point of the eye mesh is mirrored in z first
	const auto eye = EyeMatrix(glm::mat4(1.0f), cells);
	const glm::vec4 p = eye * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
	const glm::vec4 q = e * (r * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
	EXPECT_NEAR(p.x, q.x, 1e-6f);
	EXPECT_NEAR(p.y, q.y, 1e-6f);
	EXPECT_NEAR(p.z, q.z, 1e-6f);
}

TEST(SuperVillagerEyes, GlanceTurnsTheEyeballAboutItsY)
{
	const glm::mat4 eye(1.0f);
	EXPECT_EQ(Glance(eye, 0.0f), eye);
	const auto turned = Glance(eye, 0.5f);
	// r0' = c r0 + s r2, r2' = c r2 - s r0
	EXPECT_NEAR(turned[0].x, std::cos(0.5f), 1e-6f);
	EXPECT_NEAR(turned[0].z, std::sin(0.5f), 1e-6f);
	EXPECT_NEAR(turned[2].x, -std::sin(0.5f), 1e-6f);
	EXPECT_NEAR(turned[2].z, std::cos(0.5f), 1e-6f);
	EXPECT_EQ(turned[1], eye[1]);
}

TEST(SuperVillagerEyes, ShadeOfTheTable)
{
	// an identity eye matrix: the normal of the table against the default sun (-1, 1, -1) / sqrt 3 (with the
	// original's fast inverse square root and sum orders, worked out apart in float steps: the same integers as an
	// exact 1 / sqrt)
	const glm::mat3 identity(1.0f);
	EXPECT_EQ(ShadeIntensity(0, 0, identity), 147); // man, right (0, 0): (0, 0, -1)
	EXPECT_EQ(ShadeIntensity(0, 1, identity), 74);
	EXPECT_EQ(ShadeIntensity(1, 0, identity), 175);
	EXPECT_EQ(ShadeIntensity(1, 1, identity), 124);
	EXPECT_EQ(ShadeIntensity(2, 0, identity), 204);
	EXPECT_EQ(ShadeIntensity(2, 1, identity), 64);
	// turned half round about y, the man's right eye faces away from the sun's x and z
	glm::mat3 back(1.0f);
	back[0] = glm::vec3(-1.0f, 0.0f, 0.0f);
	back[2] = glm::vec3(0.0f, 0.0f, -1.0f);
	EXPECT_EQ(ShadeIntensity(0, 0, back), -147);
}

TEST(SuperVillagerEyes, FilesAndIris)
{
	EXPECT_EQ(File(0, 0), "r_paupe_up");
	EXPECT_EQ(File(1, 3), "l_paupe_down");
	EXPECT_EQ(File(2, 1), "r_paupe_down2");
	EXPECT_EQ(File(2, 4), "eye_ball2");
	EXPECT_FLOAT_EQ(IrisU(0), 0.5f);
	EXPECT_FLOAT_EQ(IrisU(1), 0.375f);
	EXPECT_FLOAT_EQ(IrisU(2), 0.625f);
}

namespace
{
void Put(std::vector<uint8_t>& file, size_t at, uint32_t value)
{
	std::memcpy(file.data() + at, &value, 4);
}
void PutF(std::vector<uint8_t>& file, size_t at, float value)
{
	std::memcpy(file.data() + at, &value, 4);
}
float GetF(const std::vector<uint8_t>& file, size_t at)
{
	float value = 0.0f;
	std::memcpy(&value, file.data() + at, 4);
	return value;
}
uint32_t Get(const std::vector<uint8_t>& file, size_t at)
{
	uint32_t value = 0;
	std::memcpy(&value, file.data() + at, 4);
	return value;
}

/// a header (76 bytes), one submesh, one primitive with skin 0x1234 and two vertices
std::vector<uint8_t> TinyL3D()
{
	std::vector<uint8_t> file(216, 0);
	std::memcpy(file.data(), "L3D0", 4);
	Put(file, 3 * 4, 1);           // submeshCount
	Put(file, 4 * 4, 76);          // submeshOffsetsOffset
	Put(file, 14 * 4, 0);          // skinCount
	Put(file, 15 * 4, 0xFFFFFFFF); // skinOffsetsOffset
	Put(file, 76, 80);             // submesh 0
	Put(file, 80 + 4, 1);          // numPrimitives
	Put(file, 80 + 8, 100);        // primitivesOffset
	Put(file, 100, 104);           // primitive 0
	Put(file, 104 + 8, 0x1234);    // material skinID
	Put(file, 104 + 16, 2);        // numVertices
	Put(file, 104 + 20, 152);      // verticesOffset
	PutF(file, 152 + 12, 0.25f);
	PutF(file, 152 + 16, 0.5f);
	PutF(file, 184 + 12, 0.1f);
	PutF(file, 184 + 16, 0.2f);
	return file;
}
} // namespace

TEST(SuperVillagerEyes, L3DPatches)
{
	auto file = TinyL3D();
	uint32_t skin = 0;
	ASSERT_TRUE(l3d_patch::FirstSkinId(file, skin));
	EXPECT_EQ(skin, 0x1234u);
	ASSERT_TRUE(l3d_patch::SetFirstSkinId(file, 0xFA71DD17u));
	ASSERT_TRUE(l3d_patch::FirstSkinId(file, skin));
	EXPECT_EQ(skin, 0xFA71DD17u);

	ASSERT_TRUE(l3d_patch::DoubleUvs(file));
	EXPECT_FLOAT_EQ(GetF(file, 152 + 12), 0.5f);
	EXPECT_FLOAT_EQ(GetF(file, 152 + 16), 1.0f);
	EXPECT_FLOAT_EQ(GetF(file, 184 + 12), 0.2f);
	EXPECT_FLOAT_EQ(GetF(file, 184 + 16), 0.4f);

	std::vector<uint8_t> texels(4 + 256 * 256 * 2, 0x5A);
	Put(texels, 0, 0xFA71DD17u);
	EXPECT_FALSE(l3d_patch::AppendSkin(file, std::span<const uint8_t>(texels.data(), 10)));
	ASSERT_TRUE(l3d_patch::AppendSkin(file, texels));
	EXPECT_EQ(Get(file, 14 * 4), 1u);
	EXPECT_EQ(Get(file, 15 * 4), 216u);
	EXPECT_EQ(Get(file, 216), 220u);
	EXPECT_EQ(l3d_patch::FindSkin(file, 0xFA71DD17u), texels);
	EXPECT_TRUE(l3d_patch::FindSkin(file, 0x1234u).empty());

	std::vector<uint8_t> empty;
	EXPECT_FALSE(l3d_patch::FirstSkinId(empty, skin));
	EXPECT_FALSE(l3d_patch::DoubleUvs(empty));
}

TEST(SuperVillagerEyes, RealCrtStream)
{
	// Random = rand (seed 1: 41, 18467, 6334, 26500, 19169) x about 1/32767 x (b - a) + a, the values worked out apart
	// in float steps; three frames of 16, 301 and 16 ms
	const openblack::game_random::testing::ScopedState state; // CRT seed 1
	const RandomFn crt = openblack::game_random::crt::Random;
	Timers timers;
	Shared shared;
	// frame 1: the countdown 0 - 16 < 0 starts a blink, the next one Random(1000, 5000) = 1005.005 away (truncated
	// 1005); the target equals the glance: Random(-0.25, 0.25)
	Step(timers, shared, 16, crt);
	EXPECT_TRUE(timers.blinking);
	EXPECT_EQ(timers.untilBlinkMs, 1005);
	EXPECT_FLOAT_EQ(shared.glanceTarget, 0.031792670488357544f);
	EXPECT_FLOAT_EQ(timers.glance, 0.0f);
	EXPECT_EQ(shared.squintMs, 16u);
	// frame 2: the blink at t = 0 (closure 0), the glance reaches the target (step 0.2107), the squint counter 317 > 300:
	// Random(0, 0.3), the closure's floor
	Step(timers, shared, 301, crt);
	EXPECT_EQ(timers.blinkMs, 301);
	EXPECT_FLOAT_EQ(timers.glance, 0.031792670488357544f);
	EXPECT_EQ(shared.squintMs, 0u);
	EXPECT_FLOAT_EQ(shared.squint, 0.05799127370119095f);
	EXPECT_FLOAT_EQ(timers.closure, 0.05799127370119095f);
	// frame 3: 301 > 200 + 100 ends the blink, the next hold Random(100, 200) = 180.874 (truncated 180); the glance is
	// on the target: a new one
	Step(timers, shared, 16, crt);
	EXPECT_FALSE(timers.blinking);
	EXPECT_EQ(timers.holdMs, 180);
	EXPECT_EQ(timers.blinkMs, 0);
	EXPECT_EQ(timers.untilBlinkMs, 1005);
	EXPECT_FLOAT_EQ(shared.glanceTarget, 0.04250466823577881f);
	EXPECT_FLOAT_EQ(timers.closure, 0.05799127370119095f);
	// five draws, the sixth is the CRT's next
	EXPECT_EQ(openblack::game_random::crt::Rand(), 15724);
}

namespace
{
namespace super_villager = openblack::ecs::super_villager;
using openblack::Locator;
using openblack::ecs::Registry;
using openblack::ecs::components::SuperVillager;
using openblack::ecs::components::Transform;

/// A registry only: the list's bookkeeping, no files, no meshes, no help system
class SuperVillagerList: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		openblack::test::EmplaceWorldSystems();
	}
	void TearDown() override
	{
		super_villager::ReleaseAll();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static entt::entity Host()
	{
		const auto host = Reg().Create();
		Reg().Assign<Transform>(host, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return host;
	}
	static Objects EyesOf(entt::entity host) { return Reg().Get<const SuperVillager>(host).eyes->objects; }
	static int LiveEyes(const Objects& objects)
	{
		int live = 0;
		for (const auto object : objects)
		{
			live += Reg().Valid(object) ? 1 : 0;
		}
		return live;
	}
};
} // namespace

TEST_F(SuperVillagerList, DestroyedHostTakesItsEyesInDraw)
{
	const auto host = Host();
	super_villager::testing::Adopt(host, 0);
	const auto eyes = EyesOf(host);
	EXPECT_EQ(LiveEyes(eyes), 6);
	ASSERT_EQ(super_villager::List().size(), 1u);
	// the host goes while it is a SuperVillager (ecs::ToBeDeleted, a script delete): its component (and the Eyes in it)
	// with it, but not the six eye entities
	Reg().Destroy(host);
	EXPECT_EQ(LiveEyes(eyes), 6);
	// the next draw finds it gone (as the driver's availability check would): Release deletes the Eyes whatever the
	// thing's state
	super_villager::Draw(16);
	EXPECT_EQ(LiveEyes(eyes), 0);
	EXPECT_TRUE(super_villager::List().empty());
}

TEST_F(SuperVillagerList, ReleaseOfAGoneHost)
{
	const auto host = Host();
	super_villager::testing::Adopt(host, 1);
	const auto eyes = EyesOf(host);
	Reg().Destroy(host);
	EXPECT_TRUE(super_villager::Release(host)); // still in the list: released, eyes deleted
	EXPECT_EQ(LiveEyes(eyes), 0);
	EXPECT_FALSE(super_villager::Release(host)); // not one any more
}

TEST_F(SuperVillagerList, ReleaseAllGoneAndAlive)
{
	const auto gone = Host();
	const auto alive = Host();
	super_villager::testing::Adopt(gone, 0);
	super_villager::testing::Adopt(alive, 2);
	// the list links at the head: the newest first
	ASSERT_EQ(super_villager::List(), (std::vector<entt::entity> {alive, gone}));
	const auto goneEyes = EyesOf(gone);
	const auto aliveEyes = EyesOf(alive);
	Reg().Destroy(gone);
	super_villager::ReleaseAll();
	EXPECT_EQ(LiveEyes(goneEyes), 0);
	EXPECT_EQ(LiveEyes(aliveEyes), 0);
	EXPECT_TRUE(Reg().Valid(alive));
	EXPECT_FALSE(Reg().AllOf<SuperVillager>(alive)); // restored
	EXPECT_TRUE(super_villager::List().empty());
}
