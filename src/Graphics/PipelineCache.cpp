/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PipelineCache.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace openblack::graphics
{

PipelineCache::PipelineCache(std::filesystem::path path)
    : _path(std::move(path))
{
	std::error_code error;
	const auto size = std::filesystem::file_size(_path, error);
	if (error || size == 0)
	{
		return;
	}
	std::ifstream file(_path, std::ios::binary);
	_bytes.resize(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(_bytes.data()), static_cast<std::streamsize>(_bytes.size())))
	{
		_bytes.clear();
	}
}

PipelineCache::~PipelineCache()
{
	if (!_changed || _bytes.empty())
	{
		return;
	}
	std::error_code error;
	std::filesystem::create_directories(_path.parent_path(), error);
	// The bytes go into a different file first, so a run that stops during the write never leaves part of a cache
	auto partial = _path;
	partial += ".part";
	{
		std::ofstream file(partial, std::ios::binary | std::ios::trunc);
		if (!file.write(reinterpret_cast<const char*>(_bytes.data()), static_cast<std::streamsize>(_bytes.size())))
		{
			return;
		}
	}
	std::filesystem::rename(partial, _path, error);
	if (error)
	{
		std::filesystem::remove(partial, error);
	}
}

uint32_t PipelineCache::ReadSize() const
{
	return static_cast<uint32_t>(_bytes.size());
}

bool PipelineCache::Read(std::span<std::byte> data) const
{
	if (data.size() != _bytes.size())
	{
		return false;
	}
	std::ranges::copy(_bytes, data.begin());
	return true;
}

void PipelineCache::Write(std::span<const std::byte> data)
{
	// The driver's cache started from the kept pipelines, so it has all of them and the new one. When the driver
	// cannot use the kept pipelines (for example, after a driver update), it starts again from none.
	_bytes.assign(data.begin(), data.end());
	_changed = true;
}

} // namespace openblack::graphics
