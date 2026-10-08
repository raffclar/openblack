/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mesh.h"

#include <utility>

#include "GraphicsHandleBgfx.h"
#include "IndexBuffer.h"
#include "ShaderProgram.h"
#include "VertexBuffer.h"

using namespace openblack::graphics;

Mesh::Mesh(std::unique_ptr<VertexBuffer> vertexBuffer, std::unique_ptr<IndexBuffer> indexBuffer, Topology topology) noexcept
    : _vertexBuffer(std::move(vertexBuffer))
    , _indexBuffer(std::move(indexBuffer))
    , _topology(topology)
{
}

Mesh::~Mesh() noexcept = default;

const VertexBuffer& Mesh::GetVertexBuffer() const
{
	return *_vertexBuffer;
}

const IndexBuffer& Mesh::GetIndexBuffer() const
{
	return *_indexBuffer;
}

bool Mesh::IsIndexed() const
{
	return _indexBuffer != nullptr && _indexBuffer->GetCount() > 0;
}

Mesh::Topology Mesh::GetTopology() const noexcept
{
	return _topology;
}

void Mesh::Draw(const DrawDesc& desc) const
{
	if (desc.instanceBuffer && (desc.skip & k_SkipInstanceBuffer) == 0)
	{
		bgfx::setInstanceDataBuffer(toBgfx(*desc.instanceBuffer), desc.instanceStart, desc.instanceCount);
	}
	if (_indexBuffer != nullptr && _indexBuffer->GetCount() > 0 && (desc.skip & k_SkipIndexBuffer) == 0)
	{
		_indexBuffer->Bind(desc.count, desc.offset);
	}
	if ((desc.skip & k_SkipVertexBuffer) == 0)
	{
		_vertexBuffer->Bind();
	}
	if ((desc.skip & k_SkipRenderState) == 0)
	{
		bgfx::setState(desc.state, desc.rgba);
	}

	bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(desc.program.GetRawHandle()), 0,
	             desc.preserveState ? BGFX_DISCARD_NONE : BGFX_DISCARD_ALL);
}
