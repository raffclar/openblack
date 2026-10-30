/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The parts of the mesh, vertex buffer and index buffer that give draws to the graphics library. Only the game's library
// has them, because the dedicated server draws nothing.

#include <bgfx/bgfx.h>

#include "3D/LandBlock.h"
#include "Common/MainThreadQueue.h"
#include "GraphicsHandleBgfx.h"
#include "IndexBuffer.h"
#include "MainThreadWork.h"
#include "Mesh.h"
#include "ShaderProgram.h"
#include "VertexBuffer.h"

using namespace openblack::graphics;

void VertexBuffer::Bind() const
{
	MainThreadQueue::CheckDirect(MainThreadApi::Graphics, "VertexBuffer::Bind");
	if (!IsValid())
	{
		return;
	}
	if (_dynamic)
	{
		bgfx::setVertexBuffer(0, toBgfx(_dynamicHandle), 0, _vertexCount, toBgfx(_layoutHandle));
	}
	else
	{
		bgfx::setVertexBuffer(0, toBgfx(_handle), 0, _vertexCount, toBgfx(_layoutHandle));
	}
}

void VertexBuffer::Bind(uint32_t firstVertex, uint32_t count) const
{
	MainThreadQueue::CheckDirect(MainThreadApi::Graphics, "VertexBuffer::Bind");
	if (!IsValid())
	{
		return;
	}
	if (_dynamic)
	{
		bgfx::setVertexBuffer(0, toBgfx(_dynamicHandle), firstVertex, count, toBgfx(_layoutHandle));
	}
	else
	{
		bgfx::setVertexBuffer(0, toBgfx(_handle), firstVertex, count, toBgfx(_layoutHandle));
	}
}

void VertexBuffer::BindStream(uint8_t stream, VertexLayoutHandle layout) const
{
	MainThreadQueue::CheckDirect(MainThreadApi::Graphics, "VertexBuffer::BindStream");
	if (!IsValid())
	{
		return;
	}
	if (_dynamic)
	{
		bgfx::setVertexBuffer(stream, toBgfx(_dynamicHandle), 0, _vertexCount, toBgfx(layout));
	}
	else
	{
		bgfx::setVertexBuffer(stream, toBgfx(_handle), 0, _vertexCount, toBgfx(layout));
	}
}

void IndexBuffer::Bind(uint32_t count, uint32_t startIndex) const
{
	MainThreadQueue::CheckDirect(MainThreadApi::Graphics, "IndexBuffer::Bind");
	if (!IsValid())
	{
		return;
	}
	if (_storage == Storage::Shared)
	{
		bgfx::setIndexBuffer(toBgfx(_sharedHandle), startIndex, count);
		return;
	}
	bgfx::setIndexBuffer(toBgfx(_handle), startIndex, count);
}

void Mesh::Draw(const DrawDesc& desc) const
{
	if (desc.instanceBuffer && (desc.skip & SkipState::SkipInstanceBuffer) == 0)
	{
		bgfx::setInstanceDataBuffer(toBgfx(*desc.instanceBuffer), desc.instanceStart, desc.instanceCount);
	}
	if (_indexBuffer != nullptr && _indexBuffer->GetCount() > 0 && (desc.skip & SkipState::SkipIndexBuffer) == 0)
	{
		_indexBuffer->Bind(desc.count, desc.offset);
	}
	if ((desc.skip & SkipState::SkipVertexBuffer) == 0)
	{
		_vertexBuffer->Bind();
	}
	if ((desc.skip & SkipState::SkipRenderState) == 0)
	{
		bgfx::setState(desc.state, desc.rgba);
	}

	desc.program.Submit(static_cast<bgfx::ViewId>(desc.viewId), 0, desc.preserveState ? BGFX_DISCARD_NONE : BGFX_DISCARD_ALL);
}

void openblack::LandBlock::BindVertices() const
{
	if (_vertices != nullptr)
	{
		_vertices->Bind(_firstVertex, k_VertexCount);
	}
}
