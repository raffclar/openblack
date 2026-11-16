/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorScriptFiles.h"

#include <algorithm>

#include "Editor/EditorMindLoads.h"

namespace openblack::editor::script_files
{

std::vector<game_socket::EditorScriptFile> SplitFile(Kind kind, std::span<const uint8_t> bytes)
{
	std::vector<game_socket::EditorScriptFile> parts;
	if (bytes.empty() || bytes.size() > game_socket::k_MostEditorScriptFileBytes)
	{
		return parts;
	}
	const auto hash = mind_loads::HashOf(bytes);
	const auto count = (bytes.size() + game_socket::k_MostEditorScriptPartBytes - 1) / game_socket::k_MostEditorScriptPartBytes;
	parts.reserve(count);
	for (size_t index = 0; index < count; ++index)
	{
		const auto start = index * game_socket::k_MostEditorScriptPartBytes;
		const auto piece = bytes.subspan(start, std::min(game_socket::k_MostEditorScriptPartBytes, bytes.size() - start));
		parts.push_back({.kind = kind,
		                 .hash = hash,
		                 .index = static_cast<uint32_t>(index),
		                 .count = static_cast<uint32_t>(count),
		                 .size = static_cast<uint32_t>(bytes.size()),
		                 .bytes = {piece.begin(), piece.end()}});
	}
	return parts;
}

PartAssembly::Step PartAssembly::Add(const game_socket::EditorScriptFile& part)
{
	const auto forget = [this]() {
		_first.reset();
		_bytes.clear();
		_next = 0;
		_done = false;
		return Step::Bad;
	};
	// The first part of a file starts it again, also after a file that did not end
	if (part.index == 0)
	{
		_first = part;
		_first->bytes.clear();
		_bytes.clear();
		_next = 0;
		_done = false;
	}
	if (!_first.has_value() || _done || part.index != _next || part.kind != _first->kind || part.hash != _first->hash ||
	    part.count != _first->count || part.size != _first->size || _bytes.size() + part.bytes.size() > part.size)
	{
		return forget();
	}
	_bytes.insert(_bytes.end(), part.bytes.begin(), part.bytes.end());
	++_next;
	if (_next < part.count)
	{
		return Step::Waiting;
	}
	if (_bytes.size() != part.size || mind_loads::HashOf(_bytes) != part.hash)
	{
		return forget();
	}
	_done = true;
	return Step::Done;
}

std::optional<PartAssembly::File> PartAssembly::Take()
{
	if (!_done || !_first.has_value())
	{
		return std::nullopt;
	}
	File file {.kind = _first->kind, .hash = _first->hash, .bytes = std::move(_bytes)};
	_first.reset();
	_bytes = {};
	_next = 0;
	_done = false;
	return file;
}

std::pair<FileStore::Added, uint64_t> FileStore::Add(Kind kind, std::vector<uint8_t> bytes)
{
	const auto hash = mind_loads::HashOf(bytes);
	if (!Find(kind, hash).empty())
	{
		return {Added::Known, hash};
	}
	if (bytes.empty() || _files.size() >= k_MostKeptFiles || _bytes + bytes.size() > k_MostKeptBytes)
	{
		return {Added::Refused, hash};
	}
	_bytes += bytes.size();
	_files.push_back({.kind = kind, .hash = hash, .bytes = std::move(bytes)});
	return {Added::New, hash};
}

std::span<const uint8_t> FileStore::Find(Kind kind, uint64_t hash) const noexcept
{
	const auto found =
	    std::ranges::find_if(_files, [kind, hash](const File& file) { return file.kind == kind && file.hash == hash; });
	return found != _files.end() ? std::span<const uint8_t>(found->bytes) : std::span<const uint8_t> {};
}

} // namespace openblack::editor::script_files
