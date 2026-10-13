/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The crossing file's line for each kind of thing a vortex takes, read from its components

#include <fmt/format.h>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAge.h"
#include "ECS/VortexCrossingLines.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Lexer.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
constexpr float k_Ground = 20.0f;
/// The vortex stands at (1000, 2000) on the land
const map_coords::MapCoords k_Vortex = map_coords::FromMetres({1000.0f, 2000.0f});

vortex_crossing::World FakeWorld()
{
	return {
	    .turn = 30000,
	    .ground = [](glm::vec2) { return k_Ground; },
	    .villagerRow = [](Tribe, VillagerNumber) { return 42; },
	    .potResource = [](PotInfo) { return 1; },
	    .life = [](entt::entity) { return 0.75f; },
	    .player = [](entt::entity) { return std::optional<PlayerNames> {}; },
	    .magicName = [](SpellSeedType seed,
	                    int powerUp) { return fmt::format("MAGIC_{}_{}", static_cast<int>(seed), powerUp); },
	    .filedAsStatic = [](entt::entity) { return true; },
	};
}

/// A line as the land scripts' reader takes it: its command and the kind of each value
struct ReadLine
{
	std::string command;
	std::vector<lhscriptx::ParameterType> kinds;
	std::vector<lhscriptx::Token> values;
};

/// Reads a line back as the reader's lexer does, a quoted place "x,z" being a place and other quoted text text
ReadLine Read(const std::string& text)
{
	using lhscriptx::ParameterType;
	using lhscriptx::Token;
	lhscriptx::Lexer lexer(text);
	ReadLine read;
	read.command = lexer.GetToken().Identifier();
	for (auto token = lexer.GetToken(); !token.IsEOF() && !token.IsEOL(); token = lexer.GetToken())
	{
		switch (token.GetType())
		{
		case Token::Type::Identifier:
			read.kinds.push_back(ParameterType::String);
			break;
		case Token::Type::String:
			read.kinds.push_back(std::ranges::count(token.StringValue(), ',') == 1 ? ParameterType::Vector
			                                                                       : ParameterType::String);
			break;
		case Token::Type::Integer:
			read.kinds.push_back(ParameterType::Number);
			break;
		case Token::Type::Float:
			read.kinds.push_back(ParameterType::Float);
			break;
		default:
			continue;
		}
		read.values.push_back(token);
	}
	return read;
}

/// Whether the reader knows the command and takes its values as they are written
bool ReaderTakes(const ReadLine& read)
{
	const auto& signatures = lhscriptx::FeatureScriptCommands::k_Signatures;
	const auto signature =
	    std::ranges::find_if(signatures, [&read](const auto& each) { return read.command == each.name.data(); });
	if (signature == signatures.end())
	{
		return false;
	}
	const auto expected = std::ranges::find(signature->parameters, lhscriptx::ParameterType::None);
	return std::ranges::equal(signature->parameters.begin(), expected, read.kinds.begin(), read.kinds.end());
}

std::string LineText(const Registry& registry, entt::entity thing)
{
	const auto line = vortex_crossing::LineFor(registry, thing, k_Vortex, FakeWorld());
	return line.has_value() ? vortex::crossing::Text(*line) : std::string {};
}

entt::entity Thing(Registry& registry, glm::vec3 position, const glm::mat3& rotation, float scale)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, rotation, glm::vec3(scale));
	return entity;
}
} // namespace

TEST(VortexCrossingLines, AVillagerIsWrittenByItsTribeAndJob)
{
	Registry registry;
	const auto town = registry.Create();
	registry.Assign<Town>(town, Town {.id = 3});
	const auto villager = Thing(registry, {1010.0f, k_Ground, 2020.0f}, glm::mat3(1.0f), 1.0f);
	auto& data = registry.Assign<Villager>(villager);
	data.tribe = Tribe::CELTIC;
	data.number = VillagerNumber::Farmer;
	data.town = town;
	data.birthTurn = 30000 - (25 * villager_age::k_TurnsPerYear);
	EXPECT_EQ(LineText(registry, villager), "CREATE_TOWN_VILLAGER(3, \"10.00,20.00\", CELTIC_FARMER, 25)\n");
	// One of no tribe by its row of the villagers' table, here without a town
	data.tribe = Tribe::NONE;
	data.town = entt::null;
	EXPECT_EQ(LineText(registry, villager), "CREATE_SPECIAL_TOWN_VILLAGER(-1, \"10.00,20.00\", 42, 25)\n");
}

TEST(VortexCrossingLines, ATreeKeepsItsTurnAndSizes)
{
	Registry registry;
	const auto tree = Thing(registry, {990.0f, k_Ground, 2000.0f}, glm::eulerAngleY(-0.5f), 0.8f);
	registry.Assign<Tree>(tree, Tree {.type = TreeInfo::Oak, .maxSize = 1.0f, .nonScenic = true});
	EXPECT_EQ(LineText(registry, tree),
	          fmt::format("CREATE_NEW_TREE(-1, \"-10.00,0.00\", {}, 1, 0.500000, 0.800000, 1.000000)\n",
	                      static_cast<int>(TreeInfo::Oak)));
}

TEST(VortexCrossingLines, ARockKeepsItsHeightAboveTheLandAndItsTurns)
{
	Registry registry;
	const auto rock = Thing(registry, {1000.0f, k_Ground + 2.0f, 2000.0f}, glm::eulerAngleXYZ(-0.1f, -0.2f, -0.3f), 1.5f);
	registry.Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
	EXPECT_EQ(LineText(registry, rock), fmt::format("CREATE_MOBILE_STATIC(\"0.00,0.00\", {}, 2.000000, 0.100000, 0.200000, "
	                                                "0.300000, 1.500000)\n",
	                                                static_cast<int>(MobileStaticInfo::Rock)));
}

TEST(VortexCrossingLines, AnArtefactOfAPlayerIsMadeOneAgainOfNoTown)
{
	Registry registry;
	const auto town = registry.Create();
	registry.Assign<Town>(town, Town {.id = 3});
	const auto rock = Thing(registry, {1000.0f, k_Ground, 2000.0f}, glm::mat3(1.0f), 1.0f);
	registry.Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
	auto& artefact = registry.Assign<TownArtefact>(rock);
	artefact.town = town;
	artefact.inTown = true;
	artefact.value = 2.5f;
	// Nobody's artefact is written as a plain static
	const auto plain = fmt::format("CREATE_MOBILE_STATIC(\"0.00,0.00\", {}, 0.000000, 0.000000, 0.000000, 0.000000, "
	                               "1.000000)\n",
	                               static_cast<int>(MobileStaticInfo::Rock));
	EXPECT_EQ(LineText(registry, rock), plain);
	registry.Get<TownArtefact>(rock).player = PlayerNames::PLAYER_TWO;
	const auto text = LineText(registry, rock);
	EXPECT_EQ(text, plain + "MAKE_LAST_OBJECT_ARTIFACT(-1, \"PLAYER_TWO\", 2.500000)\n");
	// Each is a line of its own, read on a turn of its own
	const auto second = text.substr(plain.size());
	const auto read = Read(second);
	EXPECT_TRUE(ReaderTakes(read));
	EXPECT_EQ(*read.values.at(0).IntegerValue(), -1);
	EXPECT_EQ(read.values.at(1).StringValue(), "PLAYER_TWO");
	EXPECT_FLOAT_EQ(*read.values.at(2).FloatValue(), 2.5f);
}

TEST(VortexCrossingLines, APotIsWrittenUnlessItIsAStoragePitsPile)
{
	Registry registry;
	const auto pot = Thing(registry, {1000.0f, k_Ground, 2000.0f}, glm::mat3(1.0f), 1.0f);
	registry.Assign<Pot>(pot, Pot {.amount = 1000, .maxAmount = 1000, .type = PotInfo::HandFood});
	EXPECT_EQ(LineText(registry, pot),
	          fmt::format("CREATE_POT(\"0.00,0.00\", {}, 1, 1000)\n", static_cast<int>(PotInfo::HandFood)));
	const auto pit = registry.Create();
	registry.Assign<StoragePit>(
	    pit, StoragePit {.woodPiles = {entt::null, entt::null, entt::null, entt::null, entt::null}, .foodPile = pot});
	EXPECT_EQ(LineText(registry, pot), "");
}

TEST(VortexCrossingLines, ADeadTreeOfNobodyIsTheNeutralPlayers)
{
	Registry registry;
	const auto dead = Thing(registry, {1000.0f, k_Ground, 2000.0f}, glm::mat3(1.0f), 1.0f);
	registry.Assign<DeadTree>(dead, DeadTree {.type = TreeInfo::Oak});
	EXPECT_EQ(LineText(registry, dead),
	          fmt::format("CREATE_DEAD_TREE(\"0.00,0.00\", \"NEUTRAL\", {}, 0.750000, 0.000000, 0.000000, 0.000000)\n",
	                      static_cast<int>(TreeInfo::Oak)));
}

TEST(VortexCrossingLines, ABuildingHasNoLine)
{
	Registry registry;
	const auto thing = Thing(registry, {1000.0f, k_Ground, 2000.0f}, glm::mat3(1.0f), 1.0f);
	EXPECT_EQ(LineText(registry, thing), "");
}

TEST(VortexCrossingLines, AGlobeIsWrittenByTheMiracleItsSeedCastsAtItsPowerUp)
{
	Registry registry;
	const auto globe = Thing(registry, {1000.0f, k_Ground + 3.0f, 1990.0f}, glm::mat3(1.0f), 1.0f);
	registry.Assign<OneOffSpellSeed>(globe, OneOffSpellSeed {.seedType = SpellSeedType::Food, .powerUp = 1});
	const auto text = LineText(registry, globe);
	EXPECT_EQ(text, fmt::format("CREATE_ONE_SHOT_SPELL_PU(\"0.00,-10.00\", \"MAGIC_{}_1\")\n",
	                            static_cast<int>(SpellSeedType::Food)));
	const auto read = Read(text);
	EXPECT_TRUE(ReaderTakes(read));
	EXPECT_EQ(read.values.at(0).StringValue(), "0.00,-10.00");
	EXPECT_EQ(read.values.at(1).StringValue(), fmt::format("MAGIC_{}_1", static_cast<int>(SpellSeedType::Food)));
}

TEST(VortexCrossingLines, TheReaderTakesEveryKindsLine)
{
	Registry registry;
	const auto town = registry.Create();
	registry.Assign<Town>(town, Town {.id = 3});
	std::vector<entt::entity> things;
	const auto add = [&registry, &things](glm::vec3 position) {
		things.push_back(Thing(registry, position, glm::mat3(1.0f), 1.0f));
		return things.back();
	};
	auto& villager = registry.Assign<Villager>(add({1010.0f, k_Ground, 2020.0f}));
	villager.tribe = Tribe::CELTIC;
	villager.number = VillagerNumber::Farmer;
	villager.town = town;
	auto& special = registry.Assign<Villager>(add({1010.0f, k_Ground, 2020.0f}));
	special.tribe = Tribe::NONE;
	registry.Assign<Animal>(add({1000.0f, k_Ground, 2000.0f})).type = AnimalInfo::Cow;
	registry.Assign<Pot>(add({1000.0f, k_Ground, 2000.0f}), Pot {.amount = 10, .maxAmount = 1000, .type = PotInfo::HandFood});
	registry.Assign<Tree>(add({1000.0f, k_Ground, 2000.0f}), Tree {.type = TreeInfo::Oak, .maxSize = 1.0f});
	registry.Assign<MobileStatic>(add({1000.0f, k_Ground, 2000.0f}), MobileStaticInfo::Rock);
	registry.Assign<MobileObject>(add({1000.0f, k_Ground, 2000.0f}), MobileObjectInfo::EgyptBarrel);
	registry.Assign<DeadTree>(add({1000.0f, k_Ground, 2000.0f}), DeadTree {.type = TreeInfo::Oak});
	registry.Assign<OneOffSpellSeed>(add({1000.0f, k_Ground, 2000.0f}), OneOffSpellSeed {.seedType = SpellSeedType::Food});
	for (const auto thing : things)
	{
		const auto text = LineText(registry, thing);
		EXPECT_TRUE(ReaderTakes(Read(text))) << text;
	}
}

TEST(VortexCrossingLines, StaticsOfAKindJustWhereAnotherStandsAreWrittenWithIt)
{
	Registry registry;
	const auto rock = [&registry](glm::vec3 position, MobileStaticInfo type) {
		const auto entity = Thing(registry, position, glm::mat3(1.0f), 1.0f);
		registry.Assign<MobileStatic>(entity, type);
		return entity;
	};
	const auto first = rock({1000.0f, k_Ground, 2000.0f}, MobileStaticInfo::Rock);
	const auto twin = rock({1000.0f, k_Ground, 2000.0f}, MobileStaticInfo::Rock);
	// Another kind, a little higher, a little across, or flying, is not the same
	rock({1000.0f, k_Ground, 2000.0f}, MobileStaticInfo::DeadTree);
	rock({1000.0f, k_Ground + 0.5f, 2000.0f}, MobileStaticInfo::Rock);
	rock({1000.5f, k_Ground, 2000.0f}, MobileStaticInfo::Rock);
	registry.Assign<InPhysics>(rock({1000.0f, k_Ground, 2000.0f}, MobileStaticInfo::Rock));
	EXPECT_EQ(vortex_crossing::IdenticalStatics(registry, first, FakeWorld()), std::vector<entt::entity> {twin});
	// Only statics have fellows
	const auto tree = Thing(registry, {1000.0f, k_Ground, 2000.0f}, glm::mat3(1.0f), 1.0f);
	registry.Assign<Tree>(tree, Tree {.type = TreeInfo::Oak});
	EXPECT_TRUE(vortex_crossing::IdenticalStatics(registry, tree, FakeWorld()).empty());
}

TEST(VortexCrossingLines, EveryAnimalIsWrittenWithItsAge)
{
	Registry registry;
	// A bird as much as a grazer: every animal was born on a turn of its own
	const auto bird = Thing(registry, {1000.0f, k_Ground + 30.0f, 2000.0f}, glm::mat3(1.0f), 1.0f);
	auto& animal = registry.Assign<Animal>(bird);
	animal.type = AnimalInfo::Dove;
	animal.birthTurn = villager_age::BirthTurnFor(30000, 3);
	EXPECT_EQ(LineText(registry, bird),
	          fmt::format("CREATE_NEW_ANIMAL(\"0.00,0.00\", {}, -1, -1, 3)\n", static_cast<int>(AnimalInfo::Dove)));
}
