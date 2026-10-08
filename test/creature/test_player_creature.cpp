/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player's own creature: the profile's mind file (the creature-file setting) and the script natives that act on
// that creature

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureMind.h"
#include "Debug/StateHash.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "creature/CreatureSystemWorld.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureLeash;
using openblack::ecs::components::CreatureMindState;

namespace
{
class ProfileCreatureTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		static std::atomic<int> s_count {0};
		_root = std::filesystem::temp_directory_path() /
		        ("openblack_test_player_creature_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
		         std::to_string(s_count++));
		std::filesystem::create_directories(_root / "Scripts" / "CreatureMind");
		Locator::filesystem::value().SetGamePath(_root);
		Locator::config::emplace();
	}
	void TearDown() override
	{
		std::error_code ec;
		std::filesystem::remove_all(_root, ec);
	}

	void WriteMind(const std::string& file) const
	{
		std::ofstream(_root / "Scripts" / "CreatureMind" / file, std::ios::binary).put('\0');
	}

	// first, so that they go last
	const test::RestoreService<Locator::config> _config;
	const test::ScopedDefaultFileSystem _fileSystem;
	std::filesystem::path _root;
};

/// The leash service as the natives see it: the creature a player leads is what the test sets, and the calls that
/// teach, hand over or house a creature are recorded
class RecordingLeash final: public systems::LeashSystemInterface
{
public:
	std::optional<entt::entity> playersCreature;
	std::vector<std::pair<entt::entity, LeashType>> known;
	std::vector<std::pair<entt::entity, bool>> leashable;
	std::vector<std::pair<entt::entity, glm::vec3>> homes;

	void ProcessTurn() override {}
	void Update(float) override {}
	[[nodiscard]] bool Knows(entt::entity, LeashType) const override { return false; }
	void SetKnown(entt::entity creature, LeashType type, bool isKnown) override
	{
		EXPECT_TRUE(isKnown);
		known.emplace_back(creature, type);
	}
	[[nodiscard]] bool IsLeashable(entt::entity) const override { return false; }
	bool SetLeashable(entt::entity creature, bool value) override
	{
		leashable.emplace_back(creature, value);
		return true;
	}
	void SetOwner(entt::entity, PlayerNames) override {}
	void ClaimOnArrival(entt::entity) override {}
	[[nodiscard]] creature_leash::Refusal WhyNot(PlayerNames, entt::entity, LeashType) const override
	{
		return creature_leash::Refusal::None;
	}
	[[nodiscard]] std::optional<Refused> LastRefusal(PlayerNames) const override { return std::nullopt; }
	bool PutOn(entt::entity, LeashType) override { return false; }
	void TakeOff(entt::entity) override {}
	bool Toggle(entt::entity) override { return false; }
	bool ChangeType(entt::entity, LeashType) override { return false; }
	bool TieTo(entt::entity, entt::entity) override { return false; }
	void UntieToHand(entt::entity) override {}
	void SetWorks(entt::entity, bool) override {}
	void ConfineToHome(entt::entity, float) override {}
	void ClearConfinement(entt::entity) override {}
	void SetHome(entt::entity creature, const glm::vec3& home) override { homes.emplace_back(creature, home); }
	[[nodiscard]] bool FreeOfHome(entt::entity) const override { return true; }
	[[nodiscard]] bool IsLeashed(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> TiedTo(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] LeashType TypeOf(entt::entity) const override { return LeashType::None; }
	[[nodiscard]] LeashType Picked(entt::entity) const override { return LeashType::None; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames) const override { return playersCreature; }
	bool PressKey(PlayerNames, creature_leash::LeashKey) override { return false; }
	bool TapCreature(PlayerNames, entt::entity) override { return false; }
	bool TakeOffHeldLeash(PlayerNames) override { return false; }
};

/// A world with a creature of the first player and a thing that is not a creature
class PlayerCreatureNativesTest: public ::testing::Test
{
protected:
	PlayerCreatureNativesTest()
	    : creature(test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE))
	    , other(Reg().Create())
	{
		Reg().Assign<components::Transform>(other, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	}

	static ecs::Registry& Reg() { return test::creature_world::World::Registry(); }

	test::creature_world::World world;
	RecordingLeash leash;
	entt::entity creature;
	entt::entity other;
};
} // namespace

TEST(PlayerCreature, ProfileMindPathJoinsTheFolderAndTheFile)
{
	const auto path = player_creature::ProfileMindPath("C4ba71b36.erc", std::filesystem::path("Scripts") / "CreatureMind");
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(*path, std::filesystem::path("Scripts") / "CreatureMind" / "C4ba71b36.erc");
}

TEST(PlayerCreature, AnEmptyFileNamesNoMind)
{
	EXPECT_FALSE(player_creature::ProfileMindPath("", "Scripts").has_value());
}

TEST(PlayerCreature, TheDefaultSettingIsTheFirstProfilesCreature)
{
	EXPECT_EQ(EngineConfig {}.profileCreatureFile, "C4ba71b36.erc");
}

TEST_F(ProfileCreatureTest, TheProfileHasACreatureWhenItsFileExists)
{
	WriteMind("Cacab45d4.erc");
	Locator::config::value().profileCreatureFile = "Cacab45d4.erc";
	EXPECT_TRUE(player_creature::ProfileHasCreature());
}

TEST_F(ProfileCreatureTest, AMissingFileIsNoCreature)
{
	Locator::config::value().profileCreatureFile = "C551091b1.erc";
	EXPECT_FALSE(player_creature::ProfileHasCreature());
}

TEST_F(ProfileCreatureTest, AnEmptySettingIsNoCreature)
{
	WriteMind("C4ba71b36.erc");
	Locator::config::value().profileCreatureFile.clear();
	EXPECT_FALSE(player_creature::ProfileHasCreature());
}

TEST_F(PlayerCreatureNativesTest, CallPlayerCreatureGivesTheCreatureThePlayerLeads)
{
	EXPECT_FALSE(player_creature::PlayersCreature(leash, PlayerNames::PLAYER_ONE).has_value());
	leash.playersCreature = creature;
	EXPECT_EQ(player_creature::PlayersCreature(leash, PlayerNames::PLAYER_ONE), creature);
}

TEST(PlayerCreature, AHomeIsKeptInTheFixedPointOnTheGround)
{
	const auto home = player_creature::HomeOnGround(glm::vec3(1896.5f, 29.48f, 2520.06f), 7.0f);
	EXPECT_EQ(home.x, map_coords::ToMetres(map_coords::ToFixed(1896.5f)));
	EXPECT_EQ(home.z, map_coords::ToMetres(map_coords::ToFixed(2520.06f)));
	EXPECT_EQ(home.y, 7.0f);
	// the fixed point truncates towards 0, so less than one of its units is no distance at all
	EXPECT_EQ(player_creature::HomeOnGround(glm::vec3(0.0001f, 0.0f, -0.0001f), 0.0f), glm::vec3(0.0f));
}

TEST_F(PlayerCreatureNativesTest, SetCreatureHomeHousesOnlyACreature)
{
	player_creature::SetHome(leash, Reg(), other, glm::vec3(1.0f));
	player_creature::SetHome(leash, Reg(), entt::null, glm::vec3(1.0f));
	EXPECT_TRUE(leash.homes.empty());
	player_creature::SetHome(leash, Reg(), creature, glm::vec3(1.0f, 2.0f, 3.0f));
	ASSERT_EQ(leash.homes.size(), 1u);
	EXPECT_EQ(leash.homes[0].first, creature);
	EXPECT_EQ(leash.homes[0].second, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(PlayerCreatureNativesTest, TheLeashSystemKeepsTheHomeItIsGiven)
{
	systems::LeashSystem system;
	system.SetHome(other, glm::vec3(5.0f));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<CreatureLeash>(other));
	system.SetHome(creature, glm::vec3(1896.5f, 30.0f, 2520.0f));
	EXPECT_EQ(Reg().Get<CreatureLeash>(creature).home, glm::vec3(1896.5f, 30.0f, 2520.0f));
}

TEST_F(PlayerCreatureNativesTest, SetCreatureDevStageMovesACreatureToAStage)
{
	player_creature::SetDevelopmentStage(Reg(), creature, 0);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 0u);
	player_creature::SetDevelopmentStage(Reg(), creature, player_creature::k_LastDevelopmentStage);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 13u);
}

TEST_F(PlayerCreatureNativesTest, SetCreatureDevStageLeavesAStageOutOfRangeAndOtherThingsAlone)
{
	player_creature::SetDevelopmentStage(Reg(), creature, 4);
	player_creature::SetDevelopmentStage(Reg(), creature, 14);
	player_creature::SetDevelopmentStage(Reg(), creature, -1);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 4u);
	player_creature::SetDevelopmentStage(Reg(), other, 2);
	EXPECT_FALSE(std::as_const(Reg()).AllOf<CreatureMindState>(other));
}

TEST_F(PlayerCreatureNativesTest, DevFunctionTwoTeachesTheRopeLeash)
{
	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, 2, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(leash.known.size(), 1u);
	EXPECT_EQ(leash.known[0], std::make_pair(creature, LeashType::Rope));
	EXPECT_TRUE(leash.leashable.empty());
}

TEST_F(PlayerCreatureNativesTest, DevFunctionThreeTeachesTheOtherLeashesAndHandsTheCreatureOver)
{
	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, 3, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(leash.known.size(), 2u);
	EXPECT_EQ(leash.known[0], std::make_pair(creature, LeashType::Evil));
	EXPECT_EQ(leash.known[1], std::make_pair(creature, LeashType::Good));
	ASSERT_EQ(leash.leashable.size(), 1u);
	EXPECT_EQ(leash.leashable[0], std::make_pair(creature, true));
}

TEST_F(PlayerCreatureNativesTest, DevFunctionWithoutACreatureDoesNothing)
{
	EXPECT_TRUE(player_creature::DevFunction(leash, 2, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(player_creature::DevFunction(leash, 3, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(leash.known.empty());
	EXPECT_TRUE(leash.leashable.empty());
}

TEST_F(PlayerCreatureNativesTest, OtherDevFunctionsAreNotPorted)
{
	leash.playersCreature = creature;
	for (const int32_t function : {0, 1, 4, 5, 12, 13, -1})
	{
		EXPECT_FALSE(player_creature::DevFunction(leash, function, PlayerNames::PLAYER_ONE)) << function;
	}
	EXPECT_TRUE(leash.known.empty());
	EXPECT_TRUE(leash.leashable.empty());
}

TEST_F(PlayerCreatureNativesTest, CreatureInDevScriptMarksOnlyACreature)
{
	EXPECT_FALSE(Reg().Get<Creature>(creature).inDevScript);
	player_creature::SetInDevScript(Reg(), creature, true);
	EXPECT_TRUE(Reg().Get<Creature>(creature).inDevScript);
	player_creature::SetInDevScript(Reg(), creature, false);
	EXPECT_FALSE(Reg().Get<Creature>(creature).inDevScript);
	player_creature::SetInDevScript(Reg(), other, true);
	EXPECT_FALSE(std::as_const(Reg()).AllOf<Creature>(other));
}

namespace
{
/// A mind file read as the cache keeps it, of a species row, with a size and an alignment
creature::CreatureMind Mind(uint32_t row)
{
	creature::CreatureMind mind;
	mind.result = creaturemind::MindResult::Success;
	mind.data.speciesRow = row;
	mind.data.alignment = 0.25f;
	mind.data.physique.strength = 0.75f;
	mind.data.physique.size = 2.0f;
	return mind;
}

uint64_t HashOf(const ecs::Registry& registry)
{
	state_hash::Hasher h;
	player_creature::HashCreatures(h, registry);
	return h.Value();
}
} // namespace

TEST(PlayerCreature, ALoadedMindGivesItsSpeciesAndBody)
{
	const auto mind = Mind(14);
	const auto plan = player_creature::PlanLoad(false, &mind, glm::vec2(1850.0f, 1300.0f));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->species, CreatureType::Mandrill);
	EXPECT_EQ(plan->size, 2.0f);
	EXPECT_EQ(plan->alignment, 0.25f);
	EXPECT_EQ(plan->strength, 0.75f);
}

TEST(PlayerCreature, ACreatureIsLoadedInTheMiddleOfThePointsCell)
{
	const auto mind = Mind(2);
	for (const auto point : {glm::vec2(1850.0f, 1300.0f), glm::vec2(1859.9f, 1309.99f), glm::vec2(1850.0001f, 1300.5f)})
	{
		const auto plan = player_creature::PlanLoad(false, &mind, point);
		ASSERT_TRUE(plan.has_value());
		EXPECT_EQ(plan->position, glm::vec3(1855.0f, 0.0f, 1305.0f)) << point.x << " " << point.y;
	}
}

TEST(PlayerCreature, NoCreatureIsLoadedWhenThePlayerHasOneOrTheFileGivesNone)
{
	const auto mind = Mind(14);
	EXPECT_FALSE(player_creature::PlanLoad(true, &mind, glm::vec2(0.0f)).has_value());
	EXPECT_FALSE(player_creature::PlanLoad(false, nullptr, glm::vec2(0.0f)).has_value());
	auto unread = Mind(14);
	unread.result = creaturemind::MindResult::ErrCantOpen;
	EXPECT_FALSE(player_creature::PlanLoad(false, &unread, glm::vec2(0.0f)).has_value());
	const auto noSpecies = Mind(17);
	EXPECT_FALSE(player_creature::PlanLoad(false, &noSpecies, glm::vec2(0.0f)).has_value());
}

TEST_F(PlayerCreatureNativesTest, TheHashPartFollowsEachCreaturesFields)
{
	const auto before = HashOf(Reg());
	Reg().Get<Creature>(creature).inDevScript = true;
	const auto inDevScript = HashOf(Reg());
	EXPECT_NE(inDevScript, before);
	Reg().Get<CreatureMindState>(creature).developmentPhase = 3;
	const auto staged = HashOf(Reg());
	EXPECT_NE(staged, inDevScript);
	Reg().Get<Creature>(creature).size = 1.5f;
	EXPECT_NE(HashOf(Reg()), staged);
}

TEST(PlayerCreature, TheHashPartIsEmptyWithoutACreature)
{
	const ecs::Registry registry;
	EXPECT_EQ(HashOf(registry), state_hash::Hasher {}.Value());
}

TEST(PlayerCreature, TheGameFoldersFirstProfileCreatureIsAMandrill)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		game = std::getenv("OPENBLACK_TEST_GAME_PATH");
	}
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	if (!spdlog::get("game"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::ScopedDefaultFileSystem fileSystem;
	Locator::filesystem::value().SetGamePath(game);
	const auto path =
	    Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true) / std::string(k_DefaultProfileCreatureFile);
	const auto mind = resources::CreatureMindLoader {}(resources::CreatureMindLoader::FromDiskTag {}, path);
	ASSERT_TRUE(mind->Loaded());
	const auto plan = player_creature::PlanLoad(false, mind.get(), glm::vec2(1850.0f, 1300.0f));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->species, CreatureType::Mandrill);
	EXPECT_TRUE(plan->size.has_value());
}

namespace
{
/// A temple mesh's special points, the pen point at its place and the others elsewhere
std::vector<glm::mat4> SpecialPoints(glm::vec3 pen)
{
	std::vector<glm::mat4> points(player_creature::k_TemplePenPoint + 1, glm::mat4(1.0f));
	points.back()[3] = glm::vec4(pen, 1.0f);
	return points;
}
} // namespace

TEST(PlayerCreature, ATemplesPenPointTurnsAndMovesWithIt)
{
	const components::Transform temple {
	    .position = glm::vec3(100.0f, 5.0f, 200.0f),
	    .rotation = glm::mat3(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f),
	    .scale = glm::vec3(2.0f),
	};
	const auto pen = player_creature::TemplePenPoint(temple, SpecialPoints(glm::vec3(1.0f, 0.0f, 3.0f)));
	ASSERT_TRUE(pen.has_value());
	EXPECT_EQ(*pen, temple.position + temple.rotation * glm::vec3(2.0f, 0.0f, 6.0f));
}

TEST(PlayerCreature, AMeshWithoutThePenPointKeepsNoCreature)
{
	const components::Transform temple {.position = glm::vec3(0.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
	auto points = SpecialPoints(glm::vec3(1.0f));
	points.pop_back();
	EXPECT_FALSE(player_creature::TemplePenPoint(temple, points).has_value());
	EXPECT_FALSE(player_creature::TemplePenPoint(temple, {}).has_value());
}

TEST_F(PlayerCreatureNativesTest, TheHomeFollowsTheTemplesPenOnTheGround)
{
	const auto penOf = [](PlayerNames owner) {
		return owner == PlayerNames::PLAYER_ONE ? std::optional(glm::vec3(1895.97f, 40.0f, 2520.75f)) : std::nullopt;
	};
	player_creature::FollowTemplePens(leash, Reg(), penOf, [](glm::vec2) { return 29.5f; });
	ASSERT_EQ(leash.homes.size(), 1u);
	EXPECT_EQ(leash.homes[0].first, creature);
	EXPECT_EQ(leash.homes[0].second, player_creature::HomeOnGround(glm::vec3(1895.97f, 0.0f, 2520.75f), 29.5f));
}

TEST_F(PlayerCreatureNativesTest, WithoutATempleTheHomeIsLeftAsItWas)
{
	player_creature::FollowTemplePens(
	    leash, Reg(), [](PlayerNames) { return std::optional<glm::vec3> {}; }, [](glm::vec2) { return 0.0f; });
	EXPECT_TRUE(leash.homes.empty());
}

TEST(PlayerCreature, WithoutACreatureNoTempleIsLookedAt)
{
	const ecs::Registry registry;
	RecordingLeash leash;
	bool asked = false;
	player_creature::FollowTemplePens(
	    leash, registry,
	    [&asked](PlayerNames) {
		    asked = true;
		    return std::optional(glm::vec3(0.0f));
	    },
	    [](glm::vec2) { return 0.0f; });
	EXPECT_FALSE(asked);
	EXPECT_TRUE(leash.homes.empty());
}

TEST(PlayerCreature, Land1sHomeIsBetweenThePensWalls)
{
	// the temple of Land 1 and the script's home for the creature
	const glm::vec2 temple(1915.05f, 2508.89f);
	EXPECT_TRUE(player_creature::BetweenPenWalls(temple, 36.0f, glm::vec2(1896.5f, 2520.06f)));
	EXPECT_TRUE(player_creature::BetweenPenWalls(temple, 36.0f, glm::vec2(1895.97f, 2520.75f)));
	// the temple's other side
	EXPECT_FALSE(player_creature::BetweenPenWalls(temple, 36.0f, glm::vec2(1934.0f, 2497.0f)));
}

TEST(PlayerCreature, InThePenACreatureIsDrawnDownToANewbornsSize)
{
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 0.87f, true), 0.22f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 14.0f, true), 0.22f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 15.0f, true), 0.22f + (2.0f - 0.22f) * 0.5f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 16.0f, true), 2.0f);
}

TEST(PlayerCreature, OutsideThePenACreatureIsDrawnAtItsOwnSize)
{
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 16.5f, true), 2.0f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 1.0f, false), 2.0f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, std::numeric_limits<float>::quiet_NaN(), true), 2.0f);
}
