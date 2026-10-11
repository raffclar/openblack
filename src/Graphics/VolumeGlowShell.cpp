/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VolumeGlowShell.h"

#include <algorithm>

#include <bgfx/bgfx.h>

#include "IndexBuffer.h"
#include "VertexBuffer.h"

using namespace openblack::graphics;

std::vector<VolumeGlowShell::Vertex> VolumeGlowShell::Vertices(std::span<const glm::vec3> positions,
                                                               std::span<const glm::vec3> normals)
{
	std::vector<Vertex> out;
	const auto count = std::min(positions.size(), normals.size());
	out.reserve(count * 2);
	for (size_t i = 0; i < count; ++i)
	{
		out.push_back({.position = positions[i], .normal = normals[i], .layer = {0.0f, 0.0f}});
		out.push_back({.position = positions[i], .normal = normals[i], .layer = {1.0f, 0.0f}});
	}
	return out;
}

VolumeGlowShell::VolumeGlowShell(std::span<const Vertex> shellVertices, std::span<const uint32_t> shellTriangles)
    : indexCount(static_cast<uint32_t>(shellTriangles.size()))
{
	const VertexDecl decl {
	    VertexAttrib {VertexAttrib::Attribute::Position, 3, VertexAttrib::Type::Float},
	    VertexAttrib {VertexAttrib::Attribute::Normal, 3, VertexAttrib::Type::Float},
	    VertexAttrib {VertexAttrib::Attribute::TexCoord0, 2, VertexAttrib::Type::Float},
	};
	vertices = std::make_unique<VertexBuffer>(
	    "VolumeGlowShell", bgfx::copy(shellVertices.data(), static_cast<uint32_t>(shellVertices.size_bytes())), decl);
	triangles = std::make_unique<IndexBuffer>(
	    "VolumeGlowShell", bgfx::copy(shellTriangles.data(), static_cast<uint32_t>(shellTriangles.size_bytes())),
	    IndexBuffer::Type::Uint32);
}

VolumeGlowShell::~VolumeGlowShell() = default;
