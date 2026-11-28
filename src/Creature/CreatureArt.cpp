/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureArt.h"

#include <cctype>

#include <algorithm>
#include <utility>

#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::creature_art;

namespace
{
/// The files of the folder Data/CTR, in lower case, with the species of each
constexpr std::array<std::pair<std::string_view, CreatureType>, 17> k_AnimationFiles = {{
    {"bcow", CreatureType::Cow},
    {"btiger", CreatureType::Tiger},
    {"bleopard", CreatureType::Leopard},
    {"bwolf", CreatureType::Wolf},
    {"blion", CreatureType::Lion},
    {"bhorse", CreatureType::Horse},
    {"btort", CreatureType::Tortoise},
    {"bzebra", CreatureType::Zebra},
    {"bbear", CreatureType::BrownBear},
    {"bpolarbear", CreatureType::PolarBear},
    {"bsheep", CreatureType::Sheep},
    {"bchimp", CreatureType::Chimp},
    {"bgreek", CreatureType::Ogre},
    {"bmandrill", CreatureType::Mandrill},
    {"brhino", CreatureType::Rhino},
    {"bgorilla", CreatureType::Gorilla},
    {"bape", CreatureType::GiantApe},
}};

bool SameText(std::string_view a, std::string_view b)
{
	return std::ranges::equal(a, b, [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}

bool PushesInteger(const lhvm::VMInstruction& instruction)
{
	return instruction.code == lhvm::Opcode::Push && instruction.mode == lhvm::VMMode::Immediate &&
	       instruction.type == lhvm::DataType::Int;
}

/// Whether the instruction calls a native of `makeNatives` that has the species at the place
bool CallsMakeNative(const lhvm::VMInstruction& instruction, MakeNatives makeNatives, SpeciesPlace place)
{
	return instruction.code == lhvm::Opcode::Sys && std::ranges::any_of(makeNatives, [&instruction, place](const auto& native) {
		       return native.number == instruction.data.uintVal && native.place == place;
	       });
}

std::optional<CreatureType> SpeciesOfRow(int32_t row)
{
	return row >= 0 ? script::property_rules::CreatureTypeFromScript(static_cast<uint32_t>(row)) : std::nullopt;
}

/// The species that a creature which the code makes at the address has, when the push at the address starts the values
/// of a creature that the code makes with a fixed species
std::optional<CreatureType> SpeciesMadeAt(std::span<const lhvm::VMInstruction> code, size_t address, size_t end,
                                          MakeNatives makeNatives)
{
	constexpr auto k_CreatureType = static_cast<int32_t>(script::ObjectType::Creature);
	const auto& first = code[address];
	if (address + 1 >= end || !PushesInteger(first))
	{
		return std::nullopt;
	}
	const auto& second = code[address + 1];
	// The species is the last value, thus the call comes immediately after it
	if (CallsMakeNative(second, makeNatives, SpeciesPlace::Last))
	{
		return SpeciesOfRow(first.data.intVal);
	}
	if (!PushesInteger(second))
	{
		return std::nullopt;
	}
	// The call that makes the creature is on the same line of the script as the first two values
	const bool creature = first.data.intVal == k_CreatureType;
	for (auto next = address + 2; next < end && code[next].line == first.line; ++next)
	{
		const auto& instruction = code[next];
		if (creature && CallsMakeNative(instruction, makeNatives, SpeciesPlace::AfterThingType))
		{
			return SpeciesOfRow(second.data.intVal);
		}
		if (CallsMakeNative(instruction, makeNatives, SpeciesPlace::First))
		{
			return SpeciesOfRow(first.data.intVal);
		}
	}
	return std::nullopt;
}
} // namespace

std::array<entt::id_type, k_MeshLooks.size()> creature_art::MeshIds(CreatureType species)
{
	std::array<entt::id_type, k_MeshLooks.size()> ids {};
	std::ranges::transform(k_MeshLooks, ids.begin(), [species](auto look) { return creature::GetIdFromType(species, look); });
	return ids;
}

CreatureType creature_art::SpeciesOfAnimationFile(std::string_view name)
{
	const auto found = std::ranges::find_if(k_AnimationFiles, [name](const auto& file) { return SameText(file.first, name); });
	return found != k_AnimationFiles.end() ? found->second : CreatureType::Unknown;
}

std::vector<CreatureType> creature_art::SpeciesMade(std::span<const lhvm::VMInstruction> code, ScriptCode script,
                                                    MakeNatives makeNatives, const CodeOfScript& codeOf, bool followRuns,
                                                    std::unordered_set<uint32_t>& scanned)
{
	std::vector<CreatureType> species;
	std::vector<ScriptCode> waiting {script};
	while (!waiting.empty())
	{
		const auto next = waiting.back();
		waiting.pop_back();
		if (!scanned.insert(next.begin).second)
		{
			continue;
		}
		const auto end = std::min<size_t>(next.end, code.size());
		for (size_t address = next.begin; address < end; ++address)
		{
			if (const auto made = SpeciesMadeAt(code, address, end, makeNatives);
			    made.has_value() && std::ranges::find(species, *made) == species.end())
			{
				species.push_back(*made);
			}
			const auto& instruction = code[address];
			if (followRuns && instruction.code == lhvm::Opcode::Run)
			{
				if (const auto run = codeOf(instruction.data.uintVal); run.has_value() && !scanned.contains(run->begin))
				{
					waiting.push_back(*run);
				}
			}
		}
	}
	return species;
}
