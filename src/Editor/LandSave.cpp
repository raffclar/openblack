/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandSave.h"

#include <array>

#include "Common/X87.h"
#include "LHScriptX/FeatureScriptWriter.h"

namespace openblack::editor::land_save
{

namespace writer = lhscriptx::writer;

size_t CopyCells(LandData& land, std::span<const lnd::LNDBlock> blocks)
{
	size_t copied = 0;
	for (const auto& block : blocks)
	{
		if (block.blockX >= LandData::k_BlocksPerSide || block.blockZ >= LandData::k_BlocksPerSide)
		{
			continue;
		}
		const auto index = land.blockIndexLookup.at(block.blockX * LandData::k_BlocksPerSide + block.blockZ);
		if (index == 0 || index > land.blocks.size())
		{
			continue;
		}
		land.blocks.at(index - 1u).cells = block.cells;
		++copied;
	}
	return copied;
}

std::string TreeText(const TreeLine& tree)
{
	const std::array<writer::Argument, 7> arguments {
	    tree.forest,   writer::Place {writer::PlaceText(tree.place)},
	    tree.type,     static_cast<int32_t>(tree.nonScenic ? 1 : 0),
	    tree.yAngle,   tree.scale,
	    tree.maxScale,
	};
	return writer::CommandLine("CREATE_NEW_TREE", "NANNFFF", arguments);
}

std::string FeatureText(const FeatureLine& feature)
{
	constexpr double k_Thousandths = 1000.0;
	const std::array<writer::Argument, 5> arguments {
	    writer::Place {writer::PlaceText(feature.place)},
	    writer::Name {feature.name},
	    static_cast<int32_t>(x87::Mul(feature.yAngle, k_Thousandths)),
	    static_cast<int32_t>(x87::Mul(feature.scale, k_Thousandths)),
	    int32_t {0},
	};
	return writer::CommandLine("CREATE_NEW_FEATURE", "ALNNN", arguments);
}

std::string StreamText(int32_t id, std::span<const glm::ivec2> places)
{
	std::string text = writer::CommandLine("CREATE_STREAM", "N", std::array<writer::Argument, 1> {id});
	for (const auto place : places)
	{
		text += writer::CommandLine("CREATE_STREAM_POINT", "NA",
		                            std::array<writer::Argument, 2> {id, writer::Place {writer::PlaceText(place)}});
	}
	return text;
}

std::string FootpathText(std::span<const glm::ivec2> places)
{
	// The command makes the new footpath the script's footpath 0, whatever number the line gives
	std::string text = writer::CommandLine("CREATE_FOOTPATH", "N", std::array<writer::Argument, 1> {int32_t {0}});
	for (const auto place : places)
	{
		text += writer::CommandLine("CREATE_FOOTPATH_NODE", "NA",
		                            std::array<writer::Argument, 2> {int32_t {0}, writer::Place {writer::PlaceText(place)}});
	}
	return text;
}

std::string FileName(std::string_view landName, std::string_view extension)
{
	std::string name {landName.empty() ? std::string_view("land") : landName};
	name += '.';
	name += extension;
	return name;
}

} // namespace openblack::editor::land_save
