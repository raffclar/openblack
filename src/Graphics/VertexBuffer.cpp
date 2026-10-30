/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VertexBuffer.h"

#include <cassert>

#include <array>

#include <spdlog/spdlog.h>

#include "Common/MainThreadQueue.h"
#include "GraphicsDeviceInterface.h"
#include "GraphicsHandleBgfx.h"
#include "Locator.h"
#include "MainThreadWork.h"
#include "Visual/UploadPacer.h"

using namespace openblack::graphics;

VertexBuffer::VertexBuffer(std::string name, const void* mem, VertexDecl decl, bool dynamic) noexcept
    : _name(std::move(name))
    , _vertexCount(0)
    , _vertexDecl(std::move(decl))
    , _strideBytes(0)
    , _handle(BGFX_INVALID_HANDLE)
    , _dynamicHandle(BGFX_INVALID_HANDLE)
    , _dynamic(dynamic)
    , _layoutHandle(BGFX_INVALID_HANDLE)
{
	// assert(vertices != nullptr);
	assert(!_vertexDecl.empty());

	// Extract gl types from decl
	_vertexDeclOffsets.reserve(_vertexDecl.size());
	static const std::array<std::array<uint32_t, 4>, 3> strides = {
	    std::array<uint32_t, 4> {4, 4, 4, 4},   // Uint8
	    std::array<uint32_t, 4> {4, 4, 8, 8},   // Int16
	    std::array<uint32_t, 4> {4, 8, 12, 16}, // Float
	};

	for (const auto& d : _vertexDecl)
	{
		_vertexDeclOffsets.push_back(_strideBytes);
		_strideBytes += strides.at(static_cast<size_t>(d.type)).at(d.num - 1);
	}

	const auto* bgfxMem = reinterpret_cast<const bgfx::Memory*>(mem);

	_vertexCount = bgfxMem->size / _strideBytes;

	// The graphics library makes the buffer on the main thread: a turn on another thread gives that work to the main
	// thread, which makes the buffer before the next draw
	PostToMainThread(MainThreadApi::Graphics, [this, bgfxMem] {
		UploadPacer::Pace(bgfxMem->size);
		const auto& device = Locator::graphicsDevice::value();
		if (_dynamic)
		{
			_dynamicHandle = device.CreateDynamicVertexBuffer(bgfxMem, _vertexDecl);
		}
		else
		{
			_handle = device.CreateVertexBuffer(bgfxMem, _vertexDecl);
		}
		_layoutHandle = device.CreateVertexLayout(_vertexDecl);
		if (!IsValid())
		{
			// Out of handles, setting a name would write past bgfx's own arrays; the buffer is left empty and isn't drawn
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "{}: out of bgfx buffer handles, not created", _name);
			return;
		}
		if (!_dynamic)
		{
			device.SetName(_handle, _name);
		}
	});
}

VertexBuffer::~VertexBuffer() noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Graphics, "VertexBuffer::~VertexBuffer");
	if (bgfx::isValid(toBgfx(_handle)))
	{
		Locator::graphicsDevice::value().Destroy(_handle);
	}
	if (bgfx::isValid(toBgfx(_dynamicHandle)))
	{
		Locator::graphicsDevice::value().Destroy(_dynamicHandle);
	}
	if (bgfx::isValid(toBgfx(_layoutHandle)))
	{
		Locator::graphicsDevice::value().Destroy(_layoutHandle);
	}
}

uint32_t VertexBuffer::GetCount() const noexcept
{
	return _vertexCount;
}

uint32_t VertexBuffer::GetStrideBytes() const noexcept
{
	return _strideBytes;
}

uint32_t VertexBuffer::GetSizeInBytes() const noexcept
{
	return _vertexCount * _strideBytes;
}

bool VertexBuffer::IsValid() const noexcept
{
	return _dynamic ? bgfx::isValid(toBgfx(_dynamicHandle)) : bgfx::isValid(toBgfx(_handle));
}

void VertexBuffer::Update(const void* memory) const
{
	Update(0, memory);
}

void VertexBuffer::Update(uint32_t firstVertex, const void* memory) const
{
	assert(_dynamic);
	// A turn on another thread gives the update to the main thread. It reads whether the buffer was made only then.
	const auto* vertices = reinterpret_cast<const bgfx::Memory*>(memory);
	PostToMainThread(MainThreadApi::Graphics, [this, firstVertex, vertices] {
		if (!IsValid())
		{
			return;
		}
		Locator::graphicsDevice::value().Update(_dynamicHandle, firstVertex, vertices);
	});
}
