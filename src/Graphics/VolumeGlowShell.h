/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics
{
class IndexBuffer;
class VertexBuffer;

/// The band of a glowing shell over one part of a model (see particles::volume_glow): every corner twice, on the model
/// and marked to be pushed out along its normal by the vertex shader, and the bands' triangles between them. Made once
/// for each part of each model that glows.
struct VolumeGlowShell
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		/// x: 0 on the model, 1 pushed out
		glm::vec2 layer;
	};
	static_assert(sizeof(Vertex) == 32);

	/// The corners of a part, each on the model then pushed out
	[[nodiscard]] static std::vector<Vertex> Vertices(std::span<const glm::vec3> positions, std::span<const glm::vec3> normals);

	VolumeGlowShell(std::span<const Vertex> vertices, std::span<const uint32_t> triangles);
	~VolumeGlowShell();
	VolumeGlowShell(const VolumeGlowShell&) = delete;
	VolumeGlowShell& operator=(const VolumeGlowShell&) = delete;
	VolumeGlowShell(VolumeGlowShell&&) = delete;
	VolumeGlowShell& operator=(VolumeGlowShell&&) = delete;

	std::unique_ptr<VertexBuffer> vertices;
	std::unique_ptr<IndexBuffer> triangles;
	uint32_t indexCount {0};
};

} // namespace openblack::graphics
