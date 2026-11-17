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

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "BufferRun.h"
#include "GraphicsHandle.h"

namespace bgfx
{
struct Encoder;
}

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
		/// A run of a page of the device's buffer pages, which many small lists of 16 bits share. Then a new list does not
		/// take one of the device's buffers, of which the device has a limit.
		Paged,
	};

	IndexBuffer() = delete;
	IndexBuffer(const IndexBuffer& other) = delete;
	IndexBuffer(IndexBuffer&&) = default;

	IndexBuffer(std::string name, const void* indices, uint32_t indexCount, Type type);
	IndexBuffer(std::string name, const void* memory, Type type, Storage storage = Storage::Own);
	/// An empty buffer for this count of indices of 16 bits, in a part of the shared buffer. The game writes runs of
	/// indices into it later with Write.
	IndexBuffer(std::string name, uint32_t capacity);

	~IndexBuffer();

	[[nodiscard]] uint32_t GetCount() const;
	[[nodiscard]] uint32_t GetStride() const;

	/// Whether bgfx made the buffer; one it couldn't make isn't drawn
	[[nodiscard]] bool IsValid() const noexcept;
	void Bind(uint32_t count, uint32_t startIndex = 0) const;
	/// The same into the encoder given
	void Bind(bgfx::Encoder& encoder, uint32_t count, uint32_t startIndex = 0) const;
	/// Tells the graphics device that the game gives the indices again after a restore of the device. The device then
	/// keeps no copy of the indices.
	void SetContentsGivenAgain() const;
	/// Writes a run of indices into an empty buffer, from the first index on. The run must fit in the buffer.
	void Write(uint32_t firstIndex, std::span<const uint16_t> indices) const;
	/// Gives the bytes of all the indices again after a restore of the device
	void GiveContentsAgain(std::vector<uint8_t> bytes) const;

private:
	/// Makes the buffer of the graphics library from bgfx memory, on the main thread
	void Make(const void* memory);

	std::string _name;
	uint32_t _count;
	Type _type;
	IndexBufferHandle _handle;
	DynamicIndexBufferHandle _sharedHandle;
	Storage _storage {Storage::Own};
	/// The run of the indices in a buffer page, after the device made it
	std::optional<BufferRun> _run;
};

} // namespace openblack::graphics
