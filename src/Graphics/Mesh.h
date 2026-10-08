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
#include <optional>

#include "GraphicsHandle.h"
#include "IndexBuffer.h" // complete: the constructor's default index buffer is destroyed at the call site
#include "RenderPass.h"

namespace bgfx
{
struct DynamicVertexBufferHandle;
}

namespace openblack::graphics
{

class IndexBuffer;
class ShaderProgram;
class VertexBuffer;

class Mesh
{
public:
	enum class Topology
	{
		PointList,
		LineList,
		LineStrip,
		TriangleList,
		TriangleStrip,
	};

	// Takes ownership of the buffers. Callers create the vertex buffer, then the index buffer, each in its own statement
	explicit Mesh(std::unique_ptr<VertexBuffer> vertexBuffer, std::unique_ptr<IndexBuffer> indexBuffer = nullptr,
	              Topology topology = Topology::TriangleList) noexcept;
	~Mesh() noexcept;

	[[nodiscard]] const VertexBuffer& GetVertexBuffer() const;
	[[nodiscard]] const IndexBuffer& GetIndexBuffer() const;
	[[nodiscard]] bool IsIndexed() const;

	[[nodiscard]] Topology GetTopology() const noexcept;

	// The bits of DrawDesc::skip
	static constexpr uint8_t k_SkipNone = 0b00000000;
	static constexpr uint8_t k_SkipRenderState = 0b00000001;
	static constexpr uint8_t k_SkipVertexBuffer = 0b00000010;
	static constexpr uint8_t k_SkipIndexBuffer = 0b00000100;
	static constexpr uint8_t k_SkipInstanceBuffer = 0b00001000;

	struct DrawDesc
	{
		graphics::RenderPass viewId;
		const openblack::graphics::ShaderProgram& program;
		uint32_t count;
		uint32_t offset;
		std::optional<DynamicVertexBufferHandle> instanceBuffer;
		uint32_t instanceStart;
		uint32_t instanceCount;
		uint64_t state;
		uint32_t rgba;
		uint8_t skip;
		bool preserveState;
	};

	void Draw(const DrawDesc& desc) const;

protected:
	// Declaration order is the destruction order: the index buffer is released before the vertex buffer
	std::unique_ptr<graphics::VertexBuffer> _vertexBuffer;
	std::unique_ptr<graphics::IndexBuffer> _indexBuffer;

private:
	Topology _topology;
};

} // namespace openblack::graphics
