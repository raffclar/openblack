/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>

#include <gtest/gtest.h>

#include "Graphics/LandTexelFormat.h"

namespace graphics = openblack::graphics;

// A land texture in a 16-bit format gave a black screen on WebGPU: the land shaders could not read it there
TEST(LandTexelFormat, EveryBackEndReadsTheLandTextures)
{
	EXPECT_TRUE(graphics::SampledOnEveryBackEnd(graphics::k_LandTwoValueFormat));
	EXPECT_FALSE(graphics::SampledOnEveryBackEnd(graphics::TextureFormat::RG16));
	EXPECT_FALSE(graphics::SampledOnEveryBackEnd(graphics::TextureFormat::RG32F));
	EXPECT_FALSE(graphics::SampledOnEveryBackEnd(graphics::TextureFormat::R32F));
}

// The game writes pairs of uint16_t values; the shaders read four 8-bit channels and join each pair of bytes again
TEST(LandTexelFormat, EachValueComesBackExactly)
{
	for (const uint16_t height :
	     {uint16_t {0}, uint16_t {3}, uint16_t {255}, uint16_t {256}, uint16_t {4095}, uint16_t {65535}})
	{
		const std::array<uint16_t, 2> texel {height, 255};
		std::array<uint8_t, 4> bytes {};
		std::memcpy(bytes.data(), texel.data(), bytes.size());
		const auto channel = [&bytes](size_t i) { return static_cast<float>(bytes.at(i)) / 255.0f; };
		EXPECT_EQ(graphics::JoinLandTexelBytes(channel(0), channel(1)), static_cast<float>(height));
		EXPECT_EQ(graphics::JoinLandTexelBytes(channel(2), channel(3)), 255.0f);
	}
}
