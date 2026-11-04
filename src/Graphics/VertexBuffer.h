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

#include <string>
#include <vector>

#include "GraphicsHandle.h"

namespace bgfx
{
struct Encoder;
}

namespace openblack::graphics
{

struct VertexAttrib
{
	enum class Attribute : uint8_t
	{
		Position,
		Normal,
		Tangent,
		Bitangent,
		Color0,
		Color1,
		Color2,
		Color3,
		Indices,
		Weight,
		TexCoord0,
		TexCoord1,
		TexCoord2,
		TexCoord3,
		TexCoord4,
		TexCoord5,
		TexCoord6,
		TexCoord7,
	};
	enum class Type : uint8_t
	{
		Uint8,
		Int16,
		Float,
	};

	Attribute attribute; ///< Type of data represented
	uint8_t num;         ///< Number of components per vertex attribute, must be 1, 2,
	                     ///< 3, 4.
	Type type;           ///< Data type of each attribute component in the array.
	bool normalized;     /// < When using fixed point values, range will be
	                     /// normalized to 0.0-1.0 in shader.
	bool asInt;          /// < The values reach the shader unconverted, as integers: the shader
	                     /// must declare the input as an ivec/uvec. Only meaningful when not
	                     /// normalized.

	VertexAttrib(Attribute attribute, uint8_t num, Type type, bool normalized = false, bool asInt = false) noexcept
	    : attribute(attribute)
	    , num(num)
	    , type(type)
	    , normalized(normalized)
	    , asInt(asInt)
	{
	}
};

using VertexDecl = std::vector<VertexAttrib>;

class VertexBuffer
{
public:
	/// Where the vertices are on the GPU
	enum class Storage : uint8_t
	{
		/// A buffer of their own
		Own,
		/// A part of a large buffer that bgfx shares between many vertex lists. Then a new list does not get new memory
		/// from the driver. A dynamic buffer is always in this shared buffer.
		Shared,
	};

	/// A dynamic buffer's vertices can be changed after, by Update
	VertexBuffer(std::string name, const void* memory, VertexDecl decl, bool dynamic = false,
	             Storage storage = Storage::Own) noexcept;
	~VertexBuffer() noexcept;

	[[nodiscard]] uint32_t GetCount() const noexcept;
	[[nodiscard]] uint32_t GetStrideBytes() const noexcept;
	[[nodiscard]] uint32_t GetSizeInBytes() const noexcept;
	/// Whether Update can change the vertices
	[[nodiscard]] bool IsDynamic() const noexcept { return _dynamic; }
	/// Whether the vertices are in the shared buffer of bgfx
	[[nodiscard]] bool IsShared() const noexcept { return _shared; }

	/// Whether bgfx made the buffer; one it couldn't make isn't drawn
	[[nodiscard]] bool IsValid() const noexcept;
	void Bind() const;
	/// Binds a run of the buffer's vertices
	void Bind(uint32_t firstVertex, uint32_t count) const;
	/// Binds the buffer to another stream, its vertices read by a layout of its own: a creature's variant meshes, whose
	/// positions and normals come in as other attributes than the base mesh's. The layout must have the stride of the
	/// buffer, because the first vertex of a shared buffer counts in that stride.
	void BindStream(uint8_t stream, VertexLayoutHandle layout) const;
	/// The same into the encoder given
	void Bind(bgfx::Encoder& encoder) const;
	void BindStream(bgfx::Encoder& encoder, uint8_t stream, VertexLayoutHandle layout) const;
	/// Replaces a dynamic buffer's vertices, from the start, with bgfx memory of as many or fewer
	void Update(const void* memory) const;
	/// Replaces a dynamic buffer's vertices from one on, with bgfx memory
	void Update(uint32_t firstVertex, const void* memory) const;
	/// Tells the graphics device that the game gives the vertices again after a restore of the device. The device then
	/// keeps no copy of the vertices, also of the updates of a dynamic buffer.
	void SetContentsGivenAgain() const;
	/// Gives the bytes of all the vertices again after a restore of the device
	void GiveContentsAgain(std::vector<uint8_t> bytes) const;

private:
	std::string _name;
	uint32_t _vertexCount;
	const VertexDecl _vertexDecl;
	uint32_t _strideBytes;
	std::vector<uint32_t> _vertexDeclOffsets;
	VertexBufferHandle _handle;
	DynamicVertexBufferHandle _dynamicHandle;
	bool _dynamic;
	/// The vertices are in the shared buffer of bgfx, through _dynamicHandle
	bool _shared;
	VertexLayoutHandle _layoutHandle;
};

} // namespace openblack::graphics
