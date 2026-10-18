/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

#include <CreatureRecord.h>
#include <SaveGameInfo.h>
#include <SaveSlots.h>
#include <SavedObjects.h>
#include <VanillaSaveFile.h>
#include <WorldFile.h>
#include <gtest/gtest.h>

#include "Creature/CreatureHandGlow.h"

using namespace openblack::gamesave;

namespace
{
/// The objects that the tests' streams have before the record, so that small reference numbers are earlier objects
constexpr StreamStart k_After100 {.objects = 100, .checksum = 0};

template <typename T>
T At(const std::vector<uint8_t>& bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

ObjectRef Earlier(uint32_t number)
{
	return RefTo(NumberKey(number));
}

template <typename Object>
ObjectRef NewObject(uint32_t number, const Object& object)
{
	return {.key = NumberKey(number), .record = std::make_shared<SavedObject>(SavedObject {.value = object})};
}

std::vector<uint8_t> Bytes(const CreatureRecord& record, StreamStart start = k_After100)
{
	StreamWriter writer(start);
	Write(writer, record);
	return writer.Take();
}

std::optional<CreatureRecord> Read(std::span<const uint8_t> bytes, StreamStart start = k_After100)
{
	StreamReader reader(bytes, start);
	return ReadCreatureRecord(reader);
}

/// A record with a value in every field, a plan with two steps left, spells queued and villagers that dance
CreatureRecord AFullRecord()
{
	CreatureRecord record;
	record.savedInTemple = 1;
	record.index = 2;
	record.owner = 0;
	record.spells[0] = {.spell = 0, .phase = 3, .turnsLeft = 40, .found = 1.5f, .holdTurns = 300, .miracle = Earlier(7)};
	record.spells[7] = {.spell = 7, .phase = 1, .turnsLeft = 20, .found = 0.0f, .holdTurns = -1, .miracle = Earlier(8)};
	record.queued = {{.spell = 2, .seconds = 30.5f, .miracle = Earlier(9)}, {.spell = 4, .seconds = -1.0f, .miracle = {}}};
	record.mindProcessing = 0;
	record.largestSize = 2.0f;
	record.smallestSize = 0.4f;
	record.mayGoThroughVortex = 1;
	record.fizzedForGood = 1;
	record.fizz = 0.25f;
	record.fizzTarget = 1.0f;
	record.fizzPerSecond = 0.5f;
	record.freeze = 0.75f;
	record.spellVisuals = {Earlier(10), ObjectRef {}, Earlier(11)};
	record.handParticles = {Earlier(12), Earlier(13)};
	record.targetPoint = {1.0f, 2.0f, 3.0f};
	record.targetParticles = Earlier(14);
	record.targetObject = Earlier(15);
	record.hasTarget = 1;
	record.arena = Earlier(16);
	record.home = {.x = 0x00DF71B0, .z = 0x013ACEB2, .altitude = 20.0f};
	record.fightHealth = 0.6f;
	record.autoFight = 2;
	record.dancePartners = {Earlier(17), Earlier(18), Earlier(19)};
	record.dance = Earlier(20);
	record.canDie = 0;
	record.strength = 0.8f;
	record.size = 1.7f;
	record.alignment = -0.3f;
	record.animationPace = 0.5f;
	record.autoScale = 1;
	record.autoScaleShare = 0.9f;
	record.plan = KeptPlan {
	    .desire = 24,
	    .action = 22,
	    .objects = {Earlier(21), Earlier(22), ObjectRef {}},
	    .agenda = {.word08 = 1,
	               .current = 1,
	               .state = 2,
	               .count = 3,
	               .word18 = 4,
	               .word1C = 5,
	               .word20 = 1,
	               .steps = {{.action = 10, .object = Earlier(21), .integer = 57, .point = {4.0f, 5.0f, 6.0f}, .real = 1.1f},
	                         {.action = 133, .object = Earlier(22), .spell = 9, .other = Earlier(23), .orderKind = 3}},
	               .creature = Earlier(24),
	               .other = {}},
	};
	record.creedPowers[1] = {.scale = 0.5f, .power = 2.0f};
	return record;
}

/// A creature's whole record with every part
CreatureObject AFullCreature()
{
	CreatureObject creature;
	creature.thing = {.extra = 0, .checksum = 0, .destroyed = 0, .flags = 0x40};
	creature.position = {.position = {.x = 0x00BFE92F, .z = 0x00D32E3A, .altitude = 0.0f}, .flags = 0x0103};
	creature.object = {.info = 2516,
	                   .coords = {.x = 0x00BFE92F, .z = 0x00D32E3A, .altitude = 0.0f},
	                   .creationIndex = 29,
	                   .life = 1.0f,
	                   .scale = 0.26f,
	                   .heading = 1.25f,
	                   .fire = {},
	                   .physics = std::nullopt};
	creature.move = {.mobileWord = 0,
	                 .turnsToStateChange = 2,
	                 .speed = 0x05C3,
	                 .angle = 100,
	                 .moveState = 0x0D,
	                 .circle = CircleHug {.turnsToObject = 3,
	                                      .entryDistance = 70000,
	                                      .object = Earlier(5),
	                                      .index = 1,
	                                      .coords = {.x = 1, .z = 2, .altitude = 3.0f}},
	                 .turnsToStepRebuild = std::nullopt,
	                 .footpath = {},
	                 .goal = {.x = 10, .z = 20, .altitude = 0.0f},
	                 .target = Earlier(6),
	                 .step = {.x = 4, .altitude = 0.5f, .z = -4}};
	creature.living.action = {1, 2, 3, 0, 9, 0};
	creature.living.birthTurn = 0xFFFF67A8;
	creature.living.status = 2;
	creature.living.specular = {10, 20, 30, 40};
	creature.living.reactionsDone = {{.reaction = 4, .turn = 100}, {.reaction = 7, .turn = 250}};
	creature.creature = AFullRecord();
	return creature;
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/// The players' saved games of the original game are their own files and are not part of the project. The tests
/// that read them run only on a copy of a Profiles folder that OPENBLACK_VANILLA_SAVES gives.
std::filesystem::path VanillaSavesFolder()
{
	const char* folder = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	return folder != nullptr ? std::filesystem::path(folder) : std::filesystem::path();
}

std::vector<std::filesystem::path> VanillaWorldFiles()
{
	std::vector<std::filesystem::path> files;
	const auto folder = VanillaSavesFolder();
	if (folder.empty() || !std::filesystem::is_directory(folder))
	{
		return files;
	}
	for (const auto& entry : std::filesystem::recursive_directory_iterator(folder))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".sav" &&
		    !entry.path().filename().string().starts_with("Script_"))
		{
			files.push_back(entry.path());
		}
	}
	return files;
}
} // namespace

TEST(SaveObjectStream, EachValueAddsItsFirstByteAndItsSizeToTheSum)
{
	StreamWriter writer;
	writer.Put(uint32_t {0x1234});
	writer.Put(uint8_t {7});
	writer.Put(MapCoords {.x = 0x0102, .z = 0, .altitude = 0.0f});
	const uint32_t sum = (0x34u + 4) + (7u + 1) + (0x02u + 12);
	EXPECT_EQ(writer.Checksum(), sum);
	writer.PutUncounted(99);
	EXPECT_EQ(writer.Checksum(), sum);
	StreamReader reader(writer.Bytes());
	uint32_t word = 0;
	uint8_t byte = 0;
	MapCoords coords;
	reader.Get(word);
	reader.Get(byte);
	reader.Get(coords);
	EXPECT_EQ(reader.Checksum(), sum);
}

TEST(SaveObjectStream, AReferenceIsNoneAnEarlierNumberOrTheNextNumber)
{
	StreamWriter writer({.objects = 3, .checksum = 0});
	EXPECT_FALSE(writer.PutReference(k_NoObject));
	EXPECT_FALSE(writer.PutReference(NumberKey(2)));
	EXPECT_TRUE(writer.PutReference(EntityKey(50)));
	EXPECT_FALSE(writer.PutReference(EntityKey(50)));
	EXPECT_TRUE(writer.PutReference(NumberKey(9)));
	const auto bytes = writer.Bytes();
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 4), 2u);
	EXPECT_EQ(At<uint32_t>(bytes, 8), 4u);
	EXPECT_EQ(At<uint32_t>(bytes, 12), 4u);
	EXPECT_EQ(At<uint32_t>(bytes, 16), 5u);

	StreamReader reader(bytes, {.objects = 3, .checksum = 0});
	ObjectKey key = 0;
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::None);
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::Earlier);
	EXPECT_EQ(key, NumberKey(2));
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::New);
	EXPECT_EQ(key, NumberKey(4));
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::Earlier);
	EXPECT_EQ(reader.GetReference(key), ReferenceKind::New);

	// A number past the next number is not a stream of the game
	const std::vector<uint8_t> bad {7, 0, 0, 0};
	StreamReader badReader(bad, {.objects = 3, .checksum = 0});
	EXPECT_EQ(badReader.GetReference(key), ReferenceKind::Bad);
	EXPECT_TRUE(badReader.Failed());
}

TEST(SaveCreatureRecord, ARecordWithNoListsAndNoPlanHasTheGamesLength)
{
	EXPECT_EQ(Bytes(CreatureRecord {}).size(), 569u);
}

TEST(SaveCreatureRecord, EachSpellSlotStartsWithTheSpellOfItsPlace)
{
	const auto bytes = Bytes(CreatureRecord {});
	for (uint32_t i = 0; i < k_CreatureSpellCount; ++i)
	{
		EXPECT_EQ(At<uint32_t>(bytes, 10 + i * 24), i);
	}
}

TEST(SaveCreatureRecord, TheFieldsAreAtTheGamesOffsets)
{
	auto record = AFullRecord();
	record.queued.clear();
	record.dancePartners.clear();
	record.plan.reset();
	const auto bytes = Bytes(record);
	ASSERT_EQ(bytes.size(), 569u);
	EXPECT_EQ(At<uint32_t>(bytes, 0), 33u);
	EXPECT_EQ(At<uint32_t>(bytes, 4), 1u);
	EXPECT_EQ(At<uint8_t>(bytes, 8), 2u);
	EXPECT_EQ(At<uint8_t>(bytes, 9), 0u);
	// The spell slots: six words each, freeze first
	EXPECT_EQ(At<uint32_t>(bytes, 10), 0u);
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
	EXPECT_EQ(At<int32_t>(bytes, 477), 0x00DF71B0);
	EXPECT_EQ(At<float>(bytes, 485), 20.0f);
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
	const auto bytes = Bytes(record);
	// The plan: two words, three objects, seven agenda words, two steps of 44 bytes and two objects
	EXPECT_EQ(bytes.size(), 569u + 2 * 12 + 3 * 4 + 2 * 4 + 3 * 4 + 7 * 4 + 2 * 44 + 2 * 4);
	const auto read = Read(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, record);
	EXPECT_EQ(Bytes(*read), bytes);
}

TEST(SaveCreatureRecord, AGlowingCreatureGetsItsCreedGlowsBackAndWritesTheSameBytes)
{
	namespace glow = openblack::creature_hand_glow;
	using glow::Place;
	glow::Glows glows;
	glow::Set(glows, Place::LeftHand, 1.0f, 1.5f);
	glow::Set(glows, Place::Circling, 5.0f, 0.1f);

	auto record = AFullRecord();
	for (size_t place = 0; place < record.creedPowers.size(); ++place)
	{
		const auto kept = glow::SavedPowerOf(glows, static_cast<Place>(place));
		record.creedPowers.at(place) = {.scale = kept.scale, .power = kept.power};
	}
	const auto bytes = Bytes(record);
	// Each place keeps its scale, then its power, in the order of the places. A place without a glow keeps 0 and 0.
	const size_t creed = bytes.size() - 4 * 8;
	EXPECT_EQ(At<float>(bytes, creed + 0), 1.0f);
	EXPECT_EQ(At<float>(bytes, creed + 4), 1.5f);
	EXPECT_EQ(At<float>(bytes, creed + 8), 0.0f);
	EXPECT_EQ(At<float>(bytes, creed + 12), 0.0f);
	EXPECT_EQ(At<float>(bytes, creed + 24), 5.0f);
	EXPECT_EQ(At<float>(bytes, creed + 28), 0.1f);

	const auto read = Read(bytes);
	ASSERT_TRUE(read.has_value());
	std::array<glow::SavedPower, glow::k_PlaceCount> kept {};
	std::ranges::transform(read->creedPowers, kept.begin(), [](const CreedPower& power) {
		return glow::SavedPower {.scale = power.scale, .power = power.power};
	});
	glow::Glows loaded;
	EXPECT_EQ(glow::LoadCreedPowers(loaded, kept), glow::Hum::Start);
	ASSERT_TRUE(loaded[0].has_value());
	EXPECT_EQ(loaded[0]->scale, 1.0f);
	EXPECT_EQ(loaded[0]->power, 1.5f);
	EXPECT_FALSE(loaded[1].has_value());
	EXPECT_FALSE(loaded[2].has_value());
	ASSERT_TRUE(loaded[3].has_value());
	EXPECT_EQ(loaded[3]->power, 0.1f);

	// A save of the loaded creature writes the same bytes again
	auto again = *read;
	for (size_t place = 0; place < again.creedPowers.size(); ++place)
	{
		const auto power = glow::SavedPowerOf(loaded, static_cast<Place>(place));
		again.creedPowers.at(place) = {.scale = power.scale, .power = power.power};
	}
	EXPECT_EQ(Bytes(again), bytes);
}

TEST(SaveCreatureRecord, TheAgendaKeepsOnlyTheStepsFromTheCurrentOne)
{
	auto record = AFullRecord();
	record.plan->agenda.current = 3;
	record.plan->agenda.steps.clear();
	const auto read = Read(Bytes(record));
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->plan->agenda.steps.empty());
	EXPECT_EQ(read->plan->agenda.creature, Earlier(24));
}

TEST(SaveCreatureRecord, AnOldRecordHasNoProcessingWord)
{
	auto record = AFullRecord();
	record.version = 25;
	auto bytes = Bytes(record);
	// The old layout leaves the word out
	const size_t at = 394 + 4 + record.queued.size() * 12;
	bytes.erase(bytes.begin() + static_cast<std::ptrdiff_t>(at), bytes.begin() + static_cast<std::ptrdiff_t>(at + 4));
	const auto read = Read(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->mindProcessing, 1u);
	EXPECT_EQ(read->largestSize, 2.0f);
	EXPECT_EQ(read->creedPowers[1].power, 2.0f);
}

TEST(SaveCreatureRecord, ACutRecordReadsAsNothing)
{
	const auto bytes = Bytes(AFullRecord());
	for (const size_t size : {size_t {0}, size_t {9}, size_t {400}, bytes.size() - 1})
	{
		EXPECT_FALSE(Read(std::span(bytes).first(size)).has_value()) << size;
	}
}

TEST(SaveCreatureRecord, TheLeashRecordReadsBack)
{
	const LeashRecord leash {.on = 1,
	                         .works = 0,
	                         .type = 3,
	                         .player = 1,
	                         .word20 = 5,
	                         .tiedTo = Earlier(40),
	                         .length = 10.0f,
	                         .fullLength = 25.0f,
	                         .holder = Earlier(41),
	                         .word34 = 6};
	StreamWriter writer(k_After100);
	Write(writer, leash);
	EXPECT_EQ(writer.Bytes().size(), 37u);
	StreamReader reader(writer.Bytes(), k_After100);
	EXPECT_EQ(ReadLeashRecord(reader), leash);
}

TEST(SaveCreatureRecord, TheKnownLeashesAreOneByteEvilRopeGood)
{
	EXPECT_EQ(ToByte({.evil = true, .rope = false, .good = false}), 1u);
	EXPECT_EQ(ToByte({.evil = false, .rope = true, .good = true}), 6u);
	EXPECT_EQ(KnownLeashesFromByte(5), (KnownLeashes {.evil = true, .rope = false, .good = true}));
}

TEST(SaveCreatureObject, TheWholeRecordStartsWithItsTypeAndItsRunningSum)
{
	StreamWriter writer({.objects = 247, .checksum = 0x27000});
	Write(writer, NewObject(248, AFullCreature()));
	const auto& bytes = writer.Bytes();
	EXPECT_EQ(At<uint32_t>(bytes, 0), 248u);
	EXPECT_EQ(At<uint32_t>(bytes, 4), k_CreatureType);
	EXPECT_EQ(At<uint32_t>(bytes, 8), 0u);
	// The sum after the reference, the type and the extra word
	EXPECT_EQ(At<uint32_t>(bytes, 12), 0x27000u + (248u + 4) + (k_CreatureType + 4) + (0 + 4));
	EXPECT_EQ(StartOfRecordAt(bytes, 0), (StreamStart {.objects = 247, .checksum = 0x27000}));
}

TEST(SaveCreatureObject, AWholeRecordWithANewObjectInsideReadsBackAsItWasWritten)
{
	auto creature = AFullCreature();
	// The creature aims at another creature that the stream did not write before, so its record comes inline
	auto other = AFullCreature();
	other.creature.plan.reset();
	other.creature.index = 3;
	other.creature.targetObject = Earlier(101);
	creature.creature.targetObject = NewObject(102, other);
	const auto first = NewObject(101, creature);
	StreamWriter writer(k_After100);
	Write(writer, first);
	EXPECT_EQ(writer.Objects(), 102u);
	const auto bytes = writer.Take();

	StreamReader reader(bytes, k_After100);
	ObjectRef read;
	ASSERT_TRUE(Read(reader, read));
	EXPECT_EQ(reader.Position(), bytes.size());
	ASSERT_NE(read.record, nullptr);
	EXPECT_EQ(read, first);
	StreamWriter again(k_After100);
	Write(again, read);
	EXPECT_EQ(again.Bytes(), bytes);
}

TEST(SaveCreatureObject, AnObjectOfAClassThatOpenblackDoesNotKnowStopsTheRead)
{
	StreamWriter writer(k_After100);
	EXPECT_TRUE(writer.PutReference(NumberKey(101)));
	writer.Put(uint32_t {0x27});
	writer.Put(uint32_t {0});
	StreamReader reader(writer.Bytes(), k_After100);
	ObjectRef ref;
	EXPECT_FALSE(Read(reader, ref));
	EXPECT_EQ(reader.UnknownType(), 0x27u);
}

TEST(SaveCreatureObject, ABodyInThePhysicsWritesItsMatrixAndItsSpeeds)
{
	auto creature = AFullCreature();
	creature.position.flags |= k_PositionFlagInPhysics;
	creature.object.physics = PhysicsPart {.velocity = {1.0f, 2.0f, 3.0f}, .spin = {0.0f, 0.5f, 0.0f}};
	StreamWriter with(k_After100);
	Write(with, creature);
	StreamWriter without(k_After100);
	Write(without, AFullCreature());
	EXPECT_EQ(with.Bytes().size(), without.Bytes().size() + 48 + 12 + 12);
	StreamReader reader(with.Bytes(), k_After100);
	const auto read = ReadObject(reader);
	ASSERT_NE(read, nullptr);
	EXPECT_EQ(std::get<CreatureObject>(read->value), creature);
}

TEST(SaveVanillaFile, TheStartHasTheRevisionAndTheLand)
{
	const auto bytes = WriteVanillaFraming(".\\DATA\\LANDSCAPE\\Land1.lnd");
	EXPECT_EQ(At<uint32_t>(bytes, 0), 0u);
	EXPECT_EQ(At<uint32_t>(bytes, 4), 17u);
	const auto framing = ReadVanillaFraming(bytes);
	ASSERT_TRUE(framing.has_value());
	EXPECT_EQ(framing->revision, k_VanillaRevision);
	EXPECT_EQ(framing->landscape, ".\\DATA\\LANDSCAPE\\Land1.lnd");
	EXPECT_EQ(framing->body, bytes.size());
}

TEST(SaveVanillaFile, ACraftedFileGivesItsCreaturesAndLeashesAndWritesThemBackTheSame)
{
	// A crafted file: the start, some values of other objects, a creature, a value, a leash
	StreamWriter writer;
	writer.PutUncounted(0);
	for (uint32_t i = 0; i < 40; ++i)
	{
		writer.Put(i * 977);
	}
	// The file gave 30 objects numbers before the creature
	const StreamStart atCreature {.objects = 30, .checksum = writer.Checksum()};
	StreamWriter creatureWriter(atCreature);
	const auto creature = AFullCreature();
	Write(creatureWriter, NewObject(31, creature));
	auto file = writer.Take();
	const auto creatureAt = file.size();
	file.insert(file.end(), creatureWriter.Bytes().begin(), creatureWriter.Bytes().end());
	const auto afterCreature = file.size();
	StreamWriter leashWriter({.objects = 31, .checksum = creatureWriter.Checksum()});
	leashWriter.Put(1.5f);
	const LeashStatusObject leash {
	    .thing = {}, .leash = {.on = 1, .works = 1, .type = 2, .length = 24.3f, .fullLength = 41.9f, .holder = Earlier(30)}};
	Write(leashWriter, NewObject(32, leash));
	file.insert(file.end(), leashWriter.Bytes().begin(), leashWriter.Bytes().end());

	const auto creatures = FindCreatures(file);
	ASSERT_EQ(creatures.size(), 1u);
	EXPECT_EQ(creatures[0].at, creatureAt);
	EXPECT_EQ(creatures[0].end, afterCreature);
	EXPECT_EQ(creatures[0].number, 31u);
	EXPECT_EQ(creatures[0].start, atCreature);
	EXPECT_EQ(creatures[0].object, creature);
	const auto leashes = FindLeashes(file);
	ASSERT_EQ(leashes.size(), 1u);
	EXPECT_EQ(leashes[0].object, leash);

	// The record that openblack writes from what it read is the same as the file's
	EXPECT_EQ(Replace(file, creatures[0], NewObject(31, creatures[0].object)), file);

	// A change of a value goes into the file at the creature's place, and the rest of the file stays the same
	auto stronger = creatures[0].object;
	stronger.creature.strength = 0.9f;
	const auto changed = Replace(file, creatures[0], NewObject(31, stronger));
	ASSERT_TRUE(changed.has_value());
	ASSERT_EQ(changed->size(), file.size());
	const auto found = FindCreatures(*changed);
	ASSERT_EQ(found.size(), 1u);
	EXPECT_EQ(found[0].object.creature.strength, 0.9f);
	EXPECT_TRUE(std::equal(changed->begin() + static_cast<std::ptrdiff_t>(afterCreature), changed->end(),
	                       file.begin() + static_cast<std::ptrdiff_t>(afterCreature)));

	// A record that writes another number of new objects cannot go in: the objects after it would get other numbers
	auto aimed = creatures[0].object;
	aimed.creature.targetObject = NewObject(32, AFullCreature());
	EXPECT_FALSE(Replace(file, creatures[0], NewObject(31, aimed)).has_value());
	// A record with an object that has no record in the game's layout cannot go in
	auto foreign = creatures[0].object;
	foreign.creature.targetObject = RefTo(EntityKey(5));
	EXPECT_FALSE(Replace(file, creatures[0], NewObject(31, foreign)).has_value());
}

TEST(SaveVanillaFile, TheRecordsOfRealSavedGamesWriteBackByteForByte)
{
	const auto files = VanillaWorldFiles();
	if (files.empty())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	size_t creatures = 0;
	for (const auto& path : files)
	{
		const auto bytes = ReadAll(path);
		const auto framing = ReadVanillaFraming(bytes);
		ASSERT_TRUE(framing.has_value()) << path;
		EXPECT_EQ(framing->revision, k_VanillaRevision);
		for (const auto& creature : FindCreatures(bytes))
		{
			const auto ref = NewObject(creature.number, creature.object);
			const auto written = WriteRecordAt(creature.start, ref);
			ASSERT_TRUE(written.has_value()) << path;
			ASSERT_EQ(written->size(), creature.end - creature.at) << path;
			EXPECT_TRUE(std::equal(written->begin(), written->end(), bytes.begin() + static_cast<std::ptrdiff_t>(creature.at)))
			    << path << " creature " << creature.number;
			EXPECT_EQ(Replace(bytes, creature, ref), bytes);
			++creatures;
		}
		for (const auto& leash : FindLeashes(bytes))
		{
			EXPECT_EQ(Replace(bytes, leash, NewObject(leash.number, leash.object)), bytes) << path << " leash " << leash.number;
		}
	}
	EXPECT_GT(creatures, 0u) << "No saved game of the folder has a creature";
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

TEST(SaveWorldFile, TheCreaturesReadBackWithTheirLeashesAndTheirReferences)
{
	SavedCreature first {.key = EntityKey(40), .creature = AFullCreature(), .leash = std::nullopt, .agendaStep = 2};
	SavedCreature second {.key = EntityKey(41), .creature = AFullCreature(), .leash = std::nullopt, .agendaStep = 0};
	second.creature.creature.index = 1;
	second.creature.creature.plan.reset();
	// The first creature aims at the second one, so the stream writes the second creature in full inside the first.
	// It also acts on a rock that openblack has no record for.
	first.creature.creature.targetObject = RefTo(EntityKey(41));
	first.creature.creature.plan->objects[0] = RefTo(EntityKey(77));
	first.creature.creature.plan->agenda.creature = RefTo(EntityKey(40));
	first.leash = LeashStatusObject {.thing = {}, .leash = {.on = 1, .type = 3, .tiedTo = RefTo(EntityKey(77))}};
	// The game's numbers of a creature that came from a file of the game stay as they are
	second.creature.creature.arena = RefTo(NumberKey(900));
	const std::vector creatures {first, second};
	const GameChunk game {.land = 1, .turn = 1234, .landscape = "./Data/Landscape/Land1.lnd", .knownLeashes = 2};
	WorldFile file {.version = k_WorldVersion, .chunks = {ToChunk(game)}};
	const auto chunks = ToChunks(creatures);
	file.chunks.insert(file.chunks.end(), chunks.begin(), chunks.end());
	const auto bytes = Write(file);
	const auto read = ReadWorldFile(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(ReadGameChunk(read->chunks[0].bytes), game);
	const auto readCreatures = ReadCreatures(*read);
	ASSERT_TRUE(readCreatures.has_value());
	ASSERT_EQ(readCreatures->size(), 2u);
	const auto& firstRead = (*readCreatures)[0];
	EXPECT_EQ(firstRead.key, EntityKey(40));
	EXPECT_EQ(firstRead.agendaStep, 2u);
	ASSERT_TRUE(firstRead.leash.has_value());
	EXPECT_EQ(firstRead.leash->leash.tiedTo.key, EntityKey(77));
	EXPECT_EQ(firstRead.creature.creature.targetObject.key, EntityKey(41));
	EXPECT_EQ(firstRead.creature.creature.plan->objects[0].key, EntityKey(77));
	EXPECT_EQ(firstRead.creature.creature.plan->agenda.creature.key, EntityKey(40));
	const auto& secondRead = (*readCreatures)[1];
	EXPECT_EQ(secondRead.key, EntityKey(41));
	EXPECT_EQ(secondRead.creature.creature.index, 1u);
	EXPECT_EQ(secondRead.creature.creature.arena.key, NumberKey(900));
	EXPECT_FALSE(secondRead.leash.has_value());

	auto badMagic = bytes;
	badMagic[0] = 'X';
	EXPECT_FALSE(ReadWorldFile(badMagic).has_value());
	EXPECT_FALSE(ReadWorldFile(std::span(bytes).first(bytes.size() - 1)).has_value());
}
