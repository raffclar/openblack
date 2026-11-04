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
#include <vector>

#include "GraphicsHandle.h"
#include "Visual/RenderPass.h"

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

	explicit Mesh(VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer = nullptr,
	              Topology topology = Topology::TriangleList) noexcept;
	~Mesh() noexcept;

	[[nodiscard]] const VertexBuffer& GetVertexBuffer() const;
	[[nodiscard]] const IndexBuffer& GetIndexBuffer() const;
	[[nodiscard]] bool IsIndexed() const;

	[[nodiscard]] Topology GetTopology() const noexcept;

	/// For a small mesh of fixed data: the mesh keeps a copy of its vertices and indices, and gives them to the graphics
	/// device again after a restore. The device then keeps no copy.
	void KeepContentsForRestore(std::vector<uint8_t> vertices, std::vector<uint8_t> indices = {});

	enum SkipState : uint8_t
	{
		SkipNone = 0b00000000,
		SkipRenderState = 0b00000001,
		SkipVertexBuffer = 0b00000010,
		SkipIndexBuffer = 0b00000100,
		SkipInstanceBuffer = 0b00001000,
	};

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
	std::unique_ptr<graphics::VertexBuffer> _vertexBuffer;
	std::unique_ptr<graphics::IndexBuffer> _indexBuffer;

private:
	Topology _topology;
	/// Whether the mesh is one of the owners that give their data to the graphics device again
	bool _givesContentsAgain {false};
};

} // namespace openblack::graphics
