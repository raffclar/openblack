/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BonePalette.h"

#include <algorithm>

namespace openblack::graphics::bone_palette
{

uint32_t Append(std::vector<glm::vec4>& texels, std::span<const glm::mat4> bones)
{
	const auto first = static_cast<uint32_t>(texels.size() / k_TexelsPerBone);
	texels.reserve(texels.size() + bones.size() * k_TexelsPerBone);
	for (const auto& bone : bones)
	{
		// glm keeps columns: a row is the same element of each column
		for (int row = 0; row < static_cast<int>(k_TexelsPerBone); ++row)
		{
			texels.emplace_back(bone[0][row], bone[1][row], bone[2][row], bone[3][row]);
		}
	}
	return first;
}

uint16_t RowsFor(std::size_t texels) noexcept
{
	return static_cast<uint16_t>(std::max<std::size_t>(1, (texels + k_Width - 1) / k_Width));
}

glm::mat4 BoneAt(std::span<const glm::vec4> texels, uint32_t bone)
{
	glm::mat4 matrix(1.0f);
	for (int row = 0; row < static_cast<int>(k_TexelsPerBone); ++row)
	{
		const auto& texel = texels[bone * k_TexelsPerBone + static_cast<std::size_t>(row)];
		for (int column = 0; column < 4; ++column)
		{
			matrix[column][row] = texel[column];
		}
	}
	return matrix;
}

void SetFirstBone(glm::mat4& model, uint32_t bone) noexcept
{
	model[0][3] = static_cast<float>(bone);
}

uint32_t FirstBone(const glm::mat4& model) noexcept
{
	return static_cast<uint32_t>(model[0][3] + 0.5f);
}

} // namespace openblack::graphics::bone_palette
