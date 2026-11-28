/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <map>
#include <optional>
#include <set>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureArt.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::creature_art;
using lhvm::DataType;
using lhvm::Opcode;
using lhvm::VMInstruction;
using lhvm::VMMode;
using lhvm::VMValue;

namespace
{
constexpr uint32_t k_Create = 27;
constexpr uint32_t k_CreateWithAngle = 252;
constexpr uint32_t k_GetPosition = 23;
constexpr uint32_t k_LoadCreature = 329;
constexpr uint32_t k_CreateRelative = 69;
constexpr std::array<MakeNative, 4> k_Makes = {{
    {.number = k_Create, .place = SpeciesPlace::AfterThingType},
    {.number = k_CreateWithAngle, .place = SpeciesPlace::AfterThingType},
    {.number = k_LoadCreature, .place = SpeciesPlace::First},
    {.number = k_CreateRelative, .place = SpeciesPlace::Last},
}};
/// The creature type of the scripts
constexpr int32_t k_Creature = 12;

VMInstruction PushInt(int32_t value, uint32_t line)
{
	return {Opcode::Push, VMMode::Immediate, DataType::Int, VMValue(value), line};
}

VMInstruction PushVariable(uint32_t variable, uint32_t line)
{
	return {Opcode::Push, VMMode::Reference, DataType::Float, VMValue(variable), line};
}

VMInstruction Sys(uint32_t native, uint32_t line)
{
	return {Opcode::Sys, VMMode::Immediate, DataType::None, VMValue(native), line};
}

VMInstruction RunScript(uint32_t script, uint32_t line)
{
	return {Opcode::Run, VMMode::Sync, DataType::Int, VMValue(script), line};
}

VMInstruction End(uint32_t line)
{
	return {Opcode::End, VMMode::Immediate, DataType::None, VMValue(0), line};
}

/// The pushes and the call of "create SCRIPT_OBJECT_TYPE_CREATURE <species> at <marker>" on one line
void Create(std::vector<VMInstruction>& code, int32_t type, int32_t species, uint32_t line, uint32_t native = k_Create)
{
	code.push_back(PushInt(type, line));
	code.push_back(PushInt(species, line));
	code.push_back(PushVariable(4, line));
	code.push_back(Sys(k_GetPosition, line));
	code.push_back(Sys(native, line));
}

/// The pushes and the call of "load creature <species> <text> <player> at <marker>" on one line
void LoadCreature(std::vector<VMInstruction>& code, int32_t species, uint32_t line)
{
	constexpr int32_t k_TextOffset = 348;
	code.push_back(PushInt(species, line));
	code.push_back(PushInt(k_TextOffset, line));
	code.push_back(PushVariable(2, line));
	code.push_back(PushVariable(4, line));
	code.push_back(Sys(k_GetPosition, line));
	code.push_back(Sys(k_LoadCreature, line));
}

/// The pushes and the call of "create creature from creature <creature> <scale> at <marker> <species>" on one line
void CreateRelative(std::vector<VMInstruction>& code, int32_t species, uint32_t line)
{
	code.push_back(PushVariable(1, line));
	code.push_back(PushVariable(3, line));
	code.push_back(PushVariable(4, line));
	code.push_back(Sys(k_GetPosition, line));
	code.push_back(PushInt(species, line));
	code.push_back(Sys(k_CreateRelative, line));
}

CodeOfScript NoScripts()
{
	return [](uint32_t) -> std::optional<ScriptCode> { return std::nullopt; };
}
} // namespace

TEST(CreatureArt, EachAnimationFileHasItsSpecies)
{
	EXPECT_EQ(SpeciesOfAnimationFile("Bape"), CreatureType::GiantApe);
	EXPECT_EQ(SpeciesOfAnimationFile("BPolarBear"), CreatureType::PolarBear);
	EXPECT_EQ(SpeciesOfAnimationFile("bbear"), CreatureType::BrownBear);
	EXPECT_EQ(SpeciesOfAnimationFile("Bgreek"), CreatureType::Ogre);
	EXPECT_EQ(SpeciesOfAnimationFile("btort"), CreatureType::Tortoise);
	EXPECT_EQ(SpeciesOfAnimationFile("BCOW"), CreatureType::Cow);
	// The hand's file and other names have no species
	EXPECT_EQ(SpeciesOfAnimationFile("hh"), CreatureType::Unknown);
	EXPECT_EQ(SpeciesOfAnimationFile(""), CreatureType::Unknown);
	EXPECT_EQ(SpeciesOfAnimationFile("bcows"), CreatureType::Unknown);
}

TEST(CreatureArt, TheFilesNameEverySpeciesOneTime)
{
	constexpr std::array k_Files = {"bcow",   "btiger",    "bleopard", "bwolf",      "blion",  "bhorse",
	                                "btort",  "bzebra",    "bbear",    "bpolarbear", "bsheep", "bchimp",
	                                "bgreek", "bmandrill", "brhino",   "bgorilla",   "bape"};
	std::map<CreatureType, int> count;
	for (const auto* file : k_Files)
	{
		++count[SpeciesOfAnimationFile(file)];
	}
	EXPECT_EQ(count.size(), k_Files.size());
	EXPECT_FALSE(count.contains(CreatureType::Unknown));
}

TEST(CreatureArt, TheMeshesOfASpeciesAreItsSevenLooks)
{
	const auto ids = MeshIds(CreatureType::Tiger);
	EXPECT_EQ(ids.front(), creature::GetIdFromMeshName("A_Tiger2_Base"));
	EXPECT_EQ(ids.at(1), creature::GetIdFromMeshName("A_Tiger2_Evil"));
	EXPECT_EQ(ids.back(), creature::GetIdFromMeshName("A_Tiger2_Strong"));
	EXPECT_EQ(std::set(ids.begin(), ids.end()).size(), ids.size());
	EXPECT_NE(MeshIds(CreatureType::Cow).front(), ids.front());
}

TEST(CreatureArt, SkinsComeFromTheBaseEvilAndGoodMeshes)
{
	using enum creature::CreatureBody::Appearance;
	EXPECT_TRUE(PaintsSkinsFrom(Base));
	EXPECT_TRUE(PaintsSkinsFrom(Evil));
	EXPECT_TRUE(PaintsSkinsFrom(Good));
	EXPECT_FALSE(PaintsSkinsFrom(Thin));
	EXPECT_FALSE(PaintsSkinsFrom(Fat));
	EXPECT_FALSE(PaintsSkinsFrom(Weak));
	EXPECT_FALSE(PaintsSkinsFrom(Strong));
}

TEST(CreatureArt, FindsTheFixedSpeciesThatAScriptMakes)
{
	std::vector<VMInstruction> code;
	Create(code, k_Creature, 1, 10);
	Create(code, k_Creature, 0, 11, k_CreateWithAngle);
	// The same species again counts one time
	Create(code, k_Creature, 1, 12);
	code.push_back(End(13));
	std::unordered_set<uint32_t> scanned;
	const auto species = SpeciesMade(code, {0, static_cast<uint32_t>(code.size())}, k_Makes, NoScripts(), true, scanned);
	EXPECT_EQ(species, (std::vector {CreatureType::Cow, CreatureType::GiantApe}));
}

TEST(CreatureArt, IgnoresOtherThingsAndSpeciesThatAreNotFixed)
{
	std::vector<VMInstruction> code;
	// An abode of subtype 3
	Create(code, 2, 3, 20);
	// A creature with the species in a variable
	code.push_back(PushInt(k_Creature, 21));
	code.push_back(PushVariable(7, 21));
	code.push_back(PushVariable(4, 21));
	code.push_back(Sys(k_Create, 21));
	// A creature type that the scripts do not have
	Create(code, k_Creature, 40, 22);
	// The pushes with a call on the next line are not a creature that the code makes
	code.push_back(PushInt(k_Creature, 23));
	code.push_back(PushInt(5, 23));
	code.push_back(Sys(k_Create, 24));
	// A call of another native
	Create(code, k_Creature, 6, 25, 99);
	std::unordered_set<uint32_t> scanned;
	EXPECT_TRUE(SpeciesMade(code, {0, static_cast<uint32_t>(code.size())}, k_Makes, NoScripts(), true, scanned).empty());
}

TEST(CreatureArt, FollowsTheScriptsThatTheScriptRuns)
{
	// Script 1 runs script 2, which runs script 3; script 3 runs script 1 again
	std::vector<VMInstruction> code;
	code.push_back(RunScript(2, 1));
	code.push_back(End(2));
	const auto second = static_cast<uint32_t>(code.size());
	Create(code, k_Creature, 2, 3);
	code.push_back(RunScript(3, 4));
	code.push_back(End(5));
	const auto third = static_cast<uint32_t>(code.size());
	Create(code, k_Creature, 9, 6);
	code.push_back(RunScript(1, 7));
	code.push_back(End(8));
	const auto end = static_cast<uint32_t>(code.size());
	const std::map<uint32_t, ScriptCode> scripts = {{1, {0, second}}, {2, {second, third}}, {3, {third, end}}};
	const CodeOfScript codeOf = [&scripts](uint32_t id) -> std::optional<ScriptCode> {
		const auto found = scripts.find(id);
		return found != scripts.end() ? std::optional(found->second) : std::nullopt;
	};

	std::unordered_set<uint32_t> scanned;
	EXPECT_EQ(SpeciesMade(code, scripts.at(1), k_Makes, codeOf, true, scanned),
	          (std::vector {CreatureType::Tiger, CreatureType::BrownBear}));
	EXPECT_EQ(scanned.size(), 3U);
	// A script that was scanned is not scanned again
	EXPECT_TRUE(SpeciesMade(code, scripts.at(2), k_Makes, codeOf, true, scanned).empty());

	// Without the runs, only the code of the script counts
	std::unordered_set<uint32_t> once;
	EXPECT_TRUE(SpeciesMade(code, scripts.at(1), k_Makes, codeOf, false, once).empty());
	EXPECT_EQ(SpeciesMade(code, scripts.at(2), k_Makes, codeOf, false, once), (std::vector {CreatureType::Tiger}));
}

TEST(CreatureArt, FindsTheSpeciesOfACreatureMadeFromAFileOrFromACreature)
{
	std::vector<VMInstruction> code;
	LoadCreature(code, 4, 30);
	CreateRelative(code, 13, 31);
	code.push_back(End(32));
	std::unordered_set<uint32_t> scanned;
	const auto species = SpeciesMade(code, {0, static_cast<uint32_t>(code.size())}, k_Makes, NoScripts(), true, scanned);
	EXPECT_EQ(species, (std::vector {*script::property_rules::CreatureTypeFromScript(4),
	                                 *script::property_rules::CreatureTypeFromScript(13)}));
}

TEST(CreatureArt, IgnoresAFileOrACreatureCopyWithASpeciesThatIsNotFixed)
{
	std::vector<VMInstruction> code;
	// The species of the file in a variable
	code.push_back(PushVariable(7, 40));
	code.push_back(PushInt(348, 40));
	code.push_back(PushVariable(2, 40));
	code.push_back(Sys(k_LoadCreature, 40));
	// The species of the copy in a variable
	code.push_back(PushVariable(1, 41));
	code.push_back(PushVariable(7, 41));
	code.push_back(Sys(k_CreateRelative, 41));
	// A number before the copy that is not its last value
	code.push_back(PushInt(5, 42));
	code.push_back(PushVariable(7, 42));
	code.push_back(Sys(k_CreateRelative, 42));
	// The values of a file with the call on the next line
	code.push_back(PushInt(5, 43));
	code.push_back(PushInt(348, 43));
	code.push_back(Sys(k_LoadCreature, 44));
	// A species after the creature type for a native that has the species first
	code.push_back(PushInt(k_Creature, 45));
	code.push_back(PushInt(5, 45));
	code.push_back(PushVariable(7, 45));
	code.push_back(Sys(k_CreateRelative, 45));
	code.push_back(End(46));
	std::unordered_set<uint32_t> scanned;
	EXPECT_TRUE(SpeciesMade(code, {0, static_cast<uint32_t>(code.size())}, k_Makes, NoScripts(), true, scanned).empty());
}
