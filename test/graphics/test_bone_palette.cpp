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
#include <span>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Graphics/BonePalette.h"
#include "Graphics/ShaderSamplers.h"

// The villagers' shader as the game embeds it, compiled for Vulkan, and the one for meshes without bones
#include "generated/shaders/spirv/vs_object_palette_instanced.sc.bin.h"
#include "generated/shaders/spirv/vs_object_static_instanced.sc.bin.h"

using namespace openblack::graphics;

namespace
{

/// A bone turned, scaled and moved, as a posed bone is
glm::mat4 Bone(float angle, glm::vec3 offset)
{
	auto bone = glm::translate(glm::mat4(1.0f), offset);
	bone = glm::rotate(bone, angle, glm::normalize(glm::vec3(0.3f, 1.0f, -0.2f)));
	return glm::scale(bone, glm::vec3(1.0f, 1.5f, 0.75f));
}

} // namespace

TEST(BonePalette, BonesComeBackAsTheyWent)
{
	std::vector<glm::vec4> texels;
	const std::array first {Bone(0.5f, {1.0f, 2.0f, 3.0f}), Bone(-1.25f, {-4.0f, 0.5f, 7.0f})};
	const std::array second {Bone(2.0f, {0.0f, -1.0f, 0.25f}), Bone(0.0f, {0.0f, 0.0f, 0.0f}), Bone(3.0f, {9.0f, 9.0f, -9.0f})};

	EXPECT_EQ(bone_palette::Append(texels, first), 0);
	// The next instance's bones start after the first's
	EXPECT_EQ(bone_palette::Append(texels, second), first.size());
	EXPECT_EQ(texels.size(), (first.size() + second.size()) * bone_palette::k_TexelsPerBone);

	for (uint32_t i = 0; i < first.size(); ++i)
	{
		EXPECT_EQ(bone_palette::BoneAt(texels, i), first.at(i)) << i;
	}
	for (uint32_t i = 0; i < second.size(); ++i)
	{
		EXPECT_EQ(bone_palette::BoneAt(texels, static_cast<uint32_t>(first.size()) + i), second.at(i)) << i;
	}
}

TEST(BonePalette, ABonePlacesAVertexAsItsMatrixDoes)
{
	std::vector<glm::vec4> texels;
	const auto bone = Bone(0.75f, {3.0f, -2.0f, 1.0f});
	bone_palette::Append(texels, std::span(&bone, 1));
	const glm::vec4 vertex {0.5f, 1.25f, -2.0f, 1.0f};
	// Each texel is a row of the matrix, which the shader meets the vertex with
	const glm::vec4 placed {glm::dot(texels[0], vertex), glm::dot(texels[1], vertex), glm::dot(texels[2], vertex), 1.0f};
	const auto expected = bone * vertex;
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_NEAR(placed[i], expected[i], 1e-5f) << i;
	}
}

TEST(BonePalette, TheTextureHasRowsEnoughForTheBones)
{
	EXPECT_EQ(bone_palette::RowsFor(0), 1);
	EXPECT_EQ(bone_palette::RowsFor(1), 1);
	EXPECT_EQ(bone_palette::RowsFor(bone_palette::k_Width), 1);
	EXPECT_EQ(bone_palette::RowsFor(bone_palette::k_Width + 1), 2);
	// A thousand villagers of 22 bones
	EXPECT_EQ(bone_palette::RowsFor(1000 * 22 * bone_palette::k_TexelsPerBone), 65);
}

TEST(BonePalette, TheFirstBoneRidesInTheMatrixWithoutMovingIt)
{
	const auto placement = glm::scale(glm::translate(glm::mat4(1.0f), {120.0f, 4.0f, -88.0f}), glm::vec3(1.1f));
	for (const uint32_t first : {0u, 22u, 21'978u, 1'000'000u})
	{
		auto model = placement;
		bone_palette::SetFirstBone(model, first);
		EXPECT_EQ(bone_palette::FirstBone(model), first);
		// Only the first column's w carries it, which the shader clears before placing the instance
		model[0][3] = 0.0f;
		EXPECT_EQ(model, placement);
	}
}

TEST(BonePalette, TheShaderReadsItsBonesFromThePaletteAlone)
{
	// No more is uploaded with each draw than for a mesh without bones
	const auto palette = shader_samplers::ReadSpirvUniformBufferSize(vs_object_palette_instanced_spv);
	const auto single = shader_samplers::ReadSpirvUniformBufferSize(vs_object_static_instanced_spv);
	ASSERT_TRUE(palette.has_value());
	ASSERT_TRUE(single.has_value());
	EXPECT_LE(*palette, *single);
	// The bones are read from the palette at its own stage
	const auto samplers = shader_samplers::ReadSpirvSamplers(vs_object_palette_instanced_spv);
	ASSERT_TRUE(samplers.has_value());
	const auto found = std::ranges::find(*samplers, std::string("s_bonePalette"), &shader_samplers::Sampler::name);
	ASSERT_NE(found, samplers->end());
	EXPECT_EQ(found->stage, 2);
}
