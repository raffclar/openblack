/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <utility>

#include <bgfx/bgfx.h>

namespace openblack::graphics
{

/// Owns one bgfx handle (a texture, a dynamic vertex buffer...): bgfx::destroy when it is reset, replaced or goes.
/// An owner that outlives bgfx::shutdown resets its handles explicitly before the shutdown; its destructor then has
/// nothing left to destroy
template <typename Handle>
class UniqueHandle
{
public:
	UniqueHandle() noexcept = default;
	explicit UniqueHandle(Handle handle) noexcept
	    : _handle(handle)
	{
	}
	UniqueHandle(UniqueHandle&& other) noexcept
	    : _handle(std::exchange(other._handle, Handle BGFX_INVALID_HANDLE))
	{
	}
	UniqueHandle& operator=(UniqueHandle&& other) noexcept
	{
		if (this != &other)
		{
			Reset(std::exchange(other._handle, Handle BGFX_INVALID_HANDLE));
		}
		return *this;
	}
	UniqueHandle(const UniqueHandle&) = delete;
	UniqueHandle& operator=(const UniqueHandle&) = delete;
	~UniqueHandle() { Reset(); }

	/// Destroys the handle it holds, if valid, then holds `handle`
	void Reset(Handle handle = BGFX_INVALID_HANDLE) noexcept
	{
		if (bgfx::isValid(_handle))
		{
			bgfx::destroy(_handle);
		}
		_handle = handle;
	}

	[[nodiscard]] Handle Get() const noexcept { return _handle; }
	[[nodiscard]] bool IsValid() const noexcept { return bgfx::isValid(_handle); }

private:
	Handle _handle BGFX_INVALID_HANDLE;
};

} // namespace openblack::graphics
