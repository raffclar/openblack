/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <span>

#include <gtest/gtest.h>

#include "Graphics/BoneBudget.h"
#include "Graphics/ShaderSamplers.h"

// The object shaders as the game embeds them, compiled for Vulkan
#include "generated/shaders/spirv/vs_celestial.sc.bin.h"
#include "generated/shaders/spirv/vs_footprint_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_mist.sc.bin.h"
#include "generated/shaders/spirv/vs_object_few_bones_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_object_hm_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_object_hm_static_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_object_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_object_shadow_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_object_shadow_static_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_object_static_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_sprite.sc.bin.h"

using namespace openblack::graphics;

namespace
{

constexpr std::size_t k_MatrixBytes = 16 * sizeof(float);

/// Vulkan keeps every draw's uniforms of a frame in a buffer of 128 bytes for each of bgfx's 65535 draws
constexpr std::size_t k_FrameUniformBytes = 128 * 65535;

std::size_t UniformBytes(std::span<const uint8_t> binary)
{
	const auto size = shader_samplers::ReadSpirvUniformBufferSize(binary);
	EXPECT_TRUE(size.has_value());
	return size.value_or(0);
}

} // namespace

TEST(BoneBudget, EachMeshGetsTheSmallestBudgetHoldingItsBones)
{
	EXPECT_EQ(bone_budget::For(0), bone_budget::Budget::Single);
	EXPECT_EQ(bone_budget::For(1), bone_budget::Budget::Single);
	EXPECT_EQ(bone_budget::For(2), bone_budget::Budget::Few);
	// A villager's skeleton
	EXPECT_EQ(bone_budget::For(22), bone_budget::Budget::Few);
	EXPECT_EQ(bone_budget::For(bone_budget::k_Few), bone_budget::Budget::Few);
	EXPECT_EQ(bone_budget::For(bone_budget::k_Few + 1), bone_budget::Budget::All);
	EXPECT_EQ(bone_budget::For(bone_budget::k_All), bone_budget::Budget::All);
}

TEST(BoneBudget, EveryBudgetHoldsTheBonesItIsChosenFor)
{
	for (std::size_t bones = 0; bones <= bone_budget::k_All; ++bones)
	{
		EXPECT_GE(bone_budget::Bones(bone_budget::For(bones)), bones) << bones;
	}
}

TEST(BoneBudget, ShadersDeclareTheirBudgetsBones)
{
	// Each pair of shaders differs only in how many bone matrices it declares, all of which every draw uploads
	const auto few = UniformBytes(vs_object_few_bones_instanced_spv);
	EXPECT_EQ(few - UniformBytes(vs_object_static_instanced_spv), (bone_budget::k_Few - 1) * k_MatrixBytes);
	EXPECT_EQ(UniformBytes(vs_object_instanced_spv) - few, (bone_budget::k_All - bone_budget::k_Few) * k_MatrixBytes);
	EXPECT_EQ(UniformBytes(vs_object_hm_instanced_spv) - UniformBytes(vs_object_hm_static_instanced_spv),
	          (bone_budget::k_All - 1) * k_MatrixBytes);
	EXPECT_EQ(UniformBytes(vs_object_shadow_instanced_spv) - UniformBytes(vs_object_shadow_static_instanced_spv),
	          (bone_budget::k_All - 1) * k_MatrixBytes);
}

TEST(BoneBudget, ShadersUsingOnlyTheModelMatrixDeclareOne)
{
	for (const auto binary : {std::span<const uint8_t>(vs_sprite_spv), std::span<const uint8_t>(vs_mist_spv),
	                          std::span<const uint8_t>(vs_celestial_spv), std::span<const uint8_t>(vs_footprint_instanced_spv)})
	{
		// Less than the 32 bones bgfx declares by default would take alone
		EXPECT_LT(UniformBytes(binary), bone_budget::k_Few * k_MatrixBytes);
	}
}

TEST(BoneBudget, ACrowdedLandFitsAFrame)
{
	// Land 2 draws over a thousand objects in its main and reflection views and its shadows: four hundred villagers
	// in each view, a thousand meshes without bones in each, and as many shadows, with room left for the rest
	constexpr std::size_t k_Villagers = 400;
	constexpr std::size_t k_Static = 1000;
	const auto bytes = (2 * k_Villagers * UniformBytes(vs_object_few_bones_instanced_spv)) +
	                   (2 * k_Static * UniformBytes(vs_object_static_instanced_spv)) +
	                   (k_Static * UniformBytes(vs_object_shadow_static_instanced_spv));
	EXPECT_LT(bytes, k_FrameUniformBytes / 2);
}
