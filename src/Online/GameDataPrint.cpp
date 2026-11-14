/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameDataPrint.h"

#include <cctype>

#include <algorithm>
#include <array>
#include <map>
#include <ranges>

#include <fmt/format.h>

namespace openblack::online::game_data
{
namespace
{
constexpr uint64_t k_HashOffset = 14695981039346656037ull;
constexpr uint64_t k_HashPrime = 1099511628211ull;
/// The start and the end of the names of the patch notes, such as "readme-patch-v120.txt"
constexpr std::string_view k_PatchNoteStart = "readme-patch-v";
constexpr std::string_view k_PatchNoteEnd = ".txt";

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

bool IsDigits(std::string_view text)
{
	return !text.empty() && std::ranges::all_of(text, [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool IsHexDigits(std::string_view text)
{
	return !text.empty() && std::ranges::all_of(text, [](unsigned char c) { return std::isxdigit(c) != 0; });
}

/// "120" gives "1.20"
std::string PatchName(std::string_view digits)
{
	return fmt::format("{}.{}", digits.substr(0, 1), digits.substr(1));
}

std::string Words(const FilePrint& file)
{
	return file.present ? fmt::format("size {}, hash {:016x}", file.size, file.hash) : std::string("no file");
}
} // namespace

uint64_t HashOf(std::span<const uint8_t> bytes) noexcept
{
	auto hash = k_HashOffset;
	for (const auto byte : bytes)
	{
		hash ^= byte;
		hash *= k_HashPrime;
	}
	return hash;
}

bool IsPrintPath(std::string_view path) noexcept
{
	if (path.empty() || path.size() > k_MostPrintPathBytes || path.front() == '/' ||
	    path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos)
	{
		return false;
	}
	for (const auto part : std::views::split(path, '/'))
	{
		const std::string_view name(part.begin(), part.end());
		if (name.empty() || name == "..")
		{
			return false;
		}
	}
	return true;
}

std::vector<std::string> PrintPaths(std::span<const std::filesystem::path> readFiles)
{
	std::vector<std::string> paths;
	paths.reserve(readFiles.size());
	for (const auto& file : readFiles)
	{
		auto path = file.generic_string();
		if (IsPrintPath(path) && KindOfPath(path) != FileKind::Presentation)
		{
			paths.push_back(std::move(path));
		}
	}
	// The core files come first, so that the limit never drops them
	std::ranges::sort(paths, [](const std::string& a, const std::string& b) {
		const bool coreA = KindOfPath(a) == FileKind::Core;
		const bool coreB = KindOfPath(b) == FileKind::Core;
		return coreA != coreB ? coreA : a < b;
	});
	const auto [first, last] = std::ranges::unique(paths);
	paths.erase(first, last);
	if (paths.size() > k_MostPrintFiles)
	{
		paths.resize(k_MostPrintFiles);
	}
	return paths;
}

FilePrint PrintOf(std::string path, const std::optional<std::vector<uint8_t>>& bytes)
{
	if (!bytes.has_value())
	{
		return {.path = std::move(path)};
	}
	return {.path = std::move(path), .present = true, .size = bytes->size(), .hash = HashOf(*bytes)};
}

InstallHints HintsOf(std::span<const std::string> topNames)
{
	InstallHints hints;
	std::string bestPatch;
	for (const auto& name : topNames)
	{
		const auto lower = Lower(name);
		if (lower.starts_with(k_PatchNoteStart) && lower.ends_with(k_PatchNoteEnd))
		{
			const auto digits = std::string_view(lower).substr(k_PatchNoteStart.size(),
			                                                   lower.size() - k_PatchNoteStart.size() - k_PatchNoteEnd.size());
			// The longest number is the last patch, and of two of the same length the larger one
			if (digits.size() >= 2 && IsDigits(digits) &&
			    (digits.size() > bestPatch.size() || (digits.size() == bestPatch.size() && digits > bestPatch)))
			{
				bestPatch = std::string(digits);
			}
		}
		// The sound banks of the language are named by the locale, such as "00000809.16" for English (United Kingdom)
		const auto dot = lower.find('.');
		if (hints.language.empty() && dot == 8 && IsHexDigits(std::string_view(lower).substr(0, 8)) &&
		    IsDigits(std::string_view(lower).substr(dot + 1)))
		{
			hints.language = lower.substr(4, 4);
		}
	}
	if (!bestPatch.empty())
	{
		hints.patch = PatchName(bestPatch);
	}
	return hints;
}

uint64_t HashOfPrint(std::span<const FilePrint> files) noexcept
{
	auto hash = k_HashOffset;
	const auto mix = [&hash](uint64_t value) {
		for (int shift = 0; shift < 64; shift += 8)
		{
			hash ^= (value >> shift) & 0xffu;
			hash *= k_HashPrime;
		}
	};
	for (const auto& file : files)
	{
		mix(HashOf({reinterpret_cast<const uint8_t*>(file.path.data()), file.path.size()}));
		mix(file.present ? 1 : 0);
		mix(file.size);
		mix(file.hash);
	}
	return hash;
}

std::optional<Difference> FirstDifference(std::span<const FilePrint> server, std::span<const FilePrint> player)
{
	std::map<std::string_view, const FilePrint*> given;
	for (const auto& file : player)
	{
		given.emplace(file.path, &file);
	}
	for (const auto& file : server)
	{
		const auto found = given.find(file.path);
		if (found == given.end())
		{
			return Difference {.kind = DifferenceKind::NotGiven, .server = file, .player = {.path = file.path}};
		}
		const auto& theirs = *found->second;
		if (theirs == file)
		{
			continue;
		}
		const auto kind = theirs.present || !file.present ? DifferenceKind::Different : DifferenceKind::Missing;
		return Difference {.kind = kind, .server = file, .player = theirs};
	}
	return std::nullopt;
}

FileKind KindOfPath(std::string_view path) noexcept
{
	const auto lower = Lower(path);
	const auto slash = lower.rfind('/');
	const auto name = slash == std::string::npos ? std::string_view(lower) : std::string_view(lower).substr(slash + 1);
	const auto dot = name.rfind('.');
	const auto extension = dot == std::string_view::npos ? std::string_view() : name.substr(dot);
	// The tables, the lands, the maps, the scripts and the data of the creatures, the zones, the dances and the walks
	constexpr std::array<std::string_view, 10> k_CoreExtensions {".lnd", ".map", ".chl", ".lhw", ".anm",
	                                                             ".exc", ".edt", ".cbn", ".fot", ".dat"};
	if (name == "info.dat" || std::ranges::find(k_CoreExtensions, extension) != k_CoreExtensions.end() ||
	    lower.starts_with("scripts/"))
	{
		return FileKind::Core;
	}
	// The sounds, the music, the textures, the videos, the glows, the fonts and the help sprites
	constexpr std::array<std::string_view, 15> k_PresentationExtensions {
	    ".sad", ".wav", ".mp3", ".ogg", ".raw", ".16b", ".555", ".bik", ".glw", ".hd", ".bmp", ".tga", ".png", ".jpg", ".ttf"};
	if (lower.starts_with("audio/") || std::ranges::find(k_PresentationExtensions, extension) != k_PresentationExtensions.end())
	{
		return FileKind::Presentation;
	}
	return FileKind::Sim;
}

bool RefusesJoin(std::string_view path) noexcept
{
	return KindOfPath(path) == FileKind::Core;
}

std::string Describe(const Difference& difference)
{
	switch (difference.kind)
	{
	case DifferenceKind::Missing:
		return fmt::format("{} is missing (the server has {})", difference.server.path, Words(difference.server));
	case DifferenceKind::NotGiven:
		return fmt::format("{} has no print from the player (the server has {})", difference.server.path,
		                   Words(difference.server));
	case DifferenceKind::Different:
		break;
	}
	return fmt::format("{} is not the server's ({}; the server has {})", difference.server.path, Words(difference.player),
	                   Words(difference.server));
}

} // namespace openblack::online::game_data
