/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/TempleCameraModel.h"
#include "Common/X87Vector.h"
#include "Hand/HandGrabRules.h"
#include "Magic/HandHoldPose.h"
#include "Particles/ParticleMaths.h"

using namespace openblack;

namespace
{

using Bits3 = std::array<uint32_t, 3>;

glm::vec3 Vec(const Bits3& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}

void ExpectBits(const glm::vec3& ours, const Bits3& game)
{
	for (int part = 0; part < 3; ++part)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(ours[part]), game.at(static_cast<size_t>(part))) << "part " << part;
	}
}

/// The camera, the hand, whether the hand holds a thing, and the distance
struct DistanceCase
{
	Bits3 camera;
	Bits3 hand;
	bool holding;
	uint32_t distance;
};

// Worked in floats in the game's order: the squares added x, y, z while holding and x, z, y with nothing held. Each
// case gives other bits in the other order.
constexpr std::array<DistanceCase, 5> k_DistanceCases {{
    {{0xC4DBAA50u, 0x440DCA7Bu, 0x441320FDu}, {0xC4C302AFu, 0x442DFBC1u, 0x43FB2D44u}, true, 0x437AD0D3u},
    {{0xC4B95474u, 0x435523BBu, 0xC3DA19C1u}, {0xC4A6C240u, 0x42357D04u, 0xC3E4435Cu}, false, 0x436103D6u},
    {{0xC46D40DEu, 0x41B98B49u, 0xC3A21B64u}, {0xC47A53FAu, 0x4246EB02u, 0xC30EF98Eu}, false, 0x433E7DCAu},
    {{0xC4919E50u, 0x431298B3u, 0xC41FF244u}, {0xC4A7FD58u, 0xC2553DA8u, 0xC442D1E2u}, true, 0x43973467u},
    {{0x44443261u, 0x435FAB7Cu, 0xC4054CDAu}, {0x4422E6A8u, 0x43A638F6u, 0xC4020A7Cu}, false, 0x432C73C1u},
}};

/// An axis, the size, and the first axis at that size
struct AxisCase
{
	Bits3 axis;
	uint32_t size;
	Bits3 sized;
};

// Worked in floats in the game's order for the first axis: the squares added z, x, then y. Each case gives other bits
// when they are added z, y, then x.
constexpr std::array<AxisCase, 4> k_FirstAxisCases {{
    {{0xBF82D13Au, 0xBFD4B439u, 0x3FEF3D9Au}, 0x407C6E35u, {0xBFBEFA9Bu, 0xC01B4325u, 0x402EA1F0u}},
    {{0xBFD1DD34u, 0x3DD8BEA1u, 0xBF5DDB1Eu}, 0x3ED08000u, {0xBEB808D1u, 0x3CBE1173u, 0xBE428CE4u}},
    {{0x402EB862u, 0xBF4FEB56u, 0xBFD6AF58u}, 0x3F91D58Du, {0x3F70E547u, 0xBE8F559Bu, 0xBF13FF9Cu}},
    {{0xC01F71F0u, 0x3F76A8D9u, 0x401D5ABBu}, 0x404C7307u, {0xC00C4CC8u, 0x3F590AD1u, 0x400A75D6u}},
}};

/// A frame (side, up, forward), a turn about the up, and the turned unit frame
struct SeedTurnCase
{
	Bits3 x;
	Bits3 up;
	Bits3 z;
	uint32_t turn;
	Bits3 turnedX;
	Bits3 turnedUp;
	Bits3 turnedZ;
};

// Worked in floats in the game's order: the cosine stored as a float, each product with the wide sine rounded one
// time; the first axis made unit with its squares added z, x, y and the other two z, y, x. Each case gives other bits
// with a float sine and glm's order.
constexpr std::array<SeedTurnCase, 4> k_SeedTurnCases {{
    {{0x3F1CFB05u, 0xBD5C9361u, 0xBF49BFFFu},
     {0x3F24A167u, 0xBEBEB9C4u, 0x3F2B491Bu},
     {0x3F74ECAEu, 0xBE585BA7u, 0xBE4CD558u},
     0x402B91EFu,
     {0xBE48F382u, 0xBD946A2Cu, 0x3F7A55F4u},
     {0x3F24A167u, 0xBEBEB9C4u, 0x3F2B491Bu},
     {0xBF647A48u, 0x3E2C7BDCu, 0x3ED63D11u}},
    {{0xBEBEEE5Au, 0x3DF86D62u, 0xBF6B7EC9u},
     {0xBF337878u, 0x3F2DFA50u, 0x3E5D32DAu},
     {0x3D77C0FCu, 0x3F7C9AF4u, 0xBE1A3C4Bu},
     0x400EBFD1u,
     {0x3EA10E5Bu, 0x3F4D8ED5u, 0x3F019A99u},
     {0xBF337878u, 0x3F2DFA50u, 0x3E5D32DAu},
     {0x3E6E0438u, 0xBF21D340u, 0x3F3D3ACBu}},
    {{0x3F26818Fu, 0xBF13842Bu, 0xBEFD61ECu},
     {0xBF1A8469u, 0xBF41A551u, 0x3E8105E3u},
     {0xBF098D2Cu, 0xBE392CF3u, 0xBF52E28Eu},
     0x401D7251u,
     {0xBF6B7BFDu, 0x3EBA3E97u, 0xBE164618u},
     {0xBF1A846Au, 0xBF41A553u, 0x3E8105E4u},
     {0x3BE6419Eu, 0x3EEF708Au, 0x3F624626u}},
    {{0xBDCF3F18u, 0xBF091B9Eu, 0xBF56A173u},
     {0x3F2C4EF9u, 0xBF3CB69Bu, 0xBD74450Du},
     {0x3F46B143u, 0x3E475174u, 0xBF198A12u},
     0x3DE1780Fu,
     {0xBC72760Fu, 0xBEFCEA69u, 0xBF5E8DB3u},
     {0x3F2C4EF9u, 0xBF3CB69Bu, 0xBD74450Du},
     {0x3F4FBA4Au, 0x3E85F34Eu, 0xBF05CA06u}},
}};

/// Where a dome patch's parent was and is: the share of the two lengths rounds past 1 and the axis is not zero
struct DomeCase
{
	Bits3 last;
	Bits3 now;
};

constexpr std::array<DomeCase, 3> k_DomeCases {{
    {{0xC144CC7Eu, 0x42435AE7u, 0x4237ED25u}, {0xC146085Du, 0x42449475u, 0x4239145Bu}},
    {{0x4228EEECu, 0xC20302C3u, 0x4212C9E1u}, {0x4229B171u, 0xC203999Eu, 0x421372E6u}},
    {{0xC1EC81CBu, 0x41AE6985u, 0xC1499520u}, {0xC1ED5605u, 0x41AF0607u, 0xC14A4A03u}},
}};

/// The pose a room turns to walk back through each door of the main room: its place and its focus
struct DoorBackCase
{
	Bits3 origin;
	Bits3 focus;
};

// Worked in floats in the game's order from the pose (40, 16, 0) looking at (20, 10, 0): the cosine stored as a float,
// each product with the wide sine rounded one time. Door 5 gives other bits with a float sine.
constexpr std::array<DoorBackCase, 8> k_DoorBackCases {{
    {{0x42200000u, 0x41800000u, 0x00000000u}, {0x41A00000u, 0x41200000u, 0x00000000u}},
    {{0x41E24630u, 0x41800000u, 0x41E24630u}, {0x41624630u, 0x41200000u, 0x41624630u}},
    {{0xB5EAAC7Au, 0x41800000u, 0x42200000u}, {0xB56AAC7Au, 0x41200000u, 0x41A00000u}},
    {{0xC1E24630u, 0x41800000u, 0x41E24630u}, {0xC1624630u, 0x41200000u, 0x41624630u}},
    {{0xC2200000u, 0x41800000u, 0xB66AAC7Au}, {0xC1A00000u, 0x41200000u, 0xB5EAAC7Au}},
    {{0xC1E2462Du, 0x41800000u, 0xC1E24633u}, {0xC162462Du, 0x41200000u, 0xC1624633u}},
    {{0x35000ADDu, 0x41800000u, 0xC2200000u}, {0x34800ADDu, 0x41200000u, 0xC1A00000u}},
    {{0x41E24635u, 0x41800000u, 0xC1E2462Bu}, {0x41624635u, 0x41200000u, 0xC162462Bu}},
}};

} // namespace

TEST(GlmOrder7, TheWayBackThroughADoor)
{
	const TempleCameraModel::Pose pose {{40.0f, 16.0f, 0.0f}, {20.0f, 10.0f, 0.0f}};
	for (uint32_t door = 0; door < k_DoorBackCases.size(); ++door)
	{
		const auto turned = TempleCameraModel::TurnToDoorBack(pose, door);
		ExpectBits(turned.origin, k_DoorBackCases.at(door).origin);
		ExpectBits(turned.focus, k_DoorBackCases.at(door).focus);
	}
}

TEST(GlmOrder7, TheHandsDistanceFromTheCamera)
{
	for (const auto& c : k_DistanceCases)
	{
		const float distance = magic::hand_hold::DistanceFromView(Vec(c.camera), Vec(c.hand), c.holding);
		EXPECT_EQ(std::bit_cast<uint32_t>(distance), c.distance);
	}
	EXPECT_EQ(magic::hand_hold::DistanceFromView(glm::vec3(4.0f), glm::vec3(4.0f), false), 0.0f);
}

TEST(GlmOrder7, AHeldThingsFirstAxisAtItsSize)
{
	for (const auto& c : k_FirstAxisCases)
	{
		const glm::mat3 axes(Vec(c.axis), glm::vec3(0.0f), glm::vec3(0.0f));
		const auto sized = hand_grab::AxesAtSize(axes, std::bit_cast<float>(c.size));
		ExpectBits(sized[0], c.sized);
		EXPECT_EQ(sized[1], glm::vec3(0.0f));
	}
}

TEST(GlmOrder7, AHeldThingTurnedAboutItsUp)
{
	for (const auto& c : k_SeedTurnCases)
	{
		const glm::mat3 basis(Vec(c.x), Vec(c.up), Vec(c.z));
		const auto turned = magic::hand_hold::SeedTurn(basis, std::bit_cast<float>(c.turn));
		ExpectBits(turned[0], c.turnedX);
		ExpectBits(turned[1], c.turnedUp);
		ExpectBits(turned[2], c.turnedZ);
	}
}

TEST(GlmOrder7, ADomePatchTurnStaysANumberWhenTheShareRoundsPastOne)
{
	const glm::mat3 rotation(glm::vec3(0.6f, 0.0f, -0.8f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.8f, 0.0f, 0.6f));
	for (const auto& c : k_DomeCases)
	{
		const auto last = Vec(c.last);
		const auto now = Vec(c.now);
		// The share in the game's order is past 1, where the game's arccosine gives not a number
		const float lengths = std::sqrt(x87::DotZyx(now, now) * x87::DotZyx(last, last));
		ASSERT_GT(x87::Dot<x87::SumOrder::Yzx>(last, now) / lengths, 1.0f);
		// The share is held at 1: a turn by no angle leaves the rows as they are
		const auto turned = particles::maths::TurnWithMove(rotation, last, now);
		for (glm::length_t i = 0; i < 3; ++i)
		{
			for (glm::length_t k = 0; k < 3; ++k)
			{
				EXPECT_FALSE(std::isnan(turned[i][k]));
				EXPECT_EQ(turned[i][k], rotation[i][k]);
			}
		}
	}
}
