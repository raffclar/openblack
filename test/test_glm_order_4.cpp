/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Bit tests for game maths that sits inside larger functions of the game, so the oracle cannot call it alone. The
// expected bits were worked out by hand from the game's instructions, one float step at a time.

#include <cstdint>

#include <array>
#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/ParticleBlast.h"
#include "Particles/ParticleMiracleMaths.h"

using namespace openblack::particles;

namespace
{
[[nodiscard]] float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

[[nodiscard]] glm::vec3 V(const uint32_t* bits)
{
	return {F(bits[0]), F(bits[1]), F(bits[2])};
}

void ExpectBits(const glm::vec3& ours, const uint32_t* game, size_t row)
{
	for (glm::length_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(ours[i]), game[i]) << "row " << row << " part " << i;
	}
}

struct Case10
{
	std::array<uint32_t, 10> in;
	std::array<uint32_t, 3> out;
};

struct Case7
{
	std::array<uint32_t, 7> in;
	std::array<uint32_t, 4> out;
};

struct Case11
{
	std::array<uint32_t, 11> in;
	std::array<uint32_t, 3> out;
};
} // namespace

TEST(GlmOrder4, AFireballBouncesOffASlopeAsTheGameDoes)
{
	// The velocity, the slope's normal, the drag, the step, the bounce across and the bounce up; the velocity after
	constexpr std::array<Case10, 12> k_Cases {{
	    {{0x41fe5245u, 0xc244e774u, 0x41c703a7u, 0xbdc21a39u, 0x3f6d32afu, 0x3eba616eu, 0x41c6390au, 0x3d4ccccdu, 0x3f7a534du,
	      0x3f173e3du},
	     {0x41c42cc0u, 0x411adce6u, 0x4237dc4cu}},
	    {{0x421c2093u, 0xc09d7443u, 0x41db1a42u, 0xbeb1a898u, 0x3f4bed78u, 0x3efd7285u, 0x404600dfu, 0x3d4ccccdu, 0x3eab31f3u,
	      0x3ee3191au},
	     {0x413f4af5u, 0x3f46f107u, 0x412a13e7u}},
	    {{0xc1bcd5fau, 0xc0c41a52u, 0x420d46ffu, 0x3e803e7fu, 0x3f754475u, 0xbe0e7353u, 0x41a863d3u, 0x3d4ccccdu, 0x3f356503u,
	      0x3f425751u},
	     {0xc12388b3u, 0x41979af1u, 0x41a7f572u}},
	    {{0xbe837f1fu, 0xc20b3e58u, 0xc1805f34u, 0x3f1acacbu, 0x3f36d961u, 0xbeb4761cu, 0x41a23094u, 0x3d4ccccdu, 0x3f2b83c7u,
	      0x3eeb8f80u},
	     {0x414d2e19u, 0xc0e8689bu, 0xc18ff344u}},
	    {{0xc1b21b0fu, 0xc22410a9u, 0xbe626955u, 0x3f0512a0u, 0x3f54cbb4u, 0x3e49d113u, 0x40622c6au, 0x3dcccccdu, 0x3f7ac550u,
	      0x3e8ecb6cu},
	     {0x41009f75u, 0x40f7fb96u, 0x412cc321u}},
	    {{0xc02e26e5u, 0xc25c5c68u, 0xc09abbd8u, 0xbef1915bu, 0x3f4cad8cu, 0xbebe4e9fu, 0x41c2584bu, 0x3dcccccdu, 0x3ebc4c7fu,
	      0x3f0509aeu},
	     {0xc18cfe80u, 0x4115756eu, 0xc16cf4dcu}},
	    {{0xc167401au, 0xc1c836fdu, 0x416fadfau, 0x3efbd316u, 0x3f265a17u, 0xbf145ad3u, 0x41a26d1eu, 0x3d072b02u, 0x3ec8a8a4u,
	      0x3eeca860u},
	     {0x40f7b0a9u, 0x4102cfe7u, 0xc11d40f7u}},
	    {{0x4215f639u, 0xc0b93514u, 0xc17f65d9u, 0xbe27d2cbu, 0x3f79243au, 0xbe2520edu, 0x41a746deu, 0x3d4ccccdu, 0x3f110284u,
	      0x3ef08789u},
	     {0x4199231fu, 0x40be9489u, 0xc125271au}},
	    {{0x4205dd63u, 0xc1f289e0u, 0x421cec39u, 0xbeb8e282u, 0x3f6bfbc2u, 0x3e10586au, 0x413a5679u, 0x3d4ccccdu, 0x3f244594u,
	      0x3ef885b3u},
	     {0x40e8b6beu, 0x41830a28u, 0x41f28b78u}},
	    {{0x41f528e3u, 0xc0ff9569u, 0x41ea29bbu, 0x3ed035fbu, 0x3f5db712u, 0xbe94de5fu, 0x41153ce6u, 0x3d072b02u, 0x3f126ccfu,
	      0x3f3c41d0u},
	     {0x4197cbd2u, 0xbf9812dfu, 0x4177f8bcu}},
	    {{0x40e2f893u, 0xc21c8789u, 0xc206b155u, 0xbca1b6afu, 0x3f621d42u, 0xbeefdb46u, 0x40f6e04bu, 0x3dcccccdu, 0x3f4e0eebu,
	      0x3f631519u},
	     {0x409fb5fcu, 0xc03b1514u, 0xc226414cu}},
	    {{0x41c3ff10u, 0xc252c405u, 0xc1515f74u, 0xbe2fb3c7u, 0x3f5ffeebu, 0xbee7cbfbu, 0x41e01daeu, 0x3d4ccccdu, 0x3f050889u,
	      0x3ea3a16au},
	     {0x40c0fbd3u, 0x40aedce7u, 0xc1b8726bu}},
	}};
	for (size_t row = 0; row < k_Cases.size(); ++row)
	{
		const auto& c = k_Cases.at(row);
		const auto ours = maths::BounceOffSlope(V(&c.in[0]), V(&c.in[3]), F(c.in[6]), F(c.in[7]), F(c.in[8]), F(c.in[9]));
		ExpectBits(ours, c.out.data(), row);
	}
}

TEST(GlmOrder4, AnotherCastersFireballIsLobbedAsTheGameDoes)
{
	// The start, the target and the gravity; the direction and the speed
	constexpr std::array<Case7, 12> k_Cases {{
	    {{0x436d6f9bu, 0x4183637du, 0x4374825cu, 0x42e6c233u, 0x41a871dau, 0x434290e7u, 0x41a00000u},
	     {0xbf4d2d5fu, 0x3f0005f2u, 0xbea7e8b4u, 0x424bb95eu}},
	    {{0x4366f94au, 0x4245f98eu, 0xc366c734u, 0x43944cb7u, 0x41af63d7u, 0xc3007cd6u, 0x42237516u},
	     {0x3eef6ae9u, 0x3f0005efu, 0x3f3a970fu, 0x42658d8au}},
	    {{0x438e397eu, 0x42453d30u, 0x43076a57u, 0x439d2fcfu, 0x426702dfu, 0x4283d6d1u, 0x41a00000u},
	     {0x3eb5d703u, 0x3ee140fbu, 0xbf5326a7u, 0x42322b50u}},
	    {{0xc369178fu, 0x3f9b4253u, 0xc27a7d5au, 0xc3a02b8cu, 0x40b2a471u, 0xc2849f98u, 0x41f00000u},
	     {0xbf5d7dbeu, 0x3f0005f2u, 0xbd15da21u, 0x424de2d2u}},
	    {{0xc22c6353u, 0x4243905bu, 0xc39233a5u, 0x42840ed9u, 0x4261d51eu, 0xc3d6eddcu, 0x41a00000u},
	     {0x3f09d781u, 0x3f0005f2u, 0xbf2da00au, 0x426cc4bbu}},
	    {{0xc32ce60eu, 0x42073b85u, 0x41f62af6u, 0x411645fbu, 0x41cb86bfu, 0xc30dea58u, 0x419f848au},
	     {0x3f20f0b4u, 0x3f0005f2u, 0xbf1875f6u, 0x42843f06u}},
	    {{0xc385b7c9u, 0x41fcbfc5u, 0xc20954abu, 0xc2a1dc08u, 0x425f4716u, 0x41a0b0c0u, 0x4202e85au},
	     {0x3f54d0a3u, 0x3f0005f2u, 0x3e7861b9u, 0x42ad1740u}},
	    {{0x438b4466u, 0x424e0dc0u, 0x433cb96cu, 0x43d09877u, 0x4282fd08u, 0x439f11ffu, 0x4214d7ecu},
	     {0x3f2210a8u, 0x3f0005f1u, 0x3f1743bbu, 0x42ace3b9u}},
	    {{0xc125de70u, 0x41f8d80du, 0xc3860b94u, 0x42e57cb9u, 0x40f8bdd0u, 0xc38202e2u, 0x41f00000u},
	     {0x3f5d3ac7u, 0x3f0005f0u, 0x3d6442dbu, 0x424d06f3u}},
	    {{0xc33c21f3u, 0x4223b9b3u, 0x4367a4b6u, 0xc342b773u, 0x40f75d7cu, 0x43a67e71u, 0x41f00000u},
	     {0xbd7385b8u, 0x3ecc2444u, 0x3f6a4703u, 0x422e77bbu}},
	    {{0x438b6df4u, 0x4231a596u, 0xc183b804u, 0x43d5aa19u, 0x4188205au, 0xc26b1257u, 0x41f179acu},
	     {0x3f553424u, 0x3f0005f1u, 0xbe72fd65u, 0x42658401u}},
	    {{0x42355a7bu, 0x422cc0dbu, 0x43216a1eu, 0xc2f6ad0au, 0x41e3e518u, 0x42a53ab1u, 0x41a00000u},
	     {0xbf48da3bu, 0x3f0005f2u, 0xbebba9bau, 0x425c04a6u}},
	}};
	for (size_t row = 0; row < k_Cases.size(); ++row)
	{
		const auto& c = k_Cases.at(row);
		const auto launch = maths::Lob(V(&c.in[0]), V(&c.in[3]), F(c.in[6]));
		ExpectBits(launch.direction, c.out.data(), row);
		EXPECT_EQ(std::bit_cast<uint32_t>(launch.speed), c.out[3]) << "row " << row;
	}
}

TEST(GlmOrder4, ABrokenPieceFliesOutAsTheGameThrowsIt)
{
	// The piece, the origin, the speed, the random factor and the random unit; the velocity
	constexpr std::array<Case11, 12> k_Cases {{
	    {{0x42061fc5u, 0x41f47baeu, 0x41f1f204u, 0xc2452139u, 0xc169c2e1u, 0xc21d1d1cu, 0x429e5df6u, 0x3ec5dacfu, 0xbe836e99u,
	      0xbe8a97ceu, 0x3f6d84efu},
	     {0x42406b0fu, 0x41d30d6cu, 0x4296bc75u}},
	    {{0xc1808261u, 0x41738969u, 0xbfa4a30du, 0xc1f282f1u, 0x42260b4cu, 0x40d834d3u, 0x411b94fdu, 0x3e6d8d3cu, 0xbf7a01d9u,
	      0x3e551cdcu, 0xbd5ec0d7u},
	     {0x40116539u, 0xc10054b5u, 0xc0297be2u}},
	    {{0x408c7b1fu, 0xc245dbb5u, 0xc1bdc35fu, 0xc1eb25b9u, 0x420a5cbau, 0xc013ed9du, 0x413ce962u, 0x3fb64988u, 0x3f33c91fu,
	      0xbefed85au, 0xbf024b5bu},
	     {0x4180bffau, 0xc16d8a22u, 0xc1346036u}},
	    {{0xc2148e92u, 0xc22b6a75u, 0x41209011u, 0xc21ac612u, 0x4221e139u, 0xc2113408u, 0x41ffb8abu, 0x3fa028d5u, 0xbf160087u,
	      0xbe2ef3beu, 0x3f4ac959u},
	     {0xc1b7512eu, 0xc1facb27u, 0x423cdcbcu}},
	    {{0x42284d84u, 0xc062cea9u, 0x42100f8au, 0x4240dfa5u, 0x41df40f0u, 0xc0a1a7a0u, 0x42914dc9u, 0x3ddebd6eu, 0x3f1bb256u,
	      0xbf2ce34cu, 0x3ed5950du},
	     {0xc070c158u, 0xc23a2255u, 0x42724b98u}},
	    {{0x41ccabb5u, 0x41bcf299u, 0x41885cbeu, 0xc1bb47d8u, 0x41e0b209u, 0xc239cad4u, 0x4177615eu, 0x3d3c9063u, 0xbf61620bu,
	      0x3ec1a692u, 0x3e927854u},
	     {0x410cddb9u, 0xbf39bc12u, 0x4146ce5eu}},
	    {{0xc19dcb68u, 0x419c8b67u, 0x42365f2fu, 0x41c26d94u, 0xc166b44au, 0x41e5d84cu, 0x421b2597u, 0x3eea9fc5u, 0x3ec6469bu,
	      0xbf23b139u, 0x3f2a096cu},
	     {0xc1b3fe5eu, 0x4187fdf2u, 0x41b8787cu}},
	    {{0xc2392ab1u, 0xc233c2e7u, 0xc1a960d7u, 0xc1d41948u, 0x4013e5efu, 0x4156e932u, 0x41bd37f4u, 0x3f98a7f3u, 0x3f7130a9u,
	      0x3ea80ae4u, 0x3d8b6ed9u},
	     {0x4198117eu, 0xc1573371u, 0xc1351d75u}},
	    {{0xc03a064fu, 0x42404dc5u, 0xc0dd21f6u, 0xc21aa9feu, 0xc1a40805u, 0xc12f19c1u, 0x41a48797u, 0x3faadf8au, 0xbea44575u,
	      0xbf5759fau, 0xbeded4e1u},
	     {0x3f2feb50u, 0x40d53be4u, 0xc12e0b49u}},
	    {{0x422d1b0fu, 0xc188f011u, 0xc1e7385eu, 0xc10dda20u, 0x41ebe19bu, 0x4235cb0du, 0x411fe6e7u, 0x3fb46e65u, 0x3eb3c4e0u,
	      0xbf5e2fd3u, 0x3eb3e2f3u},
	     {0x4120d32eu, 0xc12ad200u, 0xc01521a6u}},
	    {{0x41ed17abu, 0xc1500dbeu, 0x41845fddu, 0x3f21f5b0u, 0x41abe86cu, 0x410688feu, 0x411bebe4u, 0x3f8567d5u, 0xbf541b76u,
	      0x3ec32cf7u, 0xbed1fb6fu},
	     {0xc00f9078u, 0xc0aced87u, 0xc01bc04au}},
	    {{0xc01d130eu, 0xc11d0ca3u, 0xc226d1e1u, 0xc247fcf5u, 0x414cf72eu, 0xc0958e39u, 0x421be433u, 0x3fa34c40u, 0xbf313423u,
	      0x3e876a9fu, 0xbf2be708u},
	     {0xc0b43834u, 0xc0e3f4dcu, 0xc25f3aa6u}},
	}};
	for (size_t row = 0; row < k_Cases.size(); ++row)
	{
		const auto& c = k_Cases.at(row);
		const auto ours = blast::FragmentVelocity(V(&c.in[0]), V(&c.in[3]), F(c.in[6]), F(c.in[7]), V(&c.in[8]));
		ExpectBits(ours, c.out.data(), row);
	}
}
