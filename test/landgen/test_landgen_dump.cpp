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

#include <filesystem>
#include <fstream>
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
	out << ",\"start\":[" << land.start.x << ',' << land.start.z << ',' << land.start.radius << ']';
	out << ",\"towns\":" << (land.settlement.buildings.empty() ? 0 : 1);
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
