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

TEST(GraphicsBackendNames, WebGpuByItsNames)
{
	EXPECT_EQ(ParseGraphicsBackend("WebGPU"), GraphicsBackend::WebGPU);
	EXPECT_EQ(ParseGraphicsBackend("webgpu"), GraphicsBackend::WebGPU);
	EXPECT_EQ(ParseGraphicsBackend("WGPU"), GraphicsBackend::WebGPU);
}

TEST(GraphicsBackendNames, UnknownNamesAreRefused)
{
	EXPECT_FALSE(ParseGraphicsBackend("").has_value());
	EXPECT_FALSE(ParseGraphicsBackend("D3D10").has_value());
	EXPECT_FALSE(ParseGraphicsBackend("Vulkan2").has_value());
	EXPECT_FALSE(ParseGraphicsBackend("OpenGL2").has_value());
}

#if defined(_WIN32)
TEST(GraphicsBackendNames, Direct3D11ByItsNames)
{
	EXPECT_EQ(ParseGraphicsBackend("Direct3D11"), GraphicsBackend::Direct3D11);
	EXPECT_EQ(ParseGraphicsBackend("D3D11"), GraphicsBackend::Direct3D11);
	EXPECT_EQ(ParseGraphicsBackend("dx11"), GraphicsBackend::Direct3D11);
	EXPECT_EQ(ParseGraphicsBackend("DirectX11"), GraphicsBackend::Direct3D11);
}
#endif

TEST(GraphicsBackendNames, OpenGlByItsNames)
{
	EXPECT_EQ(ParseGraphicsBackend("OpenGL"), GraphicsBackend::OpenGL);
	EXPECT_EQ(ParseGraphicsBackend("opengl"), GraphicsBackend::OpenGL);
	EXPECT_EQ(ParseGraphicsBackend("GL"), GraphicsBackend::OpenGL);
}

} // namespace
