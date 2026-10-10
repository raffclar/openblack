/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <set>
#include <vector>

#include <bgfx/defines.h>
#include <gtest/gtest.h>

#include "Graphics/DrawOrder.h"

using namespace openblack::graphics;

namespace
{

constexpr uint64_t k_Opaque = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;

/// The descriptor sets a run of draws costs on Vulkan: one each time a draw binds other textures than the one before
std::size_t BindingChanges(const std::vector<draw_order::Bindings>& draws)
{
	std::size_t changes = 0;
	for (std::size_t i = 0; i < draws.size(); ++i)
	{
		if (i == 0 || draw_order::GroupKey(draws[i]) != draw_order::GroupKey(draws[i - 1]))
		{
			++changes;
		}
	}
	return changes;
}

} // namespace

TEST(DrawOrder, OpaqueSceneDrawsAreReorderable)
{
	EXPECT_TRUE(draw_order::Reorderable(RenderPass::Main, k_Opaque, 0));
	EXPECT_TRUE(draw_order::Reorderable(RenderPass::Reflection, k_Opaque, 0));
}

TEST(DrawOrder, EveryShadowCasterIsReorderable)
{
	// The casters write the same value with neither depth nor blending
	EXPECT_TRUE(draw_order::Reorderable(RenderPass::ObjectShadow, BGFX_STATE_WRITE_R, 0));
}

TEST(DrawOrder, DrawsWhoseOrderShowsKeepIt)
{
	// Blended
	EXPECT_FALSE(draw_order::Reorderable(RenderPass::Main, k_Opaque | BGFX_STATE_BLEND_ALPHA, 0));
	// Without writing depth, a farther draw after it would cover it
	EXPECT_FALSE(draw_order::Reorderable(RenderPass::Main, k_Opaque & ~BGFX_STATE_WRITE_Z, 0));
	// Drawn over what has the same depth
	EXPECT_FALSE(
	    draw_order::Reorderable(RenderPass::Main, (k_Opaque & ~BGFX_STATE_DEPTH_TEST_MASK) | BGFX_STATE_DEPTH_TEST_GEQUAL, 0));
	// Given a depth to be sorted by
	EXPECT_FALSE(draw_order::Reorderable(RenderPass::Main, k_Opaque, 42));
	// In views sorted by depth or kept in order
	EXPECT_FALSE(draw_order::Reorderable(RenderPass::Translucent, k_Opaque, 0));
	EXPECT_FALSE(draw_order::Reorderable(RenderPass::ReflectionTranslucent, k_Opaque, 0));
	EXPECT_FALSE(draw_order::Reorderable(RenderPass::Sky, k_Opaque, 0));
	EXPECT_FALSE(draw_order::Reorderable(RenderPass::Interface, k_Opaque, 0));
}

TEST(DrawOrder, GroupKeyIsNeverTheKeptOrdersDepth)
{
	EXPECT_NE(draw_order::GroupKey({}), 0u);
	EXPECT_NE(draw_order::GroupKey({.diffuse = 0, .lightmap = UINT16_MAX, .snowed = false}), 0u);
}

TEST(DrawOrder, GroupKeyTellsBindingsApart)
{
	const std::array<draw_order::Bindings, 6> bindings {{
	    {.diffuse = 0, .lightmap = UINT16_MAX, .snowed = false},
	    {.diffuse = 1, .lightmap = UINT16_MAX, .snowed = false},
	    {.diffuse = 1, .lightmap = 0, .snowed = false},
	    {.diffuse = 1, .lightmap = 0, .snowed = true},
	    {.diffuse = 4095, .lightmap = 4095, .snowed = true},
	    {.diffuse = UINT16_MAX, .lightmap = UINT16_MAX, .snowed = false},
	}};
	std::set<uint32_t> keys;
	for (const auto& binding : bindings)
	{
		keys.insert(draw_order::GroupKey(binding));
	}
	EXPECT_EQ(keys.size(), bindings.size());
}

TEST(DrawOrder, SortingByGroupKeyBindsEachTextureOnce)
{
	// Models drawn in the order they come, sharing a handful of textures
	std::vector<draw_order::Bindings> draws;
	for (uint16_t model = 0; model < 60; ++model)
	{
		draws.push_back({.diffuse = static_cast<uint16_t>(model % 5), .lightmap = UINT16_MAX, .snowed = model % 2 == 0});
		draws.push_back({.diffuse = static_cast<uint16_t>(7 + model % 3), .lightmap = UINT16_MAX, .snowed = false});
	}
	EXPECT_GT(BindingChanges(draws), 100u);
	// bgfx sorts a program's draws by their depth, keeping the order they came in where it is the same
	std::ranges::stable_sort(draws, {}, [](const draw_order::Bindings& draw) { return draw_order::GroupKey(draw); });
	EXPECT_EQ(BindingChanges(draws), 13u);
}
