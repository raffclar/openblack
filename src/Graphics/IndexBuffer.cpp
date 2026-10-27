/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IndexBuffer.h"

#include <cassert>

#include <string>
#include <utility>

#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "Common/MainThreadQueue.h"
#include "GraphicsHandleBgfx.h"
#include "MainThreadWork.h"
#include "UploadPacer.h"

using namespace openblack::graphics;

IndexBuffer::IndexBuffer(std::string name, const void* indices, uint32_t indexCount, Type type)
    : _name(std::move(name))
    , _count(indexCount)
    , _type(type)
    , _handle(BGFX_INVALID_HANDLE)
    , _sharedHandle(BGFX_INVALID_HANDLE)
{
	assert(indices != nullptr);
	assert(indexCount > 0);

	// The indices are only referenced when the buffer is made at once. For work that waits, they are copied.
	const auto bytes = indexCount * GetTypeSize(_type);
	const auto* mem = MainThreadWorkRunsAtOnce() ? bgfx::makeRef(indices, bytes) : bgfx::copy(indices, bytes);
	Make(mem);
}

IndexBuffer::IndexBuffer(std::string name, const void* mem, Type type, Storage storage)
    : _name(std::move(name))
    , _type(type)
    , _handle(BGFX_INVALID_HANDLE)
    , _sharedHandle(BGFX_INVALID_HANDLE)
    , _storage(storage)
{
	const auto* memBgfx = static_cast<const bgfx::Memory*>(mem);
	_count = memBgfx->size / sizeof(uint16_t);
	Make(memBgfx);
}

void IndexBuffer::Make(const void* memory)
{
	// The graphics library makes the buffer on the main thread: a turn on another thread gives that work to the main
	// thread, which makes the buffer before the next draw
	const auto* memBgfx = static_cast<const bgfx::Memory*>(memory);
	PostToMainThread(MainThreadApi::Graphics, [this, memBgfx] {
		UploadPacer::Pace(memBgfx->size);
		const uint16_t flags = _type == Type::Uint32 ? BGFX_BUFFER_INDEX32 : 0;
		if (_storage == Storage::Shared)
		{
			// bgfx gives a part of its shared buffer. A name is only for a buffer of its own.
			_sharedHandle = fromBgfx(bgfx::createDynamicIndexBuffer(memBgfx, flags));
			if (!bgfx::isValid(toBgfx(_sharedHandle)))
			{
				SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "{}: out of bgfx buffer handles, not created", _name);
			}
			return;
		}
		_handle = fromBgfx(bgfx::createIndexBuffer(memBgfx, flags));
		if (!bgfx::isValid(toBgfx(_handle)))
		{
			// Out of handles, setting a name would write past bgfx's own arrays; the buffer is left empty and isn't drawn
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "{}: out of bgfx buffer handles, not created", _name);
			return;
		}
		bgfx::setName(toBgfx(_handle), _name.c_str());
	});
}

IndexBuffer::~IndexBuffer()
{
	MainThreadQueue::CheckDirect(MainThreadApi::Graphics, "IndexBuffer::~IndexBuffer");
	if (bgfx::isValid(toBgfx(_handle)))
	{
		bgfx::destroy(toBgfx(_handle));
	}
	if (_storage == Storage::Shared && bgfx::isValid(toBgfx(_sharedHandle)))
	{
		bgfx::destroy(toBgfx(_sharedHandle));
	}
}

uint32_t IndexBuffer::GetCount() const
{
	return _count;
}

uint32_t IndexBuffer::GetSize() const
{
	return _count * GetStride();
}

uint32_t IndexBuffer::GetStride() const
{
	return GetTypeSize(_type);
}

IndexBuffer::Type IndexBuffer::GetType() const
{
	return _type;
}

uint32_t IndexBuffer::GetTypeSize(Type type)
{
	return static_cast<uint32_t>(type == Type::Uint16 ? sizeof(uint16_t) : sizeof(uint32_t));
}

bool IndexBuffer::IsValid() const noexcept
{
	return _storage == Storage::Shared ? bgfx::isValid(toBgfx(_sharedHandle)) : bgfx::isValid(toBgfx(_handle));
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
