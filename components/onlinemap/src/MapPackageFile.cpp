/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapPackageFile.h"

#include <algorithm>
#include <limits>

#include <nlohmann/json.hpp>

namespace
{
using openblack::onlinemap::package::k_Magic;

constexpr size_t k_LengthSize = 4;
constexpr size_t k_HeaderSize = k_Magic.size() + (4 * k_LengthSize);

uint32_t ReadNumber(std::span<const std::byte> bytes)
{
	uint32_t value = 0;
	for (size_t i = 0; i < k_LengthSize; ++i)
	{
		value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
	}
	return value;
}

void WriteNumber(std::vector<uint8_t>& out, size_t number)
{
	for (size_t i = 0; i < k_LengthSize; ++i)
	{
		out.push_back(static_cast<uint8_t>((number >> (8 * i)) & 0xFF));
	}
}

std::string ManifestText(const openblack::onlinemap::package::Manifest& manifest)
{
	auto needs = nlohmann::json::array();
	for (const auto& need : manifest.needs)
	{
		needs.push_back({{"file", need.file}, {"god", need.god}});
	}
	// The keys of a JSON object are in the order of their names, so the same manifest always gives the same text
	const nlohmann::json json {
	    {"format", openblack::onlinemap::package::k_Format},
	    {"file", manifest.file},
	    {"name", manifest.name},
	    {"players", manifest.players},
	    {"needs", needs},
	    {"frame_sha256", manifest.frameSha256},
	    {"landscape_sha256", manifest.landscapeSha256},
	};
	return json.dump();
}

bool ReadText(const nlohmann::json& object, const char* key, std::string& out)
{
	const auto found = object.find(key);
	if (found == object.end() || !found->is_string())
	{
		return false;
	}
	out = found->get<std::string>();
	return true;
}

std::optional<openblack::onlinemap::package::Manifest> ParseManifest(std::string_view text)
{
	const auto json = nlohmann::json::parse(text, nullptr, false);
	if (json.is_discarded() || !json.is_object())
	{
		return std::nullopt;
	}
	const auto format = json.find("format");
	const auto players = json.find("players");
	const auto needs = json.find("needs");
	if (format == json.end() || !format->is_number_unsigned() ||
	    format->get<uint32_t>() != openblack::onlinemap::package::k_Format || players == json.end() ||
	    !players->is_number_integer() || needs == json.end() || !needs->is_array())
	{
		return std::nullopt;
	}
	openblack::onlinemap::package::Manifest manifest;
	if (!ReadText(json, "file", manifest.file) || !ReadText(json, "name", manifest.name) ||
	    !ReadText(json, "frame_sha256", manifest.frameSha256) || !ReadText(json, "landscape_sha256", manifest.landscapeSha256))
	{
		return std::nullopt;
	}
	const auto gods = players->get<int64_t>();
	if (gods < 1 || gods > std::numeric_limits<int32_t>::max())
	{
		return std::nullopt;
	}
	manifest.players = static_cast<int32_t>(gods);
	for (const auto& item : *needs)
	{
		openblack::onlinemap::package::Need need;
		if (!item.is_object() || !ReadText(item, "file", need.file) || !ReadText(item, "god", need.god))
		{
			return std::nullopt;
		}
		manifest.needs.push_back(std::move(need));
	}
	return manifest;
}
} // namespace

std::optional<std::vector<uint8_t>> openblack::onlinemap::package::Write(const MapPackage& package)
{
	constexpr size_t k_MostLength = std::numeric_limits<uint32_t>::max();
	const auto manifest = ManifestText(package.manifest);
	if (manifest.size() > k_MostLength || package.frame.size() > k_MostLength || package.landscape.size() > k_MostLength)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> out;
	out.reserve(k_HeaderSize + manifest.size() + package.frame.size() + package.landscape.size());
	out.insert(out.end(), k_Magic.begin(), k_Magic.end());
	WriteNumber(out, k_Format);
	WriteNumber(out, manifest.size());
	WriteNumber(out, package.frame.size());
	WriteNumber(out, package.landscape.size());
	out.insert(out.end(), manifest.begin(), manifest.end());
	out.insert(out.end(), package.frame.begin(), package.frame.end());
	out.insert(out.end(), package.landscape.begin(), package.landscape.end());
	return out;
}

std::optional<openblack::onlinemap::package::MapPackage> openblack::onlinemap::package::Read(std::span<const std::byte> bytes)
{
	if (bytes.size() < k_HeaderSize || !std::ranges::equal(bytes.first(k_Magic.size()), k_Magic,
	                                                       [](std::byte a, char b) { return a == static_cast<std::byte>(b); }))
	{
		return std::nullopt;
	}
	auto at = k_Magic.size();
	const auto next = [&bytes, &at] {
		const auto value = ReadNumber(bytes.subspan(at, k_LengthSize));
		at += k_LengthSize;
		return static_cast<uint64_t>(value);
	};
	const auto format = next();
	const auto manifestLength = next();
	const auto frameLength = next();
	const auto landscapeLength = next();
	// The sum is 64 bits wide, so that large lengths do not wrap round to the size of the bytes on a 32-bit build
	if (format != k_Format ||
	    static_cast<uint64_t>(bytes.size() - k_HeaderSize) != manifestLength + frameLength + landscapeLength)
	{
		return std::nullopt;
	}
	const auto text = [&bytes](uint64_t start, uint64_t length) {
		const auto part = bytes.subspan(start, length);
		return std::string_view(reinterpret_cast<const char*>(part.data()), part.size());
	};
	auto manifest = ParseManifest(text(k_HeaderSize, manifestLength));
	if (!manifest.has_value())
	{
		return std::nullopt;
	}
	const auto landscape = bytes.subspan(k_HeaderSize + manifestLength + frameLength, landscapeLength);
	MapPackage package {
	    .manifest = std::move(*manifest),
	    .frame = std::string(text(k_HeaderSize + manifestLength, frameLength)),
	    .landscape = {},
	};
	package.landscape.resize(landscape.size());
	std::ranges::transform(landscape, package.landscape.begin(), [](std::byte b) { return static_cast<uint8_t>(b); });
	return package;
}
