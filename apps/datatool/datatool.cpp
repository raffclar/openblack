/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A tool that looks inside the game's smaller files that have no tool of their own: gesture templates, particle effect
// files, raw images, the info tables, fonts, profiles and saved games, sound placements, dances and footpaths.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Bitmap.h>
#include <DanceFile.h>
#include <DanceShapeFile.h>
#include <GestureFile.h>
#include <PackFile.h>
#include <ParticleFile.h>
#include <RawImage.h>
#include <SASFile.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "DataReport.h"
#include "FileSystem/MemoryStream.h"
#include "Gui/GameFont.h"
#include "Serializer/GameThingSerializer.h"

using namespace openblack;

namespace
{
constexpr std::string_view k_Usage =
    R"(usage: datatool COMMAND [OPTION...] FILE...
Looks inside the game's files that have no tool of their own.

commands:
  gestures FILE            the gesture templates (Data/Gestures.jty); --roundtrip checks that the file writes back
                           byte for byte
  particles FILE           a particle effect file (the loose .txt, or the compressed _txt.zzz); --properties lists
                           every property
  raw FILE                 a raw image or a bitmap; --size WIDTHxHEIGHT (a raw file with no size is taken as square);
                           --grey for one byte a pixel
  info FILE                the info tables of Scripts/info.dat; --table NAME [--row N] [--rows N] dumps the bytes
  font NAME                a font from a folder: reads NAME.met and NAME.fnt; --glyphs lists the glyphs, --draw
                           CHARACTER draws one
  profiles FILE            the players' profiles file
  saveinfo FILE            the small info file of a save slot
  world FILE               a saved world (.sav): its start, its creatures and leashes
  sounds FILE              a sound placement file (SmallSounds.SAS); --frames lists the sounds of each clip
  actions FILE             a header of enumerations, such as Data/SoundAction.h
  dance FILE               a dance file of Scripts/Dance
  shape FILE               a dancers' shape (a .DAN file of the Data folder)
  footpaths FILE           a land's footpath file (.fot)
)";

std::optional<std::vector<uint8_t>> ReadBytes(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open())
	{
		std::fprintf(stderr, "cannot open %s\n", path.string().c_str());
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

struct Options
{
	std::vector<std::string> files;
	std::string size;
	std::string table;
	std::string draw;
	size_t row {0};
	size_t rows {1};
	bool roundTrip {false};
	bool properties {false};
	bool grey {false};
	bool glyphs {false};
	bool frames {false};
};

bool ParseNumber(std::string_view text, size_t& out)
{
	if (text.empty() || !std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; }))
	{
		return false;
	}
	out = static_cast<size_t>(std::stoull(std::string(text)));
	return true;
}

bool ParseOptions(std::span<char*> arguments, Options& options)
{
	for (size_t i = 0; i < arguments.size(); ++i)
	{
		const std::string_view argument = arguments[i];
		const auto value = [&]() -> std::string_view {
			return i + 1 < arguments.size() ? arguments[++i] : std::string_view {};
		};
		if (argument == "--roundtrip")
		{
			options.roundTrip = true;
		}
		else if (argument == "--properties")
		{
			options.properties = true;
		}
		else if (argument == "--grey")
		{
			options.grey = true;
		}
		else if (argument == "--glyphs")
		{
			options.glyphs = true;
		}
		else if (argument == "--frames")
		{
			options.frames = true;
		}
		else if (argument == "--size")
		{
			options.size = value();
		}
		else if (argument == "--table")
		{
			options.table = value();
		}
		else if (argument == "--draw")
		{
			options.draw = value();
		}
		else if (argument == "--row")
		{
			if (!ParseNumber(value(), options.row))
			{
				return false;
			}
		}
		else if (argument == "--rows")
		{
			if (!ParseNumber(value(), options.rows))
			{
				return false;
			}
		}
		else if (argument.starts_with("--"))
		{
			return false;
		}
		else
		{
			options.files.emplace_back(argument);
		}
	}
	return !options.files.empty();
}

int Gestures(const std::filesystem::path& path, const Options& options)
{
	const auto bytes = ReadBytes(path);
	gestures::GestureFile file;
	if (!bytes.has_value())
	{
		return EXIT_FAILURE;
	}
	const auto result = file.Open(*bytes);
	if (result != gestures::GestureFileResult::Success)
	{
		std::printf("%s: %s\n", path.string().c_str(), gestures::ResultToStr(result).data());
		return EXIT_FAILURE;
	}
	if (options.roundTrip)
	{
		const bool same = file.Write() == *bytes;
		std::printf("%s: round trip %s\n", path.string().c_str(), same ? "identical" : "DIFFERENT");
		return same ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	std::printf("%s", datatool::DescribeGestures(file).c_str());
	return EXIT_SUCCESS;
}

int Particles(const std::filesystem::path& path, const Options& options)
{
	const auto bytes = ReadBytes(path);
	if (!bytes.has_value())
	{
		return EXIT_FAILURE;
	}
	const auto text = datatool::ParticleText(*bytes, path.extension() == ".zzz");
	const auto file = text.has_value() ? psys::ParticleFile::Parse(*text) : std::nullopt;
	if (!file.has_value())
	{
		std::printf("%s: not a particle file\n", path.string().c_str());
		return EXIT_FAILURE;
	}
	std::printf("%s", datatool::DescribeParticleFile(*file, options.properties).c_str());
	return EXIT_SUCCESS;
}

int Raw(const std::filesystem::path& path, const Options& options)
{
	const auto bytes = ReadBytes(path);
	if (!bytes.has_value())
	{
		return EXIT_FAILURE;
	}
	if (path.extension() == ".bmp" || path.extension() == ".BMP")
	{
		const auto image = rawimage::DecodeBitmap(*bytes);
		if (!image.has_value())
		{
			std::printf("%s: cannot read the bitmap\n", path.string().c_str());
			return EXIT_FAILURE;
		}
		std::printf("%s", datatool::DescribeRgbImage(*image).c_str());
		return EXIT_SUCCESS;
	}
	const uint32_t channels = options.grey ? 1 : 3;
	std::optional<std::pair<uint32_t, uint32_t>> size;
	if (!options.size.empty())
	{
		size = datatool::ParseSize(options.size);
	}
	else if (const auto side = datatool::SquareSide(bytes->size(), channels); side.has_value())
	{
		size = std::pair {*side, *side};
	}
	if (!size.has_value())
	{
		std::printf("%s: %zu bytes; give the size with --size WIDTHxHEIGHT\n", path.string().c_str(), bytes->size());
		return EXIT_FAILURE;
	}
	if (options.grey)
	{
		const auto image = rawimage::DecodeGrey(*bytes, size->first, size->second);
		if (!image.has_value())
		{
			std::printf("%s: %zu bytes do not fill %u x %u pixels\n", path.string().c_str(), bytes->size(), size->first,
			            size->second);
			return EXIT_FAILURE;
		}
		std::printf("%s", datatool::DescribeGreyImage(*image).c_str());
		return EXIT_SUCCESS;
	}
	const auto image = rawimage::DecodeRgb(*bytes, size->first, size->second);
	if (!image.has_value())
	{
		std::printf("%s: %zu bytes do not fill %u x %u pixels\n", path.string().c_str(), bytes->size(), size->first,
		            size->second);
		return EXIT_FAILURE;
	}
	std::printf("%s", datatool::DescribeRgbImage(*image).c_str());
	return EXIT_SUCCESS;
}

int Info(const std::filesystem::path& path, const Options& options)
{
	pack::PackFile pack;
	const auto result = pack.Open(path);
	if (result != pack::PackResult::Success)
	{
		std::printf("%s: %s\n", path.string().c_str(), pack::ResultToStr(result).data());
		return EXIT_FAILURE;
	}
	if (!pack.GetBlocks().contains("Info"))
	{
		std::printf("%s: the pack has no Info block\n", path.string().c_str());
		return EXIT_FAILURE;
	}
	const auto& block = pack.GetBlock("Info");
	if (options.table.empty())
	{
		std::printf("%s", datatool::DescribeInfoBlock(block).c_str());
		return EXIT_SUCCESS;
	}
	const auto dump = datatool::DumpInfoTable(block, options.table, options.row, options.rows);
	if (!dump.has_value())
	{
		std::printf("%s: no table %s, or no row %zu in it\n", path.string().c_str(), options.table.c_str(), options.row);
		return EXIT_FAILURE;
	}
	std::printf("%s", dump->c_str());
	return EXIT_SUCCESS;
}

int Font(const std::filesystem::path& base, const Options& options)
{
	auto met = base;
	met += ".met";
	auto fnt = base;
	fnt += ".fnt";
	const auto metBytes = ReadBytes(met);
	const auto fntBytes = ReadBytes(fnt);
	if (!metBytes.has_value() || !fntBytes.has_value())
	{
		return EXIT_FAILURE;
	}
	const auto font = gui::GameFont::Load(*metBytes, *fntBytes);
	if (!font.has_value())
	{
		std::printf("%s: the two files do not fit together\n", base.string().c_str());
		return EXIT_FAILURE;
	}
	if (!options.draw.empty())
	{
		const auto drawn = datatool::DrawGlyph(*font, static_cast<char16_t>(static_cast<unsigned char>(options.draw[0])));
		if (drawn.empty())
		{
			std::printf("the font has no glyph for %s\n", options.draw.c_str());
			return EXIT_FAILURE;
		}
		std::printf("%s", drawn.c_str());
		return EXIT_SUCCESS;
	}
	std::printf("%s", datatool::DescribeFont(*font, options.glyphs).c_str());
	return EXIT_SUCCESS;
}

int Footpaths(const std::filesystem::path& path)
{
	auto bytes = ReadBytes(path);
	if (!bytes.has_value())
	{
		return EXIT_FAILURE;
	}
	filesystem::MemoryStream stream(std::move(*bytes));
	serializer::GameThingSerializer serializer(stream);
	const auto links = serializer.DeserializeList<serializer::GameThingSerializer::FootpathLinkSave>();
	const auto paths = serializer.DeserializeList<serializer::GameThingSerializer::Footpath>();
	std::printf("%s", datatool::DescribeFootpaths(links, paths).c_str());
	return EXIT_SUCCESS;
}

int Run(std::string_view command, const std::filesystem::path& path, const Options& options)
{
	if (command == "gestures")
	{
		return Gestures(path, options);
	}
	if (command == "particles")
	{
		return Particles(path, options);
	}
	if (command == "raw")
	{
		return Raw(path, options);
	}
	if (command == "info")
	{
		return Info(path, options);
	}
	if (command == "font")
	{
		return Font(path, options);
	}
	if (command == "footpaths")
	{
		return Footpaths(path);
	}
	const auto bytes = ReadBytes(path);
	if (!bytes.has_value())
	{
		return EXIT_FAILURE;
	}
	if (command == "profiles")
	{
		std::printf(
		    "%s",
		    datatool::DescribeProfiles(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size())).c_str());
		return EXIT_SUCCESS;
	}
	if (command == "saveinfo")
	{
		std::printf("%s", datatool::DescribeSaveInfo(*bytes).c_str());
		return EXIT_SUCCESS;
	}
	if (command == "world")
	{
		std::printf("%s", datatool::DescribeSavedWorld(*bytes).c_str());
		return EXIT_SUCCESS;
	}
	if (command == "sounds")
	{
		const auto file = sas::Parse(*bytes);
		if (!file.has_value())
		{
			std::printf("%s: not a sound placement file\n", path.string().c_str());
			return EXIT_FAILURE;
		}
		std::printf("%s", datatool::DescribeSounds(*file, options.frames).c_str());
		return EXIT_SUCCESS;
	}
	if (command == "actions")
	{
		std::printf("%s",
		            datatool::DescribeEnumHeader(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()))
		                .c_str());
		return EXIT_SUCCESS;
	}
	if (command == "dance")
	{
		dance::DanceFile file;
		const auto result = file.Open(*bytes);
		if (result != dance::DanceResult::Success)
		{
			std::printf("%s: %s\n", path.string().c_str(), dance::ResultToStr(result).data());
			return EXIT_FAILURE;
		}
		std::printf("%s", datatool::DescribeDance(file).c_str());
		return EXIT_SUCCESS;
	}
	if (command == "shape")
	{
		dance::DanceShapeFile file;
		const auto result = file.Open(*bytes);
		if (result != dance::DanceResult::Success)
		{
			std::printf("%s: %s\n", path.string().c_str(), dance::ResultToStr(result).data());
			return EXIT_FAILURE;
		}
		std::printf("%s", datatool::DescribeDanceShape(file).c_str());
		return EXIT_SUCCESS;
	}
	std::printf("%s", k_Usage.data());
	return EXIT_FAILURE;
}
} // namespace

int main(int argc, char* argv[])
{
	// The readers log to the game's logger, which this tool does not have
	spdlog::register_logger(std::make_shared<spdlog::logger>("game", std::make_shared<spdlog::sinks::null_sink_mt>()));
	if (argc < 3)
	{
		std::printf("%s", k_Usage.data());
		return EXIT_FAILURE;
	}
	Options options;
	if (!ParseOptions(std::span(argv + 2, static_cast<size_t>(argc - 2)), options))
	{
		std::printf("%s", k_Usage.data());
		return EXIT_FAILURE;
	}
	int failures = 0;
	for (const auto& file : options.files)
	{
		failures += Run(argv[1], file, options) == EXIT_SUCCESS ? 0 : 1;
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
