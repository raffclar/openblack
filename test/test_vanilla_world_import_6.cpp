/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaWorldImportFixture.h"

using namespace openblack;
using namespace openblack::gamesave;
using namespace vanilla_import_test;

TEST(VanillaWorldImport, SavedGamesHaveACameraWhosePointsGlideTogether)
{
	const auto files = ForEachSave([](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto camera = camera_import::ReadCamera(world);
		ASSERT_TRUE(camera.has_value()) << path;
		EXPECT_EQ(camera->mode, camera_import::Mode::World) << path;
		const auto motion = camera_import::MotionOf(camera->eye, camera->focus);
		ASSERT_TRUE(motion.has_value()) << path;
		// The motion is where the file has the camera now
		const auto share = [](std::chrono::microseconds time, std::chrono::microseconds duration) {
			return duration.count() == 0 ? 1.0f : static_cast<float>(time.count()) / static_cast<float>(duration.count());
		};
		const auto eye = motion->origin.PositionAt(share(motion->time, motion->duration));
		const auto focus = motion->focus.PositionAt(share(motion->focusTime, motion->focusDuration));
		for (glm::length_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(eye[axis], camera->eye.at(static_cast<size_t>(axis)).value, 0.01f) << path;
			EXPECT_NEAR(focus[axis], camera->focus.at(static_cast<size_t>(axis)).value, 0.01f) << path;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(*files, 0u);
}

namespace
{
/// The record of a new spell, as the game writes it, after the reference to it. The kind of the spell writes its own
/// values at the end. The values are not the values of a real file.
std::vector<uint8_t> SpellRecord(StreamStart start, uint32_t number, uint32_t saveType, MagicType magicType,
                                 const std::function<void(StreamWriter&)>& kind)
{
	StreamWriter writer(start);
	EXPECT_TRUE(writer.PutReference(NumberKey(number)));
	writer.Put(saveType);
	writer.Put(uint32_t {0});
	writer.PutChecksum();
	writer.Put(uint32_t {0});
	writer.Put(uint8_t {0});
	writer.Put(MapCoords {.x = 0x10000, .z = 0x20000, .altitude = 1.5f});
	writer.Put(uint16_t {0});
	// Two marks, each before a reference to no object
	for (const auto mark : {0x12C859u, 0x2A1C9Au})
	{
		writer.Put(mark);
		writer.Put(uint32_t {0});
	}
	// The movement of the spell, the map position where it was cast and the movement of the hand
	writer.Put(std::array {0.0f, 0.0f, -3.0f});
	writer.Put(std::array<uint8_t, 12> {});
	writer.Put(std::array<uint8_t, 12> {});
	// The prayer power, the prayer power when last filled, the flag of a closed spell, two more words and the flag that a
	// person casts the spell
	writer.Put(30.0f);
	writer.Put(50.0f);
	writer.Put(uint32_t {0});
	writer.Put(uint32_t {7});
	writer.Put(uint32_t {7});
	writer.Put(uint32_t {1});
	// What the spell gives its particles: two points, the view, the movement of the hand, the power, a word and a flag
	std::array<float, 15> info {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.75f, 0.0f, 0.0f};
	std::array<uint8_t, 60> infoBytes {};
	std::memcpy(infoBytes.data(), info.data(), infoBytes.size());
	infoBytes[0x38] = 1;
	writer.Put(infoBytes);
	// The time limit, two bytes, the creator (object 7) and three references to no object
	writer.Put(12.5f);
	writer.Put(uint8_t {0});
	writer.Put(uint8_t {0});
	writer.Put(uint32_t {7});
	for (int i = 0; i < 3; ++i)
	{
		writer.Put(uint32_t {0});
	}
	// The magic type, the age, the size, the strength multiplier, the first cast place and the prayer power since the
	// last motes of light
	writer.Put(static_cast<uint32_t>(magicType));
	writer.Put(3.25f);
	writer.Put(40.0f);
	writer.Put(2.0f);
	writer.Put(std::array<uint8_t, 12> {});
	writer.Put(0.5f);
	writer.Put(0.25f);
	kind(writer);
	return writer.Take();
}

/// The record of a new water spell
std::vector<uint8_t> WaterSpellRecord(StreamStart start, uint32_t number)
{
	constexpr uint32_t k_SpellWaterType = 0x0C;
	return SpellRecord(start, number, k_SpellWaterType, MagicType::Water, [](StreamWriter& writer) {
		// The age when the last drop made a ring on the land
		writer.Put(2.0f);
	});
}
} // namespace

// The spell that a creature holds keeps its magic type, its time, its size, its prayer power and the values that it
// gives its particles
TEST(VanillaWorldImport, AHeldSpellReadsItsMagicTypeItsTimeAndItsPrayerPower)
{
	constexpr StreamStart k_Start {.objects = 100, .checksum = 0};
	std::string error;
	const auto world = ReadVanillaRecords(WaterSpellRecord(k_Start, 101), k_Start, &error);
	ASSERT_TRUE(world.has_value()) << error;
	const auto spell = world_import::ReadHeldSpell(*world, world->Records().front());
	ASSERT_TRUE(spell.has_value());
	EXPECT_EQ(spell->number, 101u);
	EXPECT_EQ(spell->magicType, MagicType::Water);
	EXPECT_EQ(spell->position, (MapCoords {.x = 0x10000, .z = 0x20000, .altitude = 1.5f}));
	EXPECT_EQ(spell->creator, 7u);
	EXPECT_EQ(spell->chants, 30.0f);
	EXPECT_EQ(spell->initialChants, 50.0f);
	EXPECT_FALSE(spell->closedDown);
	EXPECT_EQ(spell->duration, 12.5f);
	EXPECT_EQ(spell->age, 3.25f);
	EXPECT_EQ(spell->magnitude, 40.0f);
	EXPECT_EQ(spell->strengthMultiplier, 2.0f);
	EXPECT_EQ(spell->upkeepMotes, 0.5f);
	EXPECT_EQ(spell->eventMotes, 0.25f);
	EXPECT_EQ(spell->movement, (std::array {0.0f, 0.0f, -3.0f}));
	EXPECT_EQ(spell->casterPoint, (std::array {1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(spell->abovePoint, (std::array {4.0f, 5.0f, 6.0f}));
	EXPECT_EQ(spell->cameraForward, (std::array {0.0f, 0.0f, 1.0f}));
	EXPECT_EQ(spell->handMovement, (std::array {0.5f, 0.0f, 0.0f}));
	EXPECT_EQ(spell->power, 0.75f);
	EXPECT_TRUE(spell->casting);
	EXPECT_TRUE(spell->humanCasting);
	EXPECT_FALSE(spell->firstDone.has_value());
	ASSERT_TRUE(spell->lastRipple.has_value());
	EXPECT_EQ(*spell->lastRipple, 2.0f);
}

// A record that is not a spell, or a spell with no magic type, gives no held spell. Food and wood keep the flag of the
// first grain.
TEST(VanillaWorldImport, OnlyASpellWithAMagicTypeIsAHeldSpell)
{
	FakeWorld fake;
	fake.Record(VanillaType::Tree).Position(1, 2);
	fake.Record(static_cast<VanillaType>(0x0A)).Value<uint32_t>("spellMagicType", 0);
	fake.Record(static_cast<VanillaType>(0x0A))
	    .Position(3, 4)
	    .Value<uint32_t>("spellMagicType", static_cast<uint32_t>(MagicType::Food))
	    .Value<float>("spellDuration", -1.0f)
	    .Value<uint8_t>("spellFirstDone", 1);
	const auto world = fake.Make();
	const auto records = world.Records();
	EXPECT_FALSE(world_import::ReadHeldSpell(world, records[0]).has_value());
	EXPECT_FALSE(world_import::ReadHeldSpell(world, records[1]).has_value());
	const auto food = world_import::ReadHeldSpell(world, records[2]);
	ASSERT_TRUE(food.has_value());
	EXPECT_EQ(food->magicType, MagicType::Food);
	EXPECT_EQ(food->position, (MapCoords {.x = 3, .z = 4, .altitude = 0.0f}));
	EXPECT_EQ(food->duration, -1.0f);
	EXPECT_EQ(food->firstDone, std::optional(true));
	EXPECT_FALSE(food->lastRipple.has_value());
}

// A creature spell keeps the creature that it is on. Each spell keeps its particle system and the reaction to it.
TEST(VanillaWorldImport, ACreatureSpellReadsItsCreature)
{
	constexpr StreamStart k_Start {.objects = 100, .checksum = 0};
	constexpr uint32_t k_SpellCreatureType = 0x13;
	const auto bytes = SpellRecord(k_Start, 101, k_SpellCreatureType, MagicType::CreatureSpellStrong, [](StreamWriter& writer) {
		// The creature (object 40) and the flag that the hand poured the spell on it
		writer.Put(uint32_t {40});
		writer.Put(uint8_t {1});
	});
	std::string error;
	const auto world = ReadVanillaRecords(bytes, k_Start, &error);
	ASSERT_TRUE(world.has_value()) << error;
	const auto spell = world_import::ReadHeldSpell(*world, world->Records().front());
	ASSERT_TRUE(spell.has_value());
	EXPECT_EQ(spell->saveType, k_SpellCreatureType);
	EXPECT_EQ(spell->magicType, MagicType::CreatureSpellStrong);
	EXPECT_EQ(spell->target, std::optional<uint32_t>(40));
	EXPECT_FALSE(spell->flock.has_value());
	EXPECT_FALSE(spell->lastRipple.has_value());
	EXPECT_EQ(spell->system, 0u);
	EXPECT_EQ(spell->reaction, 0u);
}

// A flying flock keeps its flock, the animals that it made, and the last point of the sweep of the hand
TEST(VanillaWorldImport, AFlockSpellReadsItsFlockAndItsSweep)
{
	constexpr StreamStart k_Start {.objects = 100, .checksum = 0};
	constexpr uint32_t k_SpellFlockFlyingType = 0x0D;
	const auto bytes = SpellRecord(k_Start, 101, k_SpellFlockFlyingType, MagicType::FlockFlying, [](StreamWriter& writer) {
		// Two animals (objects 50 and 51), the flock (object 60), 2 made, 2.5 to make by now
		writer.Put(uint32_t {2});
		writer.Put(uint32_t {50});
		writer.Put(uint32_t {51});
		writer.Put(uint32_t {60});
		writer.Put(uint32_t {2});
		writer.Put(2.5f);
		// The point of the hand in map units and its height above the land
		writer.Put(int32_t {0x30000});
		writer.Put(int32_t {-0x10000});
		writer.Put(4.0f);
		// The values that the game does not read
		writer.Put(uint32_t {0});
		writer.Put(uint32_t {0});
		writer.Put(uint32_t {0});
		writer.Put(uint8_t {0});
	});
	std::string error;
	const auto world = ReadVanillaRecords(bytes, k_Start, &error);
	ASSERT_TRUE(world.has_value()) << error;
	const auto spell = world_import::ReadHeldSpell(*world, world->Records().front());
	ASSERT_TRUE(spell.has_value());
	EXPECT_EQ(spell->magicType, MagicType::FlockFlying);
	ASSERT_TRUE(spell->flock.has_value());
	EXPECT_EQ(spell->flock->flock, 60u);
	EXPECT_EQ(spell->flock->created, 2);
	EXPECT_EQ(spell->flock->emitted, 2.5f);
	EXPECT_EQ(spell->flock->lastSpawnX, 0x30000);
	EXPECT_EQ(spell->flock->lastSpawnZ, -0x10000);
	EXPECT_EQ(spell->flock->lastSpawnHeight, 4.0f);
	EXPECT_FALSE(spell->target.has_value());
}

// A spell keeps the numbers of its particle system and of the reaction to it
TEST(VanillaWorldImport, AHeldSpellReadsItsParticlesAndItsReaction)
{
	FakeWorld fake;
	fake.Record(static_cast<VanillaType>(0x12))
	    .Position(3, 4)
	    .Ref("spellSystem", 7)
	    .Ref("spellReaction", 8)
	    .Value<uint32_t>("spellMagicType", static_cast<uint32_t>(MagicType::Heal));
	const auto world = fake.Make();
	const auto heal = world_import::ReadHeldSpell(world, world.Records().front());
	ASSERT_TRUE(heal.has_value());
	EXPECT_EQ(heal->saveType, 0x12u);
	EXPECT_EQ(heal->system, 7u);
	EXPECT_EQ(heal->reaction, 8u);
}

// A forest spell keeps the flag that it planted its trees, the forest of its trees and the most trees that it can make
TEST(VanillaWorldImport, AForestSpellReadsItsForestAndItsMostTrees)
{
	FakeWorld fake;
	fake.Record(VanillaType::SpellForest)
	    .Position(3, 4)
	    .Value<uint32_t>("spellMagicType", static_cast<uint32_t>(MagicType::Forest))
	    .Value<uint8_t>("forestSpellPlanted", 1)
	    .Ref("forestSpellForest", 12)
	    .Value<uint32_t>("forestSpellMostTrees", 7);
	fake.Record(VanillaType::SpellForest)
	    .Position(3, 4)
	    .Value<uint32_t>("spellMagicType", static_cast<uint32_t>(MagicType::Forest))
	    .Value<uint8_t>("forestSpellPlanted", 0)
	    .Ref("forestSpellForest", 0)
	    .Value<uint32_t>("forestSpellMostTrees", 0xFFFFFFFFU);
	const auto world = fake.Make();
	const auto planted = world_import::ReadHeldSpell(world, world.Records()[0]);
	ASSERT_TRUE(planted.has_value());
	ASSERT_TRUE(planted->forest.has_value());
	EXPECT_TRUE(planted->forest->planted);
	EXPECT_EQ(planted->forest->forest, 12U);
	EXPECT_EQ(planted->forest->mostTrees, 7);
	EXPECT_FALSE(planted->shield.has_value());
	const auto seed = world_import::ReadHeldSpell(world, world.Records()[1]);
	ASSERT_TRUE(seed.has_value() && seed->forest.has_value());
	EXPECT_FALSE(seed->forest->planted);
	EXPECT_EQ(seed->forest->forest, 0U);
	EXPECT_EQ(seed->forest->mostTrees, -1);
}

// A shield spell keeps its object, the rings that take away the influence of the other players, and its two reactions
TEST(VanillaWorldImport, AShieldSpellReadsItsObjectItsRingsAndItsReactions)
{
	FakeWorld fake;
	fake.Record(VanillaType::SpellShield)
	    .Position(3, 4)
	    .Value<uint32_t>("spellMagicType", static_cast<uint32_t>(MagicType::PhysicalShield))
	    .Refs("spellObjects", {20})
	    .Refs("shieldSpellRings", {21, 22})
	    .Ref("shieldSpellStruckReaction", 0)
	    .Ref("shieldSpellReaction", 23);
	const auto world = fake.Make();
	const auto spell = world_import::ReadHeldSpell(world, world.Records().front());
	ASSERT_TRUE(spell.has_value());
	EXPECT_EQ(spell->objects, std::vector<uint32_t> {20});
	ASSERT_TRUE(spell->shield.has_value());
	EXPECT_EQ(spell->shield->rings, (std::vector<uint32_t> {21, 22}));
	EXPECT_EQ(spell->shield->struckReaction, 0U);
	EXPECT_EQ(spell->shield->reaction, 23U);
	EXPECT_FALSE(spell->forest.has_value());
}

// The dome of a physical shield keeps its turn, its sizes, its spins, its angles, its death and its two poses. The mark
// of a spiritual shield keeps only its place and its spell.
TEST(VanillaWorldImport, AShieldObjectReadsItsDome)
{
	std::array<float, 13> pose {};
	std::array<float, 13> lastPose {};
	pose[0] = 0.5f;
	pose[10] = 20.0f;
	pose[12] = 0.5f;
	lastPose[12] = 0.49f;
	FakeWorld fake;
	fake.Record(VanillaType::PhysicalShield)
	    .Value("position", MapCoords {.x = 0x10000, .z = 0x20000, .altitude = -1.5f})
	    .Value("scale", 0.45f)
	    .Ref("mapShieldSpell", 9)
	    .Value<uint32_t>("domeRaisedTurn", 100)
	    .Value("domePose", pose)
	    .Value("domeLastPose", lastPose)
	    .Value("domeStartScale", 0.005f)
	    .Value("domeFinalScale", 0.5f)
	    .Value("domeStartSpin", 2.0f)
	    .Value("domeEndSpin", 0.15f)
	    .Value("domeAngle", 1.5f)
	    .Value("domeBob", 0.75f)
	    .Value<uint8_t>("domeDying", 1)
	    .Value<uint8_t>("domeAlpha", 200)
	    .Value("domeDieTime", 0.25f)
	    .Ref("domeEffect", 30);
	fake.Record(VanillaType::MagicShield).Position(5, 6).Ref("mapShieldSpell", 10);
	fake.Record(VanillaType::Tree).Position(5, 6);
	const auto world = fake.Make();
	const auto physical = world_import::ReadShieldObject(world, world.Records()[0]);
	ASSERT_TRUE(physical.has_value());
	EXPECT_EQ(physical->spell, 9U);
	EXPECT_EQ(physical->position.x, 0x10000);
	EXPECT_FLOAT_EQ(physical->position.altitude, -1.5f);
	EXPECT_FLOAT_EQ(physical->scale, 0.45f);
	ASSERT_TRUE(physical->dome.has_value());
	const auto& dome = *physical->dome;
	EXPECT_EQ(dome.raisedTurn, 100U);
	EXPECT_FLOAT_EQ(dome.startScale, 0.005f);
	EXPECT_FLOAT_EQ(dome.finalScale, 0.5f);
	EXPECT_FLOAT_EQ(dome.startSpin, 2.0f);
	EXPECT_FLOAT_EQ(dome.endSpin, 0.15f);
	EXPECT_FLOAT_EQ(dome.angle, 1.5f);
	EXPECT_FLOAT_EQ(dome.bob, 0.75f);
	EXPECT_TRUE(dome.dying);
	EXPECT_EQ(dome.alpha, 200);
	EXPECT_FLOAT_EQ(dome.dieTime, 0.25f);
	EXPECT_EQ(dome.pose, pose);
	EXPECT_EQ(dome.lastPose, lastPose);
	EXPECT_EQ(dome.effect, 30U);
	const auto spiritual = world_import::ReadShieldObject(world, world.Records()[1]);
	ASSERT_TRUE(spiritual.has_value());
	EXPECT_EQ(spiritual->spell, 10U);
	EXPECT_FALSE(spiritual->dome.has_value());
	EXPECT_FALSE(world_import::ReadShieldObject(world, world.Records()[2]).has_value());
}

// A teleport stone keeps its place, its spell and its player
TEST(VanillaWorldImport, ATeleportStoneReadsItsPlaceAndItsSpell)
{
	FakeWorld fake;
	fake.Record(VanillaType::MagicTeleport)
	    .Position(0x30000, 0x40000)
	    .Ref("teleportReaction", 5)
	    .Ref("teleportSpell", 6)
	    .Ref("teleportPlayer", 7);
	fake.Record(VanillaType::MagicTeleport);
	const auto world = fake.Make();
	const auto stone = world_import::ReadTeleportStone(world, world.Records()[0]);
	ASSERT_TRUE(stone.has_value());
	EXPECT_EQ(stone->number, 1U);
	EXPECT_EQ(stone->position.x, 0x30000);
	EXPECT_EQ(stone->position.z, 0x40000);
	EXPECT_EQ(stone->spell, 6U);
	EXPECT_EQ(stone->player, 7U);
	// A stone with no place is not a stone
	EXPECT_FALSE(world_import::ReadTeleportStone(world, world.Records()[1]).has_value());
}

// A tree of a forest spell reads as a tree, with the value of its wood
TEST(VanillaWorldImport, ATreeOfAForestSpellReadsAsATreeWithItsWood)
{
	FakeWorld fake;
	fake.Record(VanillaType::Game);
	fake.Record(VanillaType::MagicTree)
	    .Position(10, 20)
	    .Value("info", world_import::k_FirstTreeRowNumber - static_cast<uint32_t>(TreeInfo::Oak))
	    .Value("life", 1.0f)
	    .Value("scale", 0.4f)
	    .Value("heading", 0.5f)
	    .Value<uint16_t>("growthCountdown", 0)
	    .Value("maxSize", 1.1f)
	    .Value<uint32_t>("magicTreePlayer", 0)
	    .Value("magicTreeWoodMultiplier", 1.75f);
	fake.Record(VanillaType::Tree)
	    .Position(30, 40)
	    .Value("info", world_import::k_FirstTreeRowNumber - static_cast<uint32_t>(TreeInfo::Pine))
	    .Value("scale", 0.9f);
	const auto imported = world_import::ReadForests(fake.Make());
	ASSERT_EQ(imported.trees.size(), 2U);
	EXPECT_EQ(imported.trees[0].number, 2U);
	EXPECT_EQ(imported.trees[0].row, TreeInfo::Oak);
	EXPECT_FLOAT_EQ(imported.trees[0].scale, 0.4f);
	EXPECT_FLOAT_EQ(imported.trees[0].maxSize, 1.1f);
	ASSERT_TRUE(imported.trees[0].magicWoodMultiplier.has_value());
	EXPECT_FLOAT_EQ(*imported.trees[0].magicWoodMultiplier, 1.75f);
	EXPECT_FALSE(imported.trees[1].magicWoodMultiplier.has_value());
}

// Each spell that a creature of a saved game holds reads as a spell that the creature cast
TEST(VanillaWorldImport, SavedGamesGiveEachHeldSpellOfACreatureItsValues)
{
	size_t held = 0;
	const auto files = ForEachSave([&held](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto& creature : FindCreatures(world.Bytes()))
		{
			const auto number = NumberOf(creature.object.creature.spellVisuals[0].key);
			if (!number.has_value())
			{
				continue;
			}
			++held;
			const auto* record = world.Record(*number);
			ASSERT_NE(record, nullptr) << path;
			const auto spell = world_import::ReadHeldSpell(world, *record);
			ASSERT_TRUE(spell.has_value()) << path << " object " << *number;
			EXPECT_EQ(spell->creator, creature.number) << path;
			std::cout << path << ": creature " << creature.number << " holds spell " << *number << " of magic type "
			          << static_cast<int>(spell->magicType) << ", age " << spell->age << " of " << spell->duration << "\n";
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << held << " creatures hold a spell\n";
}

// The object that cast a spell of the file gives the kind of its caster and the player of the spell
TEST(VanillaWorldImport, TheObjectThatCastASpellGivesItsCasterAndItsPlayer)
{
	FakeWorld fake;
	fake.Record(VanillaType::Game);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 7);
	fake.Record(VanillaType::WorshipSpellIcon);
	fake.Record(VanillaType::Creature);
	fake.Record(VanillaType::Tree);
	fake.Record(VanillaType::SpellShield)
	    .Value<uint32_t>("spellMagicType", static_cast<uint32_t>(MagicType::Shield))
	    .Ref("spellCreator", 2)
	    .Ref("spellPlayer", 2);
	const auto world = fake.Make();
	using Kind = world_import::ImportedSpellCaster::Kind;
	// The spell reads the player record of the spell
	const auto read = world_import::ReadHeldSpell(world, world.Records()[6]);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->creator, 2U);
	EXPECT_EQ(read->player, 2U);

	// The hand of a player, with a seed that no icon made
	auto hand = world_import::CasterOfSpell(world, *read);
	EXPECT_EQ(hand.kind, Kind::Hand);
	EXPECT_EQ(hand.player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(hand.creator, 2U);

	world_import::ImportedHeldSpell spell {.creator = 4, .player = 2};
	const auto icon = world_import::CasterOfSpell(world, spell);
	EXPECT_EQ(icon.kind, Kind::Icon);
	EXPECT_EQ(icon.player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(icon.creator, 4U);
	spell.creator = 5;
	EXPECT_EQ(world_import::CasterOfSpell(world, spell).kind, Kind::Creature);
	spell.creator = 6;
	EXPECT_EQ(world_import::CasterOfSpell(world, spell).kind, Kind::Object);

	// A script spell has no object, and a spell with no player record has the neutral player
	spell = {.creator = 0, .player = 0};
	const auto script = world_import::CasterOfSpell(world, spell);
	EXPECT_EQ(script.kind, Kind::None);
	EXPECT_EQ(script.player, PlayerNames::NEUTRAL);
	EXPECT_EQ(script.creator, 0U);
}

// Each record of the spell family is a spell. The save type between food and wood and water is not.
TEST(VanillaWorldImport, EachRecordOfTheSpellFamilyIsASpell)
{
	FakeWorld fake;
	for (uint32_t type = 0x09; type <= 0x15; ++type)
	{
		fake.Record(static_cast<VanillaType>(type));
	}
	fake.Record(VanillaType::MagicShield);
	fake.Record(VanillaType::MagicTeleport);
	const auto world = fake.Make();
	for (const auto& record : world.Records())
	{
		const bool spell = record.type >= 0x09 && record.type <= 0x15 && record.type != 0x0B;
		EXPECT_EQ(world_import::IsSpellRecord(record), spell) << record.type;
	}
}

// The import makes each kind of spell that no creature holds, but a spell that keeps only objects
TEST(VanillaWorldImport, TheImportMakesEachKindOfSpell)
{
	const auto spellOf = [](VanillaType type, MagicType magicType) {
		return world_import::ImportedHeldSpell {.saveType = static_cast<uint32_t>(type), .magicType = magicType};
	};
	for (const auto& [type, magicType] : std::array {
	         std::pair {VanillaType::SpellStormAndTornado, MagicType::StormWindRain},
	         std::pair {VanillaType::SpellStormAndTornado, MagicType::StormWindRainLightning},
	         std::pair {VanillaType::SpellStormAndTornado, MagicType::Tornado},
	         std::pair {VanillaType::SpellHeal, MagicType::Heal},
	         std::pair {VanillaType::SpellHeal, MagicType::HealPowerUpOne},
	         std::pair {VanillaType::SpellWater, MagicType::Water},
	         std::pair {VanillaType::SpellResource, MagicType::Food},
	         std::pair {VanillaType::SpellResource, MagicType::Wood},
	         std::pair {VanillaType::Spell, MagicType::LightningBolt},
	         std::pair {VanillaType::Spell, MagicType::LightningBoltPowerUpTwo},
	         std::pair {VanillaType::Spell, MagicType::ExplosionOne},
	         std::pair {VanillaType::SpellFlockFlying, MagicType::FlockFlying},
	         std::pair {VanillaType::SpellFlockGround, MagicType::FlockGround},
	         std::pair {VanillaType::SpellCreature, MagicType::CreatureSpellStrong},
	         std::pair {VanillaType::SpellForest, MagicType::Forest},
	         std::pair {VanillaType::SpellShield, MagicType::PhysicalShield},
	         std::pair {VanillaType::SpellTeleport, MagicType::Teleport},
	     })
	{
		EXPECT_TRUE(world_import::CanImportSpell(spellOf(type, magicType))) << static_cast<int>(magicType);
	}
	for (const auto magicType : {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo})
	{
		EXPECT_TRUE(world_import::CanImportSpell(spellOf(VanillaType::Spell, magicType))) << static_cast<int>(magicType);
	}
	EXPECT_FALSE(world_import::CanImportSpell(spellOf(VanillaType::SpellWithObjects, MagicType::Food)));
	EXPECT_FALSE(world_import::CanImportSpell(spellOf(VanillaType::SpellHeal, MagicType::None)));
}

// A fireball of the file keeps its fire, the atom that carries it, and the spell, the player and the strength of the
// effect of the atom
TEST(VanillaWorldImport, AFireballKeepsItsFireItsAtomAndTheSpellOfItsEffect)
{
	auto fake = GameAtTurn(40);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1);
	// 3: the ball, 4: its fire, 5: the atom, 6: the data of the rule that carries the ball, 7: the collection, 8: the
	// manager, 9: the spell, 10: the thing that heated the ball
	fake.Record(VanillaType::MagicFireBall)
	    .Position(300, 400)
	    .Ref("fire", 4)
	    .Ref("fireBallAtom", 5)
	    .Value("fireBallRain", uint8_t {0});
	fake.Record(VanillaType::FireEffect).Value("fireTemperature", 900.0f).Value("firePrevious", 950.0f).Ref("fireSource", 10);
	fake.Record(VanillaType::AtomCore)
	    .Refs("atomRuleData", {6})
	    .Value("atomBaseScale", 2.0f)
	    .Value("atomRuleScale", 1.5f)
	    .Ref("atomCollection", 7);
	fake.Record(VanillaType::AtomDataAttachFireBall)
	    .Value("fireBallNotMade", uint8_t {0})
	    .Value("fireBallTurn", uint32_t {39})
	    .Ref("fireBallObject", 3);
	fake.Record(VanillaType::AtomCollection).Ref("collectionManager", 8);
	fake.Record(VanillaType::ParticleManager)
	    .Ref("managerPlayer", 2)
	    .Ref("managerSpell", 9)
	    .Value("managerInfo", std::array<float, 15> {0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
	                                                 0.75f, 0.0f, 0.0f});
	fake.Record(VanillaType::Spell).Value("spellMagicType", static_cast<uint32_t>(MagicType::Fireball));
	fake.Record(VanillaType::Creature);
	const auto world = fake.Make();

	EXPECT_FALSE(world_import::ReadFireBall(world, world.Records()[3]).has_value()) << "the fire is not a ball";
	const auto ball = world_import::ReadFireBall(world, world.Records()[2]);
	ASSERT_TRUE(ball.has_value());
	EXPECT_EQ(ball->number, 3U);
	EXPECT_EQ(ball->position.x, 300);
	EXPECT_EQ(ball->position.z, 400);
	EXPECT_EQ(ball->atom, 5U);
	EXPECT_EQ(ball->spell, 9U);
	EXPECT_EQ(ball->player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(ball->strength, 0.75f);
	EXPECT_EQ(ball->radius, 3.0f);
	EXPECT_FALSE(ball->affectedByRain);
	EXPECT_EQ(ball->temperature, 900.0f);
	EXPECT_EQ(ball->previous, 950.0f);
	EXPECT_EQ(ball->source, 10U);
	EXPECT_EQ(ball->lastTurn, 39U);
}

// A fireball whose atom is gone keeps its fire, and has no spell and no player
TEST(VanillaWorldImport, AFireballWithNoAtomHasNoSpell)
{
	auto fake = GameAtTurn(40);
	fake.Record(VanillaType::MagicFireBall).Position(1, 2).Ref("fire", 3).Ref("fireBallAtom", 0);
	fake.Record(VanillaType::FireEffect).Value("fireTemperature", 500.0f).Value("firePrevious", 400.0f);
	const auto world = fake.Make();
	const auto ball = world_import::ReadFireBall(world, world.Records()[1]);
	ASSERT_TRUE(ball.has_value());
	EXPECT_EQ(ball->spell, 0U);
	EXPECT_FALSE(ball->player.has_value());
	EXPECT_FALSE(ball->lastTurn.has_value());
	EXPECT_TRUE(ball->affectedByRain);
	EXPECT_EQ(ball->temperature, 500.0f);
	EXPECT_EQ(ball->previous, 400.0f);
}

// Each player record gives its list of teleport stones in the order of the file, the head first
TEST(VanillaWorldImport, EachPlayerGivesItsStonesInTheOrderOfTheFile)
{
	FakeWorld fake;
	fake.Record(VanillaType::Game);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 0).Refs("teleports", {9, 5, 7});
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1).Refs("teleports", {});
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 2).Refs("teleports", {6});
	const auto lists = world_import::ReadStoneLists(fake.Make());
	ASSERT_EQ(lists.size(), 2U);
	EXPECT_EQ(lists[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(lists[0].stones, (std::vector<uint32_t> {9, 5, 7}));
	EXPECT_EQ(lists[1].player, PlayerNames::PLAYER_THREE);
	EXPECT_EQ(lists[1].stones, (std::vector<uint32_t> {6}));
}

// A villager in the state of the shelter keeps the point that it looks at as the one value of the state
TEST(VanillaWorldImport, AVillagerUnderAShieldGivesThePointThatItLooksAt)
{
	constexpr auto k_Shelter = static_cast<uint8_t>(VillagerStates::AmazedByMagicShieldReaction);
	constexpr auto k_Walk = static_cast<uint8_t>(VillagerStates::MoveToPos);
	EXPECT_TRUE(world_import::CanKeepStateValues(k_Shelter));
	const auto pointOf = [](int32_t x, int32_t z) {
		std::vector<uint8_t> bytes(2 * sizeof(int32_t));
		std::memcpy(bytes.data(), &x, sizeof(x));
		std::memcpy(bytes.data() + sizeof(x), &z, sizeof(z));
		return std::vector {world_import::ImportedStateValue {.object = std::nullopt, .bytes = bytes}};
	};
	const auto top = pointOf(0x12345, -0x678);
	const auto final = pointOf(0x22222, 0x33333);
	EXPECT_EQ(world_import::ShieldLookPointOf({k_Shelter, 0}, top, {}), glm::ivec2(0x12345, -0x678));
	// A villager that walks to its shelter keeps the point in its final state
	EXPECT_EQ(world_import::ShieldLookPointOf({k_Walk, k_Shelter}, {}, final), glm::ivec2(0x22222, 0x33333));
	// Another state, or a value of another size, gives no point
	EXPECT_FALSE(world_import::ShieldLookPointOf({k_Walk, 0}, top, final).has_value());
	const std::vector reference {world_import::ImportedStateValue {.object = 3U, .bytes = {}}};
	EXPECT_FALSE(world_import::ShieldLookPointOf({k_Shelter, 0}, reference, {}).has_value());

	// The record of a villager gives its point from its states
	FakeWorld fake;
	fake.Record(VanillaType::Villager)
	    .Value("action", std::array<uint8_t, 6> {k_Shelter, 0, 0, 0, 0, 0})
	    .Value("topStateValue", std::array<int32_t, 2> {500, 600});
	fake.Record(VanillaType::Villager).Value("action", std::array<uint8_t, 6> {k_Walk, 0, 0, 0, 0, 0});
	const auto world = fake.Make();
	EXPECT_EQ(world_import::ShieldLookPointOf(world, world.Records()[0]), glm::ivec2(500, 600));
	EXPECT_FALSE(world_import::ShieldLookPointOf(world, world.Records()[1]).has_value());
}

// Each spell of a saved game reads with its caster
TEST(VanillaWorldImport, SavedGamesGiveEachSpellItsCaster)
{
	size_t spells = 0;
	const auto files = ForEachSave([&spells](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto& record : world.Records())
		{
			if (!world_import::IsSpellRecord(record))
			{
				continue;
			}
			const auto spell = world_import::ReadHeldSpell(world, record);
			ASSERT_TRUE(spell.has_value()) << path << " object " << record.number;
			const auto caster = world_import::CasterOfSpell(world, *spell);
			++spells;
			std::cout << path << ": spell " << record.number << " of magic type " << static_cast<int>(spell->magicType)
			          << " cast by object " << spell->creator << " of kind " << static_cast<int>(caster.kind) << " for player "
			          << static_cast<int>(caster.player) << ", age " << spell->age << ", " << spell->objects.size()
			          << " objects\n";
		}
		for (const auto& list : world_import::ReadStoneLists(world))
		{
			std::cout << path << ": player " << static_cast<int>(list.player) << " has " << list.stones.size() << " stones\n";
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << spells << " spells\n";
}

namespace
{
/// The number of the row of the table for the seed
constexpr uint32_t SeedRow(SpellSeedType seed)
{
	return world_import::k_FirstSpellSeedRowNumber - static_cast<uint32_t>(seed);
}
} // namespace

// A spell seed keeps its row, its power-up, its charge, what its last spell left, its flags, its icon and its spell. The
// interface status of its hand gives its player.
TEST(VanillaWorldImport, ASpellSeedReadsItsValues)
{
	FakeWorld fake;
	// 1: the seed, 2: the player, 3: the interface, 4: the interface status
	fake.Record(VanillaType::SpellSeed)
	    .Position(5, 6)
	    .Value<uint32_t>("seedPowerUp", 1)
	    .Value<uint32_t>("seedInfo", SeedRow(SpellSeedType::Food))
	    .Ref("seedIcon", 30)
	    .Ref("seedCreator", 30)
	    .Ref("seedSpell", 31)
	    .Ref("seedStatus", 4)
	    .Value<uint8_t>("seedLinkedByCast", 1)
	    .Value<uint8_t>("seedLearnedFrom", 1)
	    .Value<float>("seedChantStore", 120.0f)
	    .Value<float>("seedStoredChants", 40.0f)
	    .Value<float>("seedStoredAge", 2.5f)
	    .Value<uint32_t>("seedStoredMaxObjects", 9)
	    .Value<float>("seedPower", 0.75f)
	    .Value<float>("seedMultiplier", 1.5f)
	    .Value<uint8_t>("seedReady", 1)
	    .Value<uint32_t>("seedTurnsInHand", 33)
	    .Value<uint32_t>("seedLastCastType", static_cast<uint32_t>(MagicType::Food))
	    .Value<uint8_t>("seedFromGlobe", 1);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1).Refs("interfaces", {3});
	fake.Record(VanillaType::PlayerInterface);
	fake.Record(VanillaType::PlayerInterfaceStatus).Ref("interface", 3);
	const auto world = fake.Make();

	const auto seed = world_import::ReadSpellSeed(world, world.Records().front());
	ASSERT_TRUE(seed.has_value());
	EXPECT_EQ(seed->number, 1U);
	EXPECT_EQ(seed->position, (MapCoords {.x = 5, .z = 6, .altitude = 0.0f}));
	EXPECT_EQ(seed->seedType, SpellSeedType::Food);
	EXPECT_EQ(seed->powerUp, 1);
	EXPECT_EQ(seed->player, std::optional(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(seed->icon, 30U);
	EXPECT_EQ(seed->spell, 31U);
	EXPECT_TRUE(seed->linkedByCast);
	EXPECT_TRUE(seed->learnedFrom);
	EXPECT_EQ(seed->chantStore, 120.0f);
	EXPECT_EQ(seed->storedChants, 40.0f);
	EXPECT_EQ(seed->storedAge, 2.5f);
	EXPECT_EQ(seed->storedMaxObjects, 9);
	EXPECT_EQ(seed->power, 0.75f);
	EXPECT_EQ(seed->castMultiplier, 1.5f);
	EXPECT_TRUE(seed->ready);
	EXPECT_EQ(seed->turnsInHand, 33U);
	EXPECT_EQ(seed->lastCastType, MagicType::Food);
	EXPECT_TRUE(seed->fromGlobe);

	// A record that is not a spell seed gives no seed
	EXPECT_FALSE(world_import::ReadSpellSeed(world, world.Records()[1]).has_value());
}

// The plain miracle and no stored values are -1 in the file. A seed with no status has the player of the player record
// that made it. A seed of no row of the table, and a last cast of no magic type of the tables, give nothing.
TEST(VanillaWorldImport, ASpellSeedWithNoStatusHasThePlayerOfItsCreator)
{
	FakeWorld fake;
	fake.Record(VanillaType::SpellSeed)
	    .Position(1, 2)
	    .Value<uint32_t>("seedPowerUp", 0xFFFFFFFFU)
	    .Value<uint32_t>("seedInfo", SeedRow(SpellSeedType::Water))
	    .Ref("seedCreator", 3)
	    .Ref("seedStatus", 0)
	    .Value<float>("seedStoredChants", -1.0f)
	    .Value<uint32_t>("seedStoredMaxObjects", 0xFFFFFFFFU)
	    .Value<uint32_t>("seedLastCastType", 999);
	fake.Record(VanillaType::SpellSeed).Position(1, 2).Value<uint32_t>("seedInfo", 7);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 0);
	const auto world = fake.Make();

	const auto seed = world_import::ReadSpellSeed(world, world.Records()[0]);
	ASSERT_TRUE(seed.has_value());
	EXPECT_EQ(seed->seedType, SpellSeedType::Water);
	EXPECT_EQ(seed->powerUp, -1);
	EXPECT_EQ(seed->player, std::optional(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(seed->icon, 0U);
	EXPECT_EQ(seed->storedChants, -1.0f);
	EXPECT_EQ(seed->storedMaxObjects, -1);
	EXPECT_EQ(seed->lastCastType, MagicType::None);
	EXPECT_FALSE(world_import::ReadSpellSeed(world, world.Records()[1]).has_value());
}

// The hand of each player keeps its seed with the other objects that it holds. A hand with no player is not in the list.
TEST(VanillaWorldImport, EachHandGivesTheSeedThatItHolds)
{
	FakeWorld fake;
	// 1: a seed, 2: a tree, 3: the player, 4: the interface, 5: the status of the hand, 6: a status with no player
	fake.Record(VanillaType::SpellSeed).Position(1, 2).Value<uint32_t>("seedInfo", SeedRow(SpellSeedType::Wood));
	fake.Record(VanillaType::Tree).Position(1, 2);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 0).Refs("interfaces", {4});
	fake.Record(VanillaType::PlayerInterface);
	fake.Record(VanillaType::PlayerInterfaceStatus).Ref("interface", 4).Refs("handHeld", {2, 1});
	fake.Record(VanillaType::PlayerInterfaceStatus).Ref("interface", 0).Refs("handHeld", {1});
	const auto seeds = world_import::ReadHandSeeds(fake.Make());
	ASSERT_EQ(seeds.size(), 1U);
	EXPECT_EQ(seeds[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(seeds[0].seed, 1U);
}

// A spell keeps the seed in the hand that cast it
TEST(VanillaWorldImport, ASpellReadsItsSeed)
{
	FakeWorld fake;
	fake.Record(VanillaType::SpellResource)
	    .Position(3, 4)
	    .Value<uint32_t>("spellMagicType", static_cast<uint32_t>(MagicType::Wood))
	    .Ref("spellSeed", 12);
	const auto world = fake.Make();
	const auto spell = world_import::ReadHeldSpell(world, world.Records().front());
	ASSERT_TRUE(spell.has_value());
	EXPECT_EQ(spell->seed, 12U);
}

// The seed takes the values of the file. A seed of an icon came from the worship, unless it came out of a globe. The
// seed keeps its own player, worship site and spell.
TEST(VanillaWorldImport, ASeedTakesTheValuesOfTheFile)
{
	const world_import::ImportedSpellSeed file {
	    .seedType = SpellSeedType::LightningBolt,
	    .powerUp = 2,
	    .player = PlayerNames::PLAYER_TWO,
	    .icon = 7,
	    .spell = 8,
	    .chantStore = 10.0f,
	    .learnedFrom = true,
	    .storedChants = 3.0f,
	    .storedAge = 1.5f,
	    .storedMaxObjects = 4,
	    .castMultiplier = 2.0f,
	    .power = 0.5f,
	    .fromGlobe = false,
	    .ready = true,
	    .turnsInHand = 12,
	    .lastCastType = MagicType::LightningBolt,
	    .linkedByCast = true,
	};
	ecs::components::SpellSeed seed {.player = PlayerNames::PLAYER_ONE};
	world_import::PutSeedValues(seed, file);
	EXPECT_EQ(seed.seedType, SpellSeedType::LightningBolt);
	EXPECT_EQ(seed.powerUp, 2);
	EXPECT_EQ(seed.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(seed.chantStore, 10.0f);
	EXPECT_TRUE(seed.learnedFrom);
	EXPECT_EQ(seed.storedChants, 3.0f);
	EXPECT_EQ(seed.storedAge, 1.5f);
	EXPECT_EQ(seed.storedMaxObjects, 4);
	EXPECT_EQ(seed.castMultiplier, 2.0f);
	EXPECT_EQ(seed.power, 0.5f);
	EXPECT_TRUE(seed.hasIcon);
	EXPECT_EQ(seed.origin, magic::SeedOrigin::Worship);
	EXPECT_TRUE(seed.ready);
	EXPECT_EQ(seed.turnsInHand, 12U);
	EXPECT_EQ(seed.lastCastType, MagicType::LightningBolt);
	EXPECT_TRUE(seed.linkedByCast);
	EXPECT_EQ(seed.spell, entt::entity {entt::null});

	auto globe = file;
	globe.fromGlobe = true;
	world_import::PutSeedValues(seed, globe);
	EXPECT_TRUE(seed.fromGlobe);
	EXPECT_EQ(seed.origin, magic::SeedOrigin::Bubble);
	auto noIcon = file;
	noIcon.icon = 0;
	world_import::PutSeedValues(seed, noIcon);
	EXPECT_FALSE(seed.hasIcon);
	EXPECT_EQ(seed.origin, magic::SeedOrigin::Bubble);
}

// Each spell seed of the saved games reads, and each seed in a hand is a seed that reads
TEST(VanillaWorldImport, TheSpellSeedsOfTheSavedGamesRead)
{
	size_t seeds = 0;
	size_t held = 0;
	const auto files = ForEachSave([&seeds, &held](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto* record : world.OfType(VanillaType::SpellSeed))
		{
			++seeds;
			EXPECT_TRUE(world_import::ReadSpellSeed(world, *record).has_value()) << path << " " << record->number;
		}
		for (const auto& hand : world_import::ReadHandSeeds(world))
		{
			++held;
			const auto* record = world.Record(hand.seed);
			ASSERT_NE(record, nullptr) << path;
			EXPECT_TRUE(world_import::ReadSpellSeed(world, *record).has_value()) << path << " " << hand.seed;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << seeds << " spell seeds and " << held << " seeds in a hand in the saves\n";
}

/// The game's load keeps the lists of fields and of fish farms in the order of the file
TEST(VanillaSceneryImport, ReadsTheGamesListsOfFieldsAndFishFarms)
{
	FakeWorld fake;
	auto& game = fake.Record(VanillaType::Game);
	for (size_t list = 0; list < world_import::k_GameObjectLists; ++list)
	{
		game.Refs("lists", list == world_import::k_GameFieldList      ? std::vector<uint32_t> {7, 2}
		                   : list == world_import::k_GameFishFarmList ? std::vector<uint32_t> {5, 3, 4}
		                                                              : std::vector<uint32_t> {});
	}
	const auto imported = world_import::ReadFarms(fake.Make());
	EXPECT_EQ(imported.fieldList, (std::vector<uint32_t> {7, 2}));
	EXPECT_EQ(imported.fishFarmList, (std::vector<uint32_t> {5, 3, 4}));
}

/// The things join their list again from the oldest, so the newest of the file is first in the list
TEST(VanillaSceneryImport, ThingsJoinTheirListFromTheOldestOfTheFile)
{
	const std::unordered_map<uint32_t, entt::entity> made {
	    {3, entt::entity {30}}, {4, entt::entity {40}}, {5, entt::entity {50}}};
	// 9 was not made by the import, 3 is in the list twice
	const std::vector<uint32_t> list {5, 9, 3, 4, 3};
	const auto order = world_import::ListedToJoin(list, made);
	EXPECT_EQ(order, (std::vector<entt::entity> {entt::entity {30}, entt::entity {40}, entt::entity {50}}));
}
