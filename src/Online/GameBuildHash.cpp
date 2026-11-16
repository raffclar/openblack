/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameBuildHash.h"

#include <array>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "Sha256.h"

namespace openblack::online::build_hash
{
namespace
{

/// The bytes that the hash reads from the file at one time
constexpr size_t k_ReadBytes = size_t {1} << 20;

} // namespace

std::optional<std::string> OfFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	sha256::Hasher hasher;
	std::vector<uint8_t> part(k_ReadBytes);
	while (file)
	{
		file.read(reinterpret_cast<char*>(part.data()), static_cast<std::streamsize>(part.size()));
		hasher.Add(std::span<const uint8_t>(part).first(static_cast<size_t>(file.gcount())));
	}
	if (file.bad())
	{
		return std::nullopt;
	}
	return sha256::Hex(hasher.Finish());
}

std::filesystem::path ProgramPath()
{
#if defined(_WIN32)
	std::array<wchar_t, 32768> buffer {};
	const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
	if (length > 0 && length < buffer.size())
	{
		return {std::wstring_view(buffer.data(), length)};
	}
	return {};
#elif defined(__APPLE__)
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::vector<char> buffer(size + 1);
	if (_NSGetExecutablePath(buffer.data(), &size) != 0)
	{
		return {};
	}
	return {buffer.data()};
#elif defined(__EMSCRIPTEN__)
	return {};
#else
	std::error_code error;
	auto path = std::filesystem::read_symlink("/proc/self/exe", error);
	return error ? std::filesystem::path {} : path;
#endif
}

std::shared_future<std::string> OwnLater()
{
#if defined(__EMSCRIPTEN__)
	std::promise<std::string> none;
	none.set_value({});
	return none.get_future().share();
#else
	return std::async(std::launch::async,
	                  []() {
		                  const auto path = ProgramPath();
		                  return path.empty() ? std::string {} : OfFile(path).value_or(std::string {});
	                  })
	    .share();
#endif
}

} // namespace openblack::online::build_hash
