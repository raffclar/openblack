/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

/// The posed bones of every villager drawn in a frame, gathered into one floating-point texture that the vertex shader
/// reads. Each villager's bones are found by where they start in it, which its instance carries, so every villager of a
/// mesh is drawn in one instanced draw instead of one draw each with its bones uploaded as uniforms.
namespace openblack::graphics::bone_palette
{

/// Each bone is three texels: the first three rows of its matrix, whose last row is always (0, 0, 0, 1)
constexpr std::size_t k_TexelsPerBone = 3;
/// The texture's width in texels; a villager's bones may run on from one row into the next
constexpr uint16_t k_Width = 1024;

/// Adds the bones to the end of the palette and returns the index of the first of them
uint32_t Append(std::vector<glm::vec4>& texels, std::span<const glm::mat4> bones);

/// The rows of the texture the palette fills, at least one
[[nodiscard]] uint16_t RowsFor(std::size_t texels) noexcept;

/// The bone back from the palette, as the vertex shader reads it: for tests
[[nodiscard]] glm::mat4 BoneAt(std::span<const glm::vec4> texels, uint32_t bone);

/// Where an instance's bones start, carried in the w of its model matrix's first column, which is always 0 for a
/// matrix that places an object
void SetFirstBone(glm::mat4& model, uint32_t bone) noexcept;
[[nodiscard]] uint32_t FirstBone(const glm::mat4& model) noexcept;

} // namespace openblack::graphics::bone_palette
