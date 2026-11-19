/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The developer dump of the land generator for tools/landgen/measure.py. It does nothing unless the environment names
// the folder to write to, so a normal run of the tests skips it. It writes only JSON, never a landscape file.

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <landgen/Generator.h>

#include "3D/GeneratedLand.h"

using namespace openblack;

namespace
{
/// The text of an environment variable, or nothing
std::string EnvironmentOf(const char* name)
{
	const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
	return value == nullptr ? std::string() : std::string(value);
}

/// The seeds in a list that commas divide
std::vector<uint64_t> SeedsOf(const std::string& list)
{
	std::vector<uint64_t> seeds;
	std::stringstream stream(list);
	std::string item;
	while (std::getline(stream, item, ','))
	{
		if (!item.empty())
		{
			seeds.push_back(std::stoull(item));
		}
	}
	return seeds;
}

template <typename Container, typename Value>
void WriteList(std::ofstream& out, const Container& values, Value&& value)
{
	out << '[';
	bool first = true;
	for (const auto& entry : values)
	{
		out << (first ? "" : ",") << value(entry);
		first = false;
	}
	out << ']';
}

void WriteLand(const std::filesystem::path& path, uint64_t seed, const landgen::GeneratedLand& land)
{
	std::ofstream out(path);
	out << "{\"seed\":" << seed << ",\"side\":" << land.cells.side;
	out << ",\"altitude\":";
	WriteList(out, land.cells.values, [](const lnd::LNDCell& cell) { return static_cast<int>(cell.altitude); });
	out << ",\"metres\":[";
	for (int altitude = 0; altitude <= 255; ++altitude)
	{
		out << (altitude == 0 ? "" : ",") << land.heightCurve.MetresOf(static_cast<float>(altitude));
	}
	out << ']';
	out << ",\"units\":";
	WriteList(out, land.heightUnits.values, [](uint16_t units) { return static_cast<int>(units); });
	out << ",\"water\":";
	WriteList(out, land.cells.values, [](const lnd::LNDCell& cell) { return static_cast<int>(cell.properties.hasWater); });
	out << ",\"country\":";
	WriteList(out, land.cells.values, [](const lnd::LNDCell& cell) { return static_cast<int>(cell.properties.country); });
	out << ",\"countries\":[";
	for (size_t i = 0; i < land.countries.size(); ++i)
	{
		out << (i == 0 ? "" : ",");
		WriteList(out, land.countries[i].materials, [](const lnd::LNDMapMaterial& material) {
			return std::to_string(material.indices[0]) + ',' + std::to_string(material.indices[1]) + ',' +
			       std::to_string(material.coefficient);
		});
	}
	out << "],\"surfaces\":";
	WriteList(out, land.materials, [](landgen::countries::Surface surface) { return static_cast<int>(surface); });
	out << ",\"features\":";
	WriteList(out, land.features, [](const landgen::features::Feature& feature) {
		std::ostringstream text;
		text << '[' << static_cast<int>(feature.kind) << ',' << static_cast<int>(feature.type) << ',' << feature.x << ','
		     << feature.z << ',' << feature.angle << ',' << feature.size << ',' << feature.maxSize << ',' << feature.forest
		     << ',' << feature.xAngle << ',' << feature.zAngle << ',' << feature.altitude << ']';
		return text.str();
	});
	out << ",\"forests\":";
	WriteList(out, land.forests, [](const landgen::features::Forest& forest) {
		std::ostringstream text;
		text << '[' << forest.id << ',' << forest.x << ',' << forest.z << ']';
		return text.str();
	});
	out << ",\"rivers\":" << land.rivers.size() << ",\"lakes\":" << land.lakes;
	out << ",\"highLakes\":";
	WriteList(out, land.highLakes, [&land](const landgen::lakes::Lake& lake) {
		const auto first = lake.corners.front();
		return "[" + std::to_string(lake.level) + "," + std::to_string(lake.corners.size()) + "," +
		       std::to_string(land.cells.XOf(first)) + "," + std::to_string(land.cells.ZOf(first)) + "]";
	});
	out << ",\"start\":[" << land.start.x << ',' << land.start.z << ',' << land.start.radius << ']';
	out << ",\"towns\":" << (land.settlement.town.buildings.empty() ? 0 : 1) + land.settlement.neutral.size();
	// Each town: x and z in metres, the tribe, 1 for a neutral town, the buildings, the houses, the fields, the people,
	// the fish farms, the wonders, the creches, the storage pits and the kinds of house. Planned buildings count too.
	const auto isHouse = [](landgen::settlement::Building building) {
		return std::ranges::find(landgen::settlement::k_AllHouses, building) != landgen::settlement::k_AllHouses.end();
	};
	const auto countOf = [](const landgen::settlement::Town& town, landgen::settlement::Building building) {
		return std::ranges::count_if(town.buildings, [building](const auto& placed) { return placed.building == building; });
	};
	const auto town = [&isHouse, &countOf](const landgen::settlement::Town& entry, int neutral) {
		using landgen::settlement::Building;
		std::set<Building> kinds;
		for (const auto& placed : entry.buildings)
		{
			if (isHouse(placed.building))
			{
				kinds.insert(placed.building);
			}
		}
		const auto houses =
		    std::ranges::count_if(entry.buildings, [&isHouse](const auto& placed) { return isHouse(placed.building); });
		std::ostringstream text;
		text << '[' << entry.x << ',' << entry.z << ',' << static_cast<int>(entry.tribe) << ',' << neutral << ','
		     << entry.buildings.size() << ',' << houses << ',' << entry.fields.size() << ',' << entry.villagers.size() << ','
		     << entry.fishFarms.size() << ',' << countOf(entry, Building::Wonder) << ',' << countOf(entry, Building::Creche)
		     << ',' << countOf(entry, Building::StoragePit) << ',' << kinds.size() << ']';
		return text.str();
	};
	out << ",\"town_list\":[" << town(land.settlement.town, 0);
	for (const auto& entry : land.settlement.neutral)
	{
		out << ',' << town(entry, 1);
	}
	out << ']';
	out << ",\"mists\":";
	WriteList(out, land.mists, [](const landgen::cursed::Mist& mist) {
		std::ostringstream text;
		text << '[' << mist.x << ',' << mist.z << ',' << mist.altitude << ',' << mist.colour << ',' << mist.size << ','
		     << mist.edgeShrink << ']';
		return text.str();
	});
	out << ",\"footpaths\":0}\n";
}
} // namespace

TEST(LandgenDump, WritesTheLandOfEachSeedWhenTheEnvironmentAsksForIt)
{
	const auto folder = EnvironmentOf("OPENBLACK_LANDGEN_DUMP_DIR");
	if (folder.empty())
	{
		GTEST_SKIP() << "OPENBLACK_LANDGEN_DUMP_DIR is not set";
	}
	std::filesystem::create_directories(folder);
	// The stands of trees in hundredths of the usual stands, for the tree benchmarks
	const auto trees = EnvironmentOf("OPENBLACK_LANDGEN_TREES");
	for (const auto seed :
	     SeedsOf(EnvironmentOf("OPENBLACK_LANDGEN_SEEDS").empty() ? "1,7,42" : EnvironmentOf("OPENBLACK_LANDGEN_SEEDS")))
	{
		const auto land = landgen::Generate(
		    generated_land::SettingsOf({.seed = seed, .trees = trees.empty() ? 100 : static_cast<int32_t>(std::stoi(trees))}));
		WriteLand(std::filesystem::path(folder) / ("seed" + std::to_string(seed) + ".json"), seed, land);
	}
}
