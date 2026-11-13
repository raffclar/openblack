/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mesh.h"

#include "DeviceDataGivers.h"
#include "GraphicsDeviceInterface.h"
#include "IndexBuffer.h"
#include "Locator.h"
#include "MainThreadWork.h"
#include "VertexBuffer.h"

using namespace openblack::graphics;

Mesh::Mesh(VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer) noexcept
    : _vertexBuffer(vertexBuffer)
    , _indexBuffer(indexBuffer)
{
}

Mesh::~Mesh() noexcept
{
	if (_givesContentsAgain && Locator::graphicsDevice::has_value())
	{
		Locator::graphicsDevice::value().GetDataGivers().Remove(this);
	}
}

void Mesh::KeepContentsForRestore(std::vector<uint8_t> vertices, std::vector<uint8_t> indices)
{
	if (!Locator::graphicsDevice::has_value())
	{
		return;
	}
	_vertexBuffer->SetContentsGivenAgain();
	if (_indexBuffer != nullptr && !indices.empty())
	{
		_indexBuffer->SetContentsGivenAgain();
	}
	PostToMainThread(MainThreadApi::Graphics, [this, vertices = std::move(vertices), indices = std::move(indices)] {
		Locator::graphicsDevice::value().GetDataGivers().Add(this, [this, vertices, indices](const auto&) {
			_vertexBuffer->GiveContentsAgain(vertices);
			if (_indexBuffer != nullptr && !indices.empty())
			{
				_indexBuffer->GiveContentsAgain(indices);
			}
		});
		_givesContentsAgain = true;
	});
}

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
