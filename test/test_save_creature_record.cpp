/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <CreatureRecord.h>
#include <SaveGameInfo.h>
#include <SaveSlots.h>
#include <WorldFile.h>
#include <gtest/gtest.h>

using namespace openblack::gamesave;

namespace
{
template <typename T>
T At(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

/// A record with a value in every field, a plan with two steps left, spells queued and villagers that dance
CreatureRecord AFullRecord()
{
	CreatureRecord record;
	record.savedInTemple = 1;
	record.index = 2;
	record.owner = 0;
	record.spells[0] = {.start = 16, .phase = 3, .turnsLeft = 40, .found = 1.5f, .holdTurns = 300, .miracle = 7};
	record.spells[7] = {.start = 16, .phase = 1, .turnsLeft = 20, .found = 0.0f, .holdTurns = -1, .miracle = 8};
	record.queued = {{.spell = 2, .seconds = 30.5f, .miracle = 9}, {.spell = 4, .seconds = -1.0f, .miracle = 0}};
	record.mindProcessing = 0;
	record.largestSize = 2.0f;
	record.smallestSize = 0.4f;
	record.mayGoThroughVortex = 1;
	record.fizzedForGood = 1;
	record.fizz = 0.25f;
	record.fizzTarget = 1.0f;
	record.fizzPerSecond = 0.5f;
	record.freeze = 0.75f;
	record.spellVisuals = {10, 0, 11};
	record.handParticles = {12, 13};
	record.targetPoint = {1.0f, 2.0f, 3.0f};
	record.targetParticles = 14;
	record.targetObject = 15;
	record.hasTarget = 1;
	record.arena = 16;
	record.home = {100.0f, 20.0f, 300.0f};
	record.fightHealth = 0.6f;
	record.autoFight = 2;
	record.dancePartners = {17, 18, 19};
	record.dance = 20;
	record.canDie = 0;
	record.strength = 0.8f;
	record.size = 1.7f;
	record.alignment = -0.3f;
	record.animationPace = 0.5f;
	record.autoScale = 1;
	record.autoScaleShare = 0.9f;
	record.plan = KeptPlan {
	    .desire = 12,
	    .action = 21,
	    .objects = {21, 22, 0},
	    .agenda = {.word08 = 1,
	               .current = 1,
	               .state = 2,
	               .count = 3,
	               .word18 = 4,
	               .word1C = 5,
	               .word20 = 1,
	               .steps = {{.action = 30, .object = 21, .word10 = 1, .point = {4.0f, 5.0f, 6.0f}},
	                         {.action = 31, .object = 22, .word24 = 9, .other = 23, .word30 = 3}},
	               .creature = 24,
	               .other = 0},
	};
	record.creedPowers[1] = {.scale = 0.5f, .power = 2.0f};
	return record;
}
} // namespace

TEST(SaveCreatureRecord, ARecordWithNoListsAndNoPlanHasTheGamesLength)
{
	EXPECT_EQ(Write(CreatureRecord {}).size(), 569u);
}

TEST(SaveCreatureRecord, TheFieldsAreAtTheGamesOffsets)
{
	auto record = AFullRecord();
	record.queued.clear();
	record.dancePartners.clear();
	record.plan.reset();
	const auto bytes = Write(record);
	ASSERT_EQ(bytes.size(), 569u);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 33u);
	EXPECT_EQ(At<uint32_t>(bytes, 4), 1u);
	EXPECT_EQ(At<uint8_t>(bytes, 8), 2u);
	EXPECT_EQ(At<uint8_t>(bytes, 9), 0u);
	// The spell slots: six words each, freeze first
	EXPECT_EQ(At<uint32_t>(bytes, 10), 16u);
	EXPECT_EQ(At<uint32_t>(bytes, 14), 3u);
	EXPECT_EQ(At<int32_t>(bytes, 18), 40);
	EXPECT_EQ(At<float>(bytes, 22), 1.5f);
	EXPECT_EQ(At<int32_t>(bytes, 26), 300);
	EXPECT_EQ(At<uint32_t>(bytes, 30), 7u);
	EXPECT_EQ(At<int32_t>(bytes, 10 + 7 * 24 + 16), -1);
	EXPECT_EQ(At<uint32_t>(bytes, 394), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 398), 0u);
	EXPECT_EQ(At<float>(bytes, 402), 2.0f);
	EXPECT_EQ(At<float>(bytes, 406), 0.4f);
	EXPECT_EQ(At<uint8_t>(bytes, 410), 1u);
	EXPECT_EQ(At<uint8_t>(bytes, 411), 1u);
	EXPECT_EQ(At<float>(bytes, 412), 0.25f);
	EXPECT_EQ(At<float>(bytes, 424), 0.75f);
	EXPECT_EQ(At<uint32_t>(bytes, 432), 10u);
	EXPECT_EQ(At<uint32_t>(bytes, 448), 13u);
	EXPECT_EQ(At<float>(bytes, 460), 3.0f);
	EXPECT_EQ(At<uint32_t>(bytes, 468), 15u);
	EXPECT_EQ(At<uint8_t>(bytes, 472), 1u);
	EXPECT_EQ(At<uint32_t>(bytes, 473), 16u);
	EXPECT_EQ(At<float>(bytes, 477), 100.0f);
	EXPECT_EQ(At<float>(bytes, 489), 0.6f);
	EXPECT_EQ(At<uint32_t>(bytes, 493), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 497), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 501), 20u);
	EXPECT_EQ(At<uint32_t>(bytes, 505), 0u);
	EXPECT_EQ(At<float>(bytes, 509), 0.8f);
	EXPECT_EQ(At<float>(bytes, 513), 1.7f);
	EXPECT_EQ(At<float>(bytes, 517), -0.3f);
	EXPECT_EQ(At<float>(bytes, 521), 0.5f);
	EXPECT_EQ(At<uint32_t>(bytes, 525), 1u);
	EXPECT_EQ(At<float>(bytes, 529), 0.9f);
	EXPECT_EQ(At<uint32_t>(bytes, 533), 0u);
	EXPECT_EQ(At<float>(bytes, 537 + 8), 0.5f);
	EXPECT_EQ(At<float>(bytes, 537 + 12), 2.0f);
}

TEST(SaveCreatureRecord, AFullRecordReadsBackAsItWasWritten)
{
	const auto record = AFullRecord();
	const auto bytes = Write(record);
	// The plan: two words, three objects, seven agenda words, two steps of 44 bytes and two objects
	EXPECT_EQ(bytes.size(), 569u + 2 * 12 + 3 * 4 + 2 * 4 + 3 * 4 + 7 * 4 + 2 * 44 + 2 * 4);
	const auto read = ReadCreatureRecord(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, record);
	EXPECT_EQ(Write(*read), bytes);
}

TEST(SaveCreatureRecord, TheAgendaKeepsOnlyTheStepsFromTheCurrentOne)
{
	auto record = AFullRecord();
	record.plan->agenda.current = 3;
	record.plan->agenda.steps.clear();
	const auto read = ReadCreatureRecord(Write(record));
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->plan->agenda.steps.empty());
	EXPECT_EQ(read->plan->agenda.creature, 24u);
}

TEST(SaveCreatureRecord, AnOldRecordHasNoProcessingWord)
{
	auto record = AFullRecord();
	record.version = 25;
	auto bytes = Write(record);
	// The old layout leaves the word out
	const size_t at = 394 + 4 + record.queued.size() * 12;
	bytes.erase(bytes.begin() + static_cast<std::ptrdiff_t>(at), bytes.begin() + static_cast<std::ptrdiff_t>(at + 4));
	const auto read = ReadCreatureRecord(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->mindProcessing, 1u);
	EXPECT_EQ(read->largestSize, 2.0f);
	EXPECT_EQ(read->creedPowers[1].power, 2.0f);
}

TEST(SaveCreatureRecord, ACutRecordReadsAsNothing)
{
	const auto bytes = Write(AFullRecord());
	for (const size_t size : {size_t {0}, size_t {9}, size_t {400}, bytes.size() - 1})
	{
		EXPECT_FALSE(ReadCreatureRecord(std::span(bytes).first(size)).has_value()) << size;
	}
}

TEST(SaveCreatureRecord, TheLeashRecordReadsBack)
{
	const LeashRecord leash {.on = 1,
	                         .works = 0,
	                         .type = 3,
	                         .player = 1,
	                         .word20 = 5,
	                         .tiedTo = 40,
	                         .length = 10.0f,
	                         .fullLength = 25.0f,
	                         .holder = 41,
	                         .word34 = 6};
	ByteWriter writer;
	Write(writer, leash);
	EXPECT_EQ(writer.Bytes().size(), 37u);
	ByteReader reader(writer.Bytes());
	EXPECT_EQ(ReadLeashRecord(reader), leash);
}

TEST(SaveCreatureRecord, TheKnownLeashesAreOneByteEvilRopeGood)
{
	EXPECT_EQ(ToByte({.evil = true, .rope = false, .good = false}), 1u);
	EXPECT_EQ(ToByte({.evil = false, .rope = true, .good = true}), 6u);
	EXPECT_EQ(KnownLeashesFromByte(5), (KnownLeashes {.evil = true, .rope = false, .good = true}));
}

TEST(SaveGameInfoFile, TheFileHas260BytesAndReadsBack)
{
	const SaveGameInfo info {.slot = 4, .date = u"10:15:00 Friday, 16 October 2026", .name = u"Before the ogre"};
	const auto bytes = Write(info);
	ASSERT_EQ(bytes.size(), k_SaveGameInfoSize);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 4u);
	EXPECT_EQ(At<char16_t>(bytes, 4), u'1');
	EXPECT_EQ(At<char16_t>(bytes, 132), u'B');
	EXPECT_EQ(ReadSaveGameInfo(bytes), info);
}

TEST(SaveGameInfoFile, ALongDateIsLeftOutAndALongNameIsCut)
{
	const SaveGameInfo info {.slot = 0, .date = std::u16string(62, u'x'), .name = std::u16string(70, u'n')};
	const auto read = ReadSaveGameInfo(Write(info));
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->date.empty());
	EXPECT_EQ(read->name, std::u16string(63, u'n'));
	EXPECT_FALSE(ReadSaveGameInfo(std::vector<uint8_t>(259)).has_value());
}

TEST(SaveSlots, TheLandSlotsHaveAFolderForEachLand)
{
	EXPECT_EQ(SlotFolder(3, 1), std::filesystem::path("Saved Games") / "3");
	EXPECT_EQ(SlotFolder(14, 2), std::filesystem::path("Saved Games") / "14" / "2");
	EXPECT_EQ(SlotFolder(18, 2), std::filesystem::path("Saved Games") / "18" / "2");
	EXPECT_EQ(SlotFolder(20, 2), std::filesystem::path("Saved Games") / "20");
	// Only the autosaves keep the minds in the land's folder
	EXPECT_EQ(MindFolder(14, 2), std::filesystem::path("Saved Games") / "14");
	EXPECT_EQ(MindFolder(16, 2), std::filesystem::path("Saved Games") / "16" / "2");
	EXPECT_EQ(SavedCreatureFileName(0), "SavedCreature0");
	EXPECT_TRUE(IsPlayerSlot(13));
	EXPECT_FALSE(IsPlayerSlot(14));
	EXPECT_FALSE(IsSlot(21));
}

TEST(SaveWorldFile, BlocksReadBackAndABadFileReadsAsNothing)
{
	CreatureChunk creature {.self = 99, .position = {1.0f, 2.0f, 3.0f}, .heading = 1.5f, .life = 0.5f};
	creature.record = AFullRecord();
	creature.leash = LeashRecord {.on = 1, .tiedTo = 3};
	creature.agendaStep = 2;
	const GameChunk game {.land = 1, .turn = 1234, .landscape = "./Data/Landscape/Land1.lnd", .knownLeashes = 2};
	const WorldFile file {.version = k_WorldVersion, .chunks = {ToChunk(game), ToChunk(creature)}};
	const auto bytes = Write(file);
	const auto read = ReadWorldFile(bytes);
	ASSERT_TRUE(read.has_value());
	ASSERT_EQ(read->chunks.size(), 2u);
	EXPECT_EQ(ReadGameChunk(read->chunks[0].bytes), game);
	EXPECT_EQ(ReadCreatureChunk(read->chunks[1].bytes), creature);

	auto badMagic = bytes;
	badMagic[0] = 'X';
	EXPECT_FALSE(ReadWorldFile(badMagic).has_value());
	EXPECT_FALSE(ReadWorldFile(std::span(bytes).first(bytes.size() - 1)).has_value());
}
