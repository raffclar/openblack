/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorWorldUpload.h"

#include <cstddef>

#include <algorithm>
#include <utility>

namespace openblack::online::editor_world
{

std::vector<game_socket::EditorWorld> Split(std::span<const uint8_t> file)
{
	std::vector<game_socket::EditorWorld> parts;
	if (file.empty() || file.size() > game_socket::k_MostEditorWorldBytes)
	{
		return parts;
	}
	constexpr auto k_Chunk = game_socket::k_MostEditorWorldChunkBytes;
	const auto count = static_cast<uint32_t>((file.size() + k_Chunk - 1) / k_Chunk);
	for (uint32_t index = 0; index < count; ++index)
	{
		const auto first = static_cast<size_t>(index) * k_Chunk;
		const auto chunk = file.subspan(first, std::min(k_Chunk, file.size() - first));
		parts.push_back({.index = index,
		                 .count = count,
		                 .size = static_cast<uint32_t>(file.size()),
		                 .bytes = std::vector<uint8_t>(chunk.begin(), chunk.end())});
	}
	return parts;
}

Upload::Added Upload::Add(const game_socket::EditorWorld& part)
{
	if (part.index == 0)
	{
		Clear();
		_count = part.count;
		_size = part.size;
	}
	const bool next = !_done && part.index == _next && part.count == _count && part.size == _size && _count > 0;
	if (!next || part.bytes.empty() || _bytes.size() + part.bytes.size() > _size)
	{
		Clear();
		return Added::Broken;
	}
	_bytes.insert(_bytes.end(), part.bytes.begin(), part.bytes.end());
	++_next;
	if (_next < _count)
	{
		return Added::More;
	}
	if (_bytes.size() != _size)
	{
		Clear();
		return Added::Broken;
	}
	_done = true;
	return Added::Done;
}

std::vector<uint8_t> Upload::Take()
{
	auto bytes = _done ? std::exchange(_bytes, {}) : std::vector<uint8_t> {};
	Clear();
	return bytes;
}

void Upload::Clear()
{
	_bytes.clear();
	_next = 0;
	_count = 0;
	_size = 0;
	_done = false;
}

} // namespace openblack::online::editor_world
