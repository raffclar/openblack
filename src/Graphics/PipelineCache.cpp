/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PipelineCache.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>
#include <utility>

using namespace openblack::graphics;

namespace
{

/// The start of a file of caches for each pipeline, and its version
constexpr std::array<char, 4> k_PerPipelineMagic = {'O', 'B', 'P', 'C'};
constexpr uint32_t k_PerPipelineVersion = 1;

template <typename T>
bool ReadValue(std::span<const std::byte>& bytes, T& value)
{
	if (bytes.size() < sizeof(T))
	{
		return false;
	}
	std::memcpy(&value, bytes.data(), sizeof(T));
	bytes = bytes.subspan(sizeof(T));
	return true;
}

template <typename T>
void WriteValue(std::vector<std::byte>& bytes, const T& value)
{
	const auto* first = reinterpret_cast<const std::byte*>(&value);
	bytes.insert(bytes.end(), first, first + sizeof(T));
}

/// The caches of a file for each pipeline: a magic, a version, the count, then the number, size and bytes of each.
/// Empty if the file is not of this form.
std::map<uint64_t, std::vector<std::byte>> ParsePerPipeline(std::span<const std::byte> bytes)
{
	std::map<uint64_t, std::vector<std::byte>> caches;
	std::array<char, 4> magic {};
	uint32_t version = 0;
	uint32_t count = 0;
	if (!ReadValue(bytes, magic) || magic != k_PerPipelineMagic || !ReadValue(bytes, version) ||
	    version != k_PerPipelineVersion || !ReadValue(bytes, count))
	{
		return {};
	}
	for (uint32_t i = 0; i < count; ++i)
	{
		uint64_t id = 0;
		uint32_t size = 0;
		if (!ReadValue(bytes, id) || !ReadValue(bytes, size) || bytes.size() < size)
		{
			return {};
		}
		caches.emplace(id, std::vector<std::byte>(bytes.begin(), bytes.begin() + size));
		bytes = bytes.subspan(size);
	}
	return caches;
}

std::vector<std::byte> WritePerPipeline(const std::map<uint64_t, std::vector<std::byte>>& caches)
{
	std::vector<std::byte> bytes;
	WriteValue(bytes, k_PerPipelineMagic);
	WriteValue(bytes, k_PerPipelineVersion);
	WriteValue(bytes, static_cast<uint32_t>(caches.size()));
	for (const auto& [id, cache] : caches)
	{
		WriteValue(bytes, id);
		WriteValue(bytes, static_cast<uint32_t>(cache.size()));
		bytes.insert(bytes.end(), cache.begin(), cache.end());
	}
	return bytes;
}

} // namespace

PipelineCache::PipelineCache(std::filesystem::path path, Kind kind)
    : _path(std::move(path))
    , _kind(kind)
{
	std::error_code error;
	const auto size = std::filesystem::file_size(_path, error);
	if (error || size == 0)
	{
		return;
	}
	std::ifstream file(_path, std::ios::binary);
	std::vector<std::byte> bytes(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
	{
		return;
	}
	if (_kind == Kind::Shared)
	{
		_caches.emplace(0, std::move(bytes));
	}
	else
	{
		_caches = ParsePerPipeline(bytes);
	}
}

PipelineCache::~PipelineCache()
{
	if (!_changed || _caches.empty())
	{
		return;
	}
	const auto bytes = _kind == Kind::Shared ? _caches.begin()->second : WritePerPipeline(_caches);
	std::error_code error;
	std::filesystem::create_directories(_path.parent_path(), error);
	// The bytes go into a different file first, so a run that stops during the write never leaves part of a cache
	auto partial = _path;
	partial += ".part";
	{
		std::ofstream file(partial, std::ios::binary | std::ios::trunc);
		if (!file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
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

uint32_t PipelineCache::ReadSize(uint64_t id) const
{
	const auto found = _caches.find(KeyOf(id));
	return found != _caches.end() ? static_cast<uint32_t>(found->second.size()) : 0;
}

bool PipelineCache::Read(uint64_t id, std::span<std::byte> data) const
{
	const auto found = _caches.find(KeyOf(id));
	if (found == _caches.end() || data.size() != found->second.size())
	{
		return false;
	}
	std::ranges::copy(found->second, data.begin());
	return true;
}

void PipelineCache::Write(uint64_t id, std::span<const std::byte> data)
{
	// A shared driver cache started from the kept pipelines, so it has all of them and the new one. When the driver
	// cannot use the kept pipelines (for example, after a driver update), it starts again from none.
	auto& cache = _caches[KeyOf(id)];
	if (std::ranges::equal(cache, data))
	{
		return;
	}
	cache.assign(data.begin(), data.end());
	_changed = true;
}
