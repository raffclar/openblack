/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <string>

#include "GraphicsHandle.h"

namespace openblack::graphics
{
class IndexBuffer
{
public:
	enum class Type : uint8_t
	{
		Uint16,
		Uint32,
	};
	static uint32_t GetTypeSize(Type type);

	/// Where the indices are on the GPU
	enum class Storage : uint8_t
	{
		/// A buffer of their own
		Own,
		/// A part of a large buffer that bgfx shares between many small lists. A new list then does not make a new
		/// buffer on the GPU, which can take a quarter of a millisecond.
		Shared,
	};

	IndexBuffer() = delete;
	IndexBuffer(const IndexBuffer& other) = delete;
	IndexBuffer(IndexBuffer&&) = default;

	IndexBuffer(std::string name, const void* indices, uint32_t indexCount, Type type);
	IndexBuffer(std::string name, const void* memory, Type type, Storage storage = Storage::Own);

	~IndexBuffer();

	[[nodiscard]] uint32_t GetCount() const;
	[[nodiscard]] uint32_t GetSize() const;
	[[nodiscard]] uint32_t GetStride() const;
	[[nodiscard]] Type GetType() const;

	/// Whether bgfx made the buffer; one it couldn't make isn't drawn
	[[nodiscard]] bool IsValid() const noexcept;
	void Bind(uint32_t count, uint32_t startIndex = 0) const;

private:
	std::string _name;
	uint32_t _count;
	Type _type;
	IndexBufferHandle _handle;
	DynamicIndexBufferHandle _sharedHandle;
	Storage _storage {Storage::Own};
};

} // namespace openblack::graphics
