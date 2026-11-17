/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GeneratedLand.h"

#include <charconv>

#include <algorithm>
#include <exception>
#include <map>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"

using namespace openblack;
using landgen::countries::Surface;

namespace
{
constexpr int k_CellsPerBlock = 16;
constexpr int k_BlockCornersPerSide = k_CellsPerBlock + 1;
constexpr float k_BlockSize = 160.0f;

/// The materials of the story for the surfaces. Each one is the material of its kind that the lands of the story show
/// most: the sand of the beaches of Land 1, the snow and the ice of the mountains of Land 2, the jungle floor of Land 5.
constexpr std::array<generated_land::MaterialSource, landgen::countries::k_SurfaceCount> k_Sources {{
    {"Land2.lnd", 11}, // wet sand
    {"Land1.lnd", 6},  // sand
    {"Land5.lnd", 0},  // red desert sand
    {"Land1.lnd", 1},  // grass
    {"Land2.lnd", 13}, // lush grass
    {"Land4.lnd", 12}, // dry grass
    {"Land1.lnd", 0},  // earth
    {"Land4.lnd", 21}, // river mud
    {"Land4.lnd", 20}, // wet grass
    {"Land5.lnd", 16}, // forest undergrowth, the jungle floor
    {"Land1.lnd", 5},  // heather
    {"Land4.lnd", 9},  // scrub
    {"Land4.lnd", 2},  // gravel
    {"Land1.lnd", 4},  // solid rock
    {"Land4.lnd", 3},  // dark solid rock
    {"Land2.lnd", 5},  // snow
    {"Land2.lnd", 3},  // snowy mountain tops
    {"Land2.lnd", 0},  // solid ice
}};

using generated_land::ChoiceRange;
using generated_land::Choices;

/// The ranges keep the land inside the limits of the map. Above the highest altitude, the crests of the land are flat.
constexpr std::array<ChoiceRange, 13> k_ChoiceRanges {{
    {"land", "land amount", &Choices::landAmount, -30, 30},
    {"ranges", "mountain ranges", &Choices::ranges, 0, 6},
    {"mountains", "mountain height", &Choices::mountainHeight, 100, 450},
    {"plains", "plain height", &Choices::plainHeight, 20, 120},
    {"hills", "hill height", &Choices::hillHeight, 0, 200},
    {"hollows", "lake hollows", &Choices::hollows, 0, 16},
    {"rain", "rain drops (thousands)", &Choices::rain, 0, 1000},
    {"rivers", "river source flow", &Choices::riverFlow, 300, 3000},
    {"north", "north temperature", &Choices::northTemperature, 0, 100},
    {"south", "south temperature", &Choices::southTemperature, 0, 100},
    {"wind", "wind", &Choices::wind, -1, 7},
    {"massifs", "mountain massifs", &Choices::massifs, -1, 8},
    {"top", "highest ground (m)", &Choices::topHeight, 171, 2700},
}};

/// Reads a whole number that fills all of the text, or gives false
template <typename Number>
bool ReadWhole(std::string_view text, Number& number)
{
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
	return !text.empty() && error == std::errc() && end == text.data() + text.size();
}

/// True when each corner of the block is on the sea bed, so that the block is all open sea
bool AllOpenSea(const landgen::Grid<lnd::LNDCell>& cells, int blockX, int blockZ)
{
	for (int cx = 0; cx < k_BlockCornersPerSide; ++cx)
	{
		for (int cz = 0; cz < k_BlockCornersPerSide; ++cz)
		{
			if (cells.At((blockX * k_CellsPerBlock) + cx, (blockZ * k_CellsPerBlock) + cz).altitude != 0)
			{
				return false;
			}
		}
	}
	return true;
}

// The plan gives the game's numbers of the tribes
static_assert(static_cast<int>(landgen::settlement::Tribe::Celtic) == static_cast<int>(Tribe::CELTIC));
static_assert(static_cast<int>(landgen::settlement::Tribe::Norse) == static_cast<int>(Tribe::NORSE));
static_assert(static_cast<int>(landgen::settlement::Tribe::Tibetan) == static_cast<int>(Tribe::TIBETAN));

/// The town of the land script
constexpr int k_Town = 0;
/// The belief of the town in player one at the start
constexpr float k_TownBelief = 1.0f;
/// The food and wood in the storage pit at the start, as the land script of Land 1 gives the player's first town
constexpr int k_StartFood = 20000;
constexpr int k_StartWood = 30000;
/// The full size of a building, in thousandths
constexpr int k_FullSize = 1000;

/// The end of the name of a kind of building, after the tribe
std::string_view BuildingName(landgen::settlement::Building building)
{
	using landgen::settlement::Building;
	switch (building)
	{
	case Building::TownCentre:
		return "TOWN_CENTRE";
	case Building::StoragePit:
		return "STORAGE_PIT";
	case Building::Creche:
		return "CRECHE";
	case Building::AbodeA:
		return "A";
	case Building::AbodeC:
		return "C";
	case Building::AbodeD:
		return "D";
	case Building::AbodeE:
		return "E";
	case Building::AbodeF:
		return "F";
	}
	return "A";
}

std::string Place(float x, float z)
{
	return fmt::format("\"{:.2f},{:.2f}\"", x, z);
}
} // namespace

std::span<const generated_land::ChoiceRange> generated_land::ChoiceRanges()
{
	return k_ChoiceRanges;
}

generated_land::Choices generated_land::Clamped(Choices choices)
{
	for (const auto& range : k_ChoiceRanges)
	{
		choices.*range.member = std::clamp(choices.*range.member, range.lowest, range.highest);
	}
	return choices;
}

landgen::Settings generated_land::SettingsOf(const Choices& choices)
{
	constexpr float k_Hundredths = 100.0f;
	constexpr int32_t k_DropsPerThousand = 1000;
	const auto clamped = Clamped(choices);
	landgen::Settings settings {.seed = clamped.seed};
	settings.base.landAmount = static_cast<float>(clamped.landAmount) / k_Hundredths;
	settings.base.ranges = clamped.ranges;
	settings.base.mountainHeight = static_cast<float>(clamped.mountainHeight);
	settings.base.plainHeight = static_cast<float>(clamped.plainHeight);
	settings.base.hillHeight = static_cast<float>(clamped.hillHeight);
	settings.base.hollows = clamped.hollows;
	settings.hydraulic.drops = clamped.rain * k_DropsPerThousand;
	settings.rivers.sourceFlow = static_cast<float>(clamped.riverFlow);
	settings.climate.northTemperature = static_cast<float>(clamped.northTemperature) / k_Hundredths;
	settings.climate.southTemperature = static_cast<float>(clamped.southTemperature) / k_Hundredths;
	if (clamped.wind >= 0)
	{
		settings.wind = static_cast<landgen::climate::Wind>(clamped.wind);
	}
	settings.topHeight = static_cast<float>(clamped.topHeight);
	if (clamped.massifs >= 0)
	{
		settings.massifs.countLow = clamped.massifs;
		settings.massifs.countHigh = clamped.massifs;
	}
	return settings;
}

std::string generated_land::PathOf(const Choices& choices)
{
	const Choices first {};
	auto path = std::string(k_PathPrefix) + std::to_string(choices.seed);
	for (const auto& range : k_ChoiceRanges)
	{
		if (choices.*range.member != first.*range.member)
		{
			path += fmt::format(";{}={}", range.key, choices.*range.member);
		}
	}
	return path;
}

std::string generated_land::PathOf(uint64_t seed)
{
	return PathOf(Choices {.seed = seed});
}

std::optional<generated_land::Choices> generated_land::ChoicesOf(std::string_view path)
{
	if (!path.starts_with(k_PathPrefix))
	{
		return std::nullopt;
	}
	path.remove_prefix(k_PathPrefix.size());
	const auto seedEnd = std::min(path.find(';'), path.size());
	Choices choices;
	if (!ReadWhole(path.substr(0, seedEnd), choices.seed))
	{
		return std::nullopt;
	}
	path.remove_prefix(seedEnd);
	// Each part after the seed is ";key=value"
	while (!path.empty())
	{
		path.remove_prefix(1);
		const auto partEnd = std::min(path.find(';'), path.size());
		const auto part = path.substr(0, partEnd);
		path.remove_prefix(partEnd);
		const auto equals = part.find('=');
		if (equals == std::string_view::npos)
		{
			return std::nullopt;
		}
		const auto range = std::ranges::find(k_ChoiceRanges, part.substr(0, equals), &ChoiceRange::key);
		int32_t value = 0;
		if (range == k_ChoiceRanges.end() || !ReadWhole(part.substr(equals + 1), value) || value < range->lowest ||
		    value > range->highest)
		{
			return std::nullopt;
		}
		choices.*range->member = value;
	}
	return choices;
}

std::optional<uint64_t> generated_land::SeedOf(std::string_view path)
{
	const auto choices = ChoicesOf(path);
	return choices.has_value() ? std::optional(choices->seed) : std::nullopt;
}

const std::array<generated_land::MaterialSource, landgen::countries::k_SurfaceCount>& generated_land::MaterialSources()
{
	return k_Sources;
}

std::optional<generated_land::StoryParts> generated_land::ReadStoryParts(filesystem::FileSystemInterface& files)
{
	std::map<std::string_view, lnd::LNDFile> landscapes;
	const auto open = [&files, &landscapes](std::string_view name) -> const lnd::LNDFile* {
		if (const auto it = landscapes.find(name); it != landscapes.end())
		{
			return &it->second;
		}
		const auto path = files.GetPath<filesystem::Path::Landscape>() / name;
		try
		{
			if (!files.Exists(path))
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "A generated land needs the landscape {}", path.generic_string());
				return nullptr;
			}
			auto& file = landscapes[name];
			if (file.Open(files.ReadAll(path)) != lnd::LNDResult::Success)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The landscape {} does not read", path.generic_string());
				landscapes.erase(name);
				return nullptr;
			}
			return &file;
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The landscape {} does not read: {}", path.generic_string(), e.what());
			landscapes.erase(name);
			return nullptr;
		}
	};

	StoryParts parts;
	for (const auto& source : k_Sources)
	{
		const auto* file = open(source.landscape);
		if (file == nullptr || source.material >= file->GetMaterials().size())
		{
			return std::nullopt;
		}
		parts.surfaces.push_back(file->GetMaterials()[source.material]);
	}
	const auto* noiseFile = open(k_NoiseLandscape);
	if (noiseFile == nullptr)
	{
		return std::nullopt;
	}
	const auto& extra = noiseFile->GetExtra();
	parts.noise.assign(extra.noise.texels.begin(), extra.noise.texels.end());
	parts.bump.assign(extra.bump.texels.begin(), extra.bump.texels.end());
	return parts;
}

LandData generated_land::Build(const landgen::GeneratedLand& land, const StoryParts& parts)
{
	LandData data;
	for (int x = 0; x < LandData::k_BlocksPerSide; ++x)
	{
		for (int z = 0; z < LandData::k_BlocksPerSide; ++z)
		{
			if (AllOpenSea(land.cells, x, z))
			{
				continue;
			}
			auto& block = data.blocks.emplace_back();
			for (int cx = 0; cx < k_BlockCornersPerSide; ++cx)
			{
				for (int cz = 0; cz < k_BlockCornersPerSide; ++cz)
				{
					block.cells.at(static_cast<size_t>((cx * k_BlockCornersPerSide) + cz)) =
					    land.cells.At((x * k_CellsPerBlock) + cx, (z * k_CellsPerBlock) + cz);
				}
			}
			block.index = static_cast<uint32_t>(data.blocks.size());
			block.mapX = static_cast<float>(x) * k_BlockSize;
			block.mapZ = static_cast<float>(z) * k_BlockSize;
			block.blockX = static_cast<uint32_t>(x);
			block.blockZ = static_cast<uint32_t>(z);
			data.blockIndexLookup.at(static_cast<size_t>(x * LandData::k_BlocksPerSide + z)) =
			    static_cast<uint16_t>(block.index);
		}
	}
	data.countries = land.countries;
	data.heightCurve = land.heightCurve;
	if (!land.heightCurve.IsLinear() && land.heightUnits.side == landgen::k_MapCorners)
	{
		data.heightUnits = land.heightUnits.values;
	}
	for (const auto surface : land.materials)
	{
		data.materials.push_back(parts.surfaces.at(static_cast<size_t>(surface)));
	}
	data.noise = parts.noise;
	data.bump = parts.bump;
	return data;
}

landgen::Settings generated_land::SettingsFor(uint64_t seed)
{
	return SettingsOf({.seed = seed});
}

std::string_view generated_land::TribeName(landgen::settlement::Tribe tribe)
{
	return k_TribeStrs.at(static_cast<size_t>(tribe));
}

std::string generated_land::SettlementScript(const landgen::settlement::Plan& plan)
{
	using landgen::settlement::Building;
	const auto tribe = TribeName(plan.tribe);
	std::string script;
	const auto line = [&script](const std::string& text) {
		script += text;
		script += '\n';
	};
	line(fmt::format(R"(CREATE_TOWN({}, {}, "PLAYER_ONE", {}, "{}"))", k_Town, Place(plan.townX, plan.townZ),
	                 static_cast<int>(plan.tribe), tribe));
	line(fmt::format(R"(SET_TOWN_BELIEF({}, "PLAYER_ONE", {:.6f}))", k_Town, k_TownBelief));
	for (const auto& field : plan.fields)
	{
		line(fmt::format("CREATE_NEW_TOWN_FIELD({}, {}, 0, 0.000000)", k_Town, Place(field.x, field.z)));
	}
	// The town centre comes after the other buildings, as on the lands of the story
	for (const auto& placed : plan.buildings)
	{
		if (placed.building == Building::TownCentre)
		{
			continue;
		}
		const bool store = placed.building == Building::StoragePit;
		line(fmt::format(R"(CREATE_ABODE({}, {}, "{}_ABODE_{}", {}, {}, {}, {}))", k_Town, Place(placed.x, placed.z), tribe,
		                 BuildingName(placed.building), placed.rotation, k_FullSize, store ? k_StartFood : 0,
		                 store ? k_StartWood : 0));
	}
	for (const auto& placed : plan.buildings)
	{
		if (placed.building == Building::TownCentre)
		{
			line(fmt::format(R"(CREATE_TOWN_CENTRE({}, {}, "{}_ABODE_TOWN_CENTRE", {}, {}, 0))", k_Town,
			                 Place(placed.x, placed.z), tribe, placed.rotation, k_FullSize));
		}
	}
	for (const auto& person : plan.villagers)
	{
		const std::string_view kind = person.kind == landgen::settlement::Villager::Forester ? "FORESTER" : "HOUSEWIFE";
		line(fmt::format(R"(CREATE_TOWN_VILLAGER({}, {}, "{}_{}", {}))", k_Town, Place(person.x, person.z), tribe, kind,
		                 person.age));
	}
	line(fmt::format(R"(CREATE_CITADEL({}, 0, "PLAYER_ONE", {}, {}))", Place(plan.citadelX, plan.citadelZ),
	                 plan.citadelRotation, k_FullSize));
	// The worship site goes to the place of its tribe round the temple, whatever place the line gives
	line(fmt::format(R"(CREATE_WORSHIP_SITE({}, 0, "PLAYER_ONE", "{}", 0, {}))", Place(plan.citadelX, plan.citadelZ), tribe,
	                 k_FullSize));
	return script;
}
