/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "EngineConfig.h"

using openblack::GraphicsBackend;
using openblack::ParseGraphicsBackend;

namespace
{

TEST(GraphicsBackendNames, TheFullNamesStillWork)
{
	EXPECT_EQ(ParseGraphicsBackend("Direct3D12"), GraphicsBackend::Direct3D12);
	EXPECT_EQ(ParseGraphicsBackend("Vulkan"), GraphicsBackend::Vulkan);
	EXPECT_EQ(ParseGraphicsBackend("Metal"), GraphicsBackend::Metal);
	EXPECT_EQ(ParseGraphicsBackend("Noop"), GraphicsBackend::Noop);
}

TEST(GraphicsBackendNames, ShortNamesInAnyCase)
{
	EXPECT_EQ(ParseGraphicsBackend("D3D12"), GraphicsBackend::Direct3D12);
	EXPECT_EQ(ParseGraphicsBackend("d3d12"), GraphicsBackend::Direct3D12);
	EXPECT_EQ(ParseGraphicsBackend("DX12"), GraphicsBackend::Direct3D12);
	EXPECT_EQ(ParseGraphicsBackend("direct3d12"), GraphicsBackend::Direct3D12);
	EXPECT_EQ(ParseGraphicsBackend("vk"), GraphicsBackend::Vulkan);
	EXPECT_EQ(ParseGraphicsBackend("VULKAN"), GraphicsBackend::Vulkan);
	EXPECT_EQ(ParseGraphicsBackend("mtl"), GraphicsBackend::Metal);
	EXPECT_EQ(ParseGraphicsBackend("null"), GraphicsBackend::Noop);
}

TEST(GraphicsBackendNames, UnknownNamesAreRefused)
{
	EXPECT_FALSE(ParseGraphicsBackend("").has_value());
	EXPECT_FALSE(ParseGraphicsBackend("D3D11").has_value());
	EXPECT_FALSE(ParseGraphicsBackend("Vulkan2").has_value());
	EXPECT_FALSE(ParseGraphicsBackend("OpenGL").has_value());
}

} // namespace
