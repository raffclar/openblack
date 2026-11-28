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
namespace script_import = openblack::ecs::vanilla_script_import;
namespace reaction_import = openblack::ecs::vanilla_reaction_import;

// The import marks a villager as a dancer when a group of a dance of a creature or of a script has it and its group is
// that group
TEST(VanillaWorldImport, TheImportFindsTheDancersOfADanceOfACreatureOrAScript)
{
	auto fake = GameAtTurn(100);
	// 2: the dance, 3: its group, 4 and 5: villagers, 6: a dance of a reaction, 7: its group, 8: the reaction, 9: a
	// villager of the reaction
	fake.Record(VanillaType::Dance)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::DanceAroundPerson))
	    .Refs("danceGroups", {3});
	fake.Record(VanillaType::DanceGroup).Refs("groupDancers", {4});
	for (const uint32_t group : {3U, 0U})
	{
		fake.Record(VanillaType::Villager)
		    .Position(1, 2)
		    .Value("info", VillagerRowNumber(VillagerInfo::NorseForesterMale))
		    .Value("action", std::array<uint8_t, 6> {})
		    .Ref("danceGroup", group);
	}
	fake.Record(VanillaType::Dance)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::BuildingSmall))
	    .Refs("danceGroups", {7});
	fake.Record(VanillaType::DanceGroup).Refs("groupDancers", {9});
	fake.Record(VanillaType::Reaction).Ref("", 0).Ref("reactionDance", 6);
	fake.Record(VanillaType::Villager)
	    .Position(1, 2)
	    .Value("info", VillagerRowNumber(VillagerInfo::NorseForesterMale))
	    .Value("action", std::array<uint8_t, 6> {})
	    .Ref("reaction", 8)
	    .Ref("danceGroup", 7);
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.villagers.size(), 3U);
	EXPECT_TRUE(imported.villagers[0].importedDancer);
	EXPECT_FALSE(imported.villagers[1].importedDancer);
	// A dancer of the dance of a reaction is a dancer too
	EXPECT_TRUE(imported.villagers[2].importedDancer);
	EXPECT_EQ(imported.villagers[0].reaction, 0U);
	EXPECT_EQ(imported.villagers[2].reaction, 8U);
	ASSERT_EQ(imported.dances.made.size(), 2U);
	EXPECT_EQ(imported.dances.made[0].number, 2U);
	EXPECT_EQ(imported.dances.made[1].number, 6U);
}

// A villager in a state of the dance of a creature keeps it only as a dancer of a dance that the import makes. A
// villager that dances as it reacts keeps its state only as a dancer that reacts to a reaction of the file.
TEST(VanillaWorldImport, TheStatesOfTheDanceOfACreatureNeedADanceOfTheImport)
{
	const auto decide = world_import::DecideAgain();
	for (const auto state : {VillagerStates::WorshippingCreature, VillagerStates::RestartWorshippingCreature,
	                         VillagerStates::ControlledByCreature})
	{
		const auto value = static_cast<uint8_t>(state);
		world_import::ImportedVillager villager {.states = {value, value, 0}, .moveState = 0x01};
		EXPECT_EQ(world_import::StartStates(villager), decide) << static_cast<int>(value);
		villager.importedDancer = true;
		EXPECT_EQ(world_import::StartStates(villager), villager.states) << static_cast<int>(value);
	}
	constexpr auto k_Reacting = static_cast<uint8_t>(VillagerStates::DanceWhileReacting);
	world_import::ImportedVillager reacting {.states = {k_Reacting, k_Reacting, 0}, .moveState = 0x01};
	EXPECT_EQ(world_import::StartStates(reacting), decide);
	reacting.importedDancer = true;
	EXPECT_EQ(world_import::StartStates(reacting), decide);
	reacting.reaction = 8;
	EXPECT_EQ(world_import::StartStates(reacting), reacting.states);
	reacting.importedDancer = false;
	EXPECT_EQ(world_import::StartStates(reacting), decide);
}

// Each dance of the saves is the dance of a worship site or a dance that the import makes on its own. A dance that the
// import makes is not the dance of a script unless the camera word is set.
TEST(VanillaWorldImport, SavedGamesTellWhichDancesTheImportMakes)
{
	size_t all = 0;
	size_t ofSites = 0;
	size_t made = 0;
	const auto files = ForEachSave([&all, &ofSites, &made](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadTowns(world);
		const auto dances = world.OfType(VanillaType::Dance);
		all += dances.size();
		ofSites += static_cast<size_t>(std::ranges::count_if(
		    imported.worshipSites, [](const world_import::ImportedWorshipSite& site) { return site.dance != 0; }));
		made += imported.dances.made.size();
		for (const auto& dance : imported.dances.made)
		{
			const auto* record = world.Record(dance.number);
			ASSERT_NE(record, nullptr) << path << " " << dance.number;
			EXPECT_EQ(dance.madeByScript, world.Word(*record, "danceCameraOff").value_or(0) != 0);
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_EQ(all, ofSites + made);
	std::cout << "Dances " << all << ", of worship sites " << ofSites << ", made on their own " << made << '\n' << std::flush;
}

// Each group of a dance in the saves belongs to a thing that keeps groups of a dance, and its dancers are living things
// whose group is that group
TEST(VanillaWorldImport, SavedGamesHaveGroupsOfDancesWithTheirDancers)
{
	std::map<uint32_t, size_t> owners;
	size_t groups = 0;
	size_t dancers = 0;
	const auto files = ForEachSave([&owners, &groups, &dancers](const std::filesystem::path& path, const VanillaWorld& world) {
		for (const auto* group : world.OfType(VanillaType::DanceGroup))
		{
			++groups;
			const auto dance = world.Ref(*group, "groupDance").value_or(0);
			EXPECT_EQ(dance, world.Ref(*group, "groupBehaviour").value_or(0)) << path << " " << group->number;
			const auto* owner = dance != 0 ? world.Record(dance) : nullptr;
			ASSERT_NE(owner, nullptr) << path << " " << group->number;
			++owners[owner->type];
			EXPECT_NE(std::ranges::find(world.Refs(*owner, "danceGroups"), group->number),
			          world.Refs(*owner, "danceGroups").end())
			    << path << " " << group->number;
			for (const auto number : world.Refs(*group, "groupDancers"))
			{
				++dancers;
				const auto* dancer = world.Record(number);
				ASSERT_NE(dancer, nullptr) << path << " " << number;
				EXPECT_EQ(world.Ref(*dancer, "danceGroup").value_or(0), group->number) << path << " " << number;
			}
		}
		// The number of each group in its dance is its place in the list of all groups, so the parts of a group and the
		// actions of the key frames name groups of the dance. Each group is in the list of the limited groups or of the
		// shared groups.
		for (const auto* record : world.OfType(VanillaType::Dance))
		{
			const auto dance = world_import::ReadDance(world, *record);
			ASSERT_TRUE(dance.has_value()) << path << " " << record->number;
			for (size_t place = 0; place < dance->groups.size(); ++place)
			{
				const auto& group = dance->groups[place];
				EXPECT_EQ(group.id, place) << path << " " << group.number;
				EXPECT_NE(std::ranges::find(dance->limitedGroups, place) != dance->limitedGroups.end(),
				          std::ranges::find(dance->sharedGroups, place) != dance->sharedGroups.end())
				    << path << " " << group.number;
				EXPECT_EQ(group.limited, std::ranges::find(dance->limitedGroups, place) != dance->limitedGroups.end());
				for (const auto child : group.children)
				{
					EXPECT_LT(child, dance->groups.size()) << path << " " << group.number;
				}
			}
			for (const auto& keyFrame : dance->keyFrames)
			{
				for (const auto& action : keyFrame.actions)
				{
					for (const auto group : action.groups)
					{
						EXPECT_LT(group, dance->groups.size()) << path << " " << record->number;
					}
				}
			}
			std::cout << path << " dance " << record->number << " row " << static_cast<int>(dance->type) << ": "
			          << dance->groups.size() << " groups, " << dance->limitedGroups.size() << " limited, "
			          << dance->keyFrames.size() << " key frames, round " << static_cast<int>(dance->round) << " of "
			          << dance->roundLength << "\n";
		}
		// The dance of each worship site comes with the site
		for (const auto& site : world_import::ReadTowns(world).worshipSites)
		{
			EXPECT_EQ(site.danceValues.has_value(), site.dance != 0) << path << " " << site.number;
			std::cout << path << " worship site " << site.number << " dance " << site.dance << " with "
			          << (site.danceValues.has_value() ? site.danceValues->groups.size() : 0U) << " groups\n";
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << groups << " groups of dances with " << dancers << " dancers; owners:";
	for (const auto& [type, count] : owners)
	{
		std::cout << " type " << type << " x" << count;
	}
	std::cout << "\n";
}

// A breeder disciple keeps its lover and the place where it draws villagers to it
TEST(VanillaWorldImport, ABreederKeepsItsLoverAndItsPlace)
{
	constexpr auto k_Breeder = static_cast<uint8_t>(VillagerStates::BreederDisciple);
	EXPECT_TRUE(world_import::CanKeepStateValues(k_Breeder));
	const std::vector values = {ObjectValue(42), BytesValue({0x00, 0x00, 0x01, 0x00, 0x00, 0x80, 0x02, 0x00})};
	const auto breeder = world_import::BreederValuesOf(k_Breeder, values);
	ASSERT_TRUE(breeder.has_value());
	EXPECT_EQ(breeder->lover, 42U);
	EXPECT_EQ(breeder->x, 0x10000);
	EXPECT_EQ(breeder->z, 0x28000);
	// No lover is the object 0
	EXPECT_EQ(world_import::BreederValuesOf(k_Breeder, {ObjectValue(0), values[1]})->lover, 0U);
	// Another state, or values of another size, give nothing
	EXPECT_FALSE(world_import::BreederValuesOf(static_cast<uint8_t>(VillagerStates::Dead), values).has_value());
	EXPECT_FALSE(world_import::BreederValuesOf(k_Breeder, {ObjectValue(42), BytesValue({0x01})}).has_value());
}

// In the saves of the construct land, every villager keeps its states: breeders, and the worship site keeps its dance
TEST(VanillaWorldImport, SavedGamesOfTheConstructLandHaveNoVillagerThatDecidesAgain)
{
	size_t villagers = 0;
	size_t breeders = 0;
	const auto files = ForEachSave([&villagers, &breeders](const std::filesystem::path& path, const VanillaWorld& world) {
		if (world.Framing().landscape.find("construct.lnd") == std::string::npos)
		{
			return;
		}
		for (const auto& villager : world_import::ReadTowns(world).villagers)
		{
			++villagers;
			EXPECT_EQ(world_import::StartStates(villager), villager.states)
			    << path << " " << villager.number << " " << static_cast<int>(villager.states[0]) << " "
			    << static_cast<int>(villager.states[1]);
			const auto top = world_import::BreederValuesOf(villager.states[0], villager.topValues);
			const auto next = world_import::BreederValuesOf(villager.states[1], villager.finalValues);
			if (top.has_value() || next.has_value())
			{
				++breeders;
				const auto& values = top.has_value() ? *top : *next;
				const auto* lover = values.lover != 0 ? world.Record(values.lover) : nullptr;
				EXPECT_TRUE(values.lover == 0 || (lover != nullptr && lover->Is(VanillaType::Villager)))
				    << path << " " << villager.number;
				std::cout << path << " breeder " << villager.number << " lover " << values.lover << " place " << values.x
				          << ", " << values.z << "\n";
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << villagers << " villagers of the construct land, " << breeders << " breeders\n";
}

namespace
{

/// A game object, a human player with no computer god, and a computer player whose god has two nodes and two attitudes
VanillaWorld WorldWithComputerGod()
{
	auto fake = GameAtTurn(100);
	// Record 2: the human player, with no computer god
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 0).Value<uint32_t>("type", 1).Ref("computer", 0);
	// Record 3: the computer player, whose god is record 4
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1).Value<uint32_t>("type", 2).Ref("computer", 4);
	fake.Record(VanillaType::ComputerPlayer)
	    .Value("position", MapCoords {.x = 0x10000, .z = 0x20000, .altitude = 3.0f})
	    .Refs("casts", {})
	    .Refs("seen", {})
	    .Refs("queue", {})
	    .Value("handSpeed", 45.0f)
	    .Value<uint32_t>("started", 1)
	    .Words("castRequests", {0, 7, 2})
	    .Value<uint32_t>("switchedOn", 1)
	    .Value<uint32_t>("paused", 0)
	    .Value<uint32_t>("scriptHold", 1)
	    .Refs("attitudes", {5, 6})
	    .Value("tree", 0.5f)
	    .Value("tree", 2.0f)
	    .Value("tree", 0.25f)
	    .Value("tree", 3.0f);
	fake.Record(VanillaType::ComputerAttitude).Value("attitude", 4.0f);
	fake.Record(VanillaType::ComputerAttitude).Value("attitude", -1.0f);
	return fake.Make();
}

} // namespace

TEST(VanillaPlayerImport, EachPlayerOfTheFileHasTheComputerGodOfTheFile)
{
	const auto players = ecs::vanilla_player_import::ReadPlayers(WorldWithComputerGod());
	ASSERT_EQ(players.size(), 2U);
	EXPECT_EQ(players[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(players[0].computerType);
	EXPECT_FALSE(players[0].computer.has_value());
	EXPECT_EQ(players[1].player, PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(players[1].computerType);
	ASSERT_TRUE(players[1].computer.has_value());
	const auto& god = *players[1].computer;
	EXPECT_TRUE(god.switchedOn);
	EXPECT_FALSE(god.paused);
	EXPECT_TRUE(god.scriptHold);
	EXPECT_TRUE(god.started);
	EXPECT_EQ(god.handSpeed, 45.0f);
	EXPECT_EQ(god.hand.x, 0x10000);
	EXPECT_EQ(god.hand.z, 0x20000);
	EXPECT_EQ(god.castRequests[1], 7U);
	EXPECT_EQ(god.castRequests[2], 2U);
	EXPECT_EQ(god.castRequests[3], 0U);
	EXPECT_EQ(god.nodes, (std::vector<std::pair<float, float>> {{0.5f, 2.0f}, {0.25f, 3.0f}}));
	EXPECT_EQ(god.attitudes, (std::vector<float> {4.0f, -1.0f}));
}

TEST(VanillaPlayerImport, TheComputerGodOfTheFileStartsAgainAsAfterALoad)
{
	const auto players = ecs::vanilla_player_import::ReadPlayers(WorldWithComputerGod());
	ASSERT_EQ(players.size(), 2U);
	const auto computer = ecs::vanilla_player_import::ComputerOf(players[1], *players[1].computer, 10.0f);
	EXPECT_TRUE(computer.switchedOn);
	EXPECT_TRUE(computer.marked);
	EXPECT_TRUE(computer.started);
	EXPECT_FALSE(computer.creatureLike.has_value());
	EXPECT_EQ(computer.speed, 45.0f);
	EXPECT_EQ(computer.hand.speed, 45.0f);
	EXPECT_FALSE(computer.hand.locked);
	EXPECT_TRUE(computer.hand.steps.empty());
	EXPECT_EQ(computer.hand.coords.x, 0x10000);
	EXPECT_EQ(computer.hand.coords.z, 0x20000);
	EXPECT_EQ(computer.hand.hand, glm::vec3(map_coords::ToMetres(0x10000), 13.0f, map_coords::ToMetres(0x20000)));
	EXPECT_EQ(computer.hand.camera, computer.hand.hand);
	// The tree is a new tree with the suppression and the weight of the file in the first nodes
	const auto fresh = computer_mind::MakeMind();
	ASSERT_EQ(computer.mind.nodes.size(), fresh.nodes.size());
	EXPECT_EQ(computer.mind.nodes[0].suppression, 0.5f);
	EXPECT_EQ(computer.mind.nodes[0].weight, 2.0f);
	EXPECT_EQ(computer.mind.nodes[1].suppression, 0.25f);
	EXPECT_EQ(computer.mind.nodes[1].weight, 3.0f);
	EXPECT_EQ(computer.mind.nodes[2].weight, fresh.nodes[2].weight);
	EXPECT_FALSE(computer.mind.paused);
	EXPECT_TRUE(computer.mind.scriptHold);
	EXPECT_TRUE(computer.mind.wasBusy);
	EXPECT_EQ(computer.mind.castRequested[1], 7U);
	EXPECT_TRUE(computer.casts.empty());
	EXPECT_TRUE(computer.seen.empty());
	EXPECT_TRUE(computer.mind.queue.empty());
	// A god that played is not unused, and a new god is unused
	EXPECT_FALSE(ecs::vanilla_player_import::IsUnusedGod(players[1], *players[1].computer));
	ecs::vanilla_player_import::ImportedComputerGod unused;
	for (const auto& node : fresh.nodes)
	{
		unused.nodes.emplace_back(node.suppression, node.weight);
	}
	EXPECT_TRUE(ecs::vanilla_player_import::IsUnusedGod(players[0], unused));
	unused.castRequests[3] = 1;
	EXPECT_FALSE(ecs::vanilla_player_import::IsUnusedGod(players[0], unused));
	// A god of a player that is not of the computer type keeps no mark
	EXPECT_FALSE(ecs::vanilla_player_import::ComputerOf(players[0], *players[1].computer, 0.0f).marked);
}

TEST(VanillaPlayerImport, TheMindTakesTheAttitudesOfTheFile)
{
	const auto players = ecs::vanilla_player_import::ReadPlayers(WorldWithComputerGod());
	ASSERT_EQ(players.size(), 2U);
	ecs::components::ComputerAttitudes attitudes;
	ecs::vanilla_player_import::PutAttitudes(attitudes, *players[1].computer);
	EXPECT_EQ(attitudes.kept[0], 4.0f);
	EXPECT_EQ(attitudes.kept[1], -1.0f);
	EXPECT_EQ(attitudes.kept[2], ecs::components::ComputerAttitudes::k_StartValue);
}

// The game's load keeps the order of the dance list of the file, the newest first. The dances join again from the oldest.
TEST(VanillaWorldImport, TheDancesJoinTheListInTheOrderOfTheFile)
{
	const auto first = static_cast<entt::entity>(10);
	const auto second = static_cast<entt::entity>(11);
	const auto third = static_cast<entt::entity>(12);
	const std::unordered_map<uint32_t, entt::entity> made {{5, first}, {6, second}, {7, third}, {8, entt::null}};
	const std::vector<entt::entity> dances {first, second, third};
	// The file lists 7, then 5, then 6: 6 is the oldest
	EXPECT_EQ(world_import::DancesToJoin(std::vector<uint32_t> {7, 5, 6}, made, dances),
	          (std::vector<entt::entity> {second, first, third}));
	// A number that the import did not make as a dance is not in the order
	EXPECT_EQ(world_import::DancesToJoin(std::vector<uint32_t> {9, 8, 5}, made, dances), (std::vector<entt::entity> {first}));
	// A dance that stopped is not in the order
	EXPECT_EQ(world_import::DancesToJoin(std::vector<uint32_t> {5, 6}, made, std::vector<entt::entity> {second}),
	          (std::vector<entt::entity> {second}));
}

// The saves give each player and its computer god, and the dance list of each save has each dance of the save
TEST(VanillaPlayerImport, SavedGamesGiveEachPlayerItsComputerGod)
{
	const auto files = ForEachSave([](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto players = ecs::vanilla_player_import::ReadPlayers(world);
		EXPECT_EQ(players.size(), world.OfType(VanillaType::Player).size()) << path;
		for (const auto& player : players)
		{
			std::cout << path.parent_path().filename() << "/" << path.filename() << " player "
			          << static_cast<int>(player.player) << " computer type " << player.computerType;
			if (player.computer.has_value())
			{
				const auto& god = *player.computer;
				EXPECT_EQ(god.nodes.size(), k_ComputerPlayerTreeWords / 2) << path;
				std::cout << " unused " << ecs::vanilla_player_import::IsUnusedGod(player, god);
				std::cout << " god: on " << god.switchedOn << " paused " << god.paused << " hold " << god.scriptHold
				          << " started " << god.started << " speed " << god.handSpeed << " attitudes " << god.attitudes.size()
				          << " lists " << god.casts << "/" << god.seen << "/" << god.queue;
			}
			std::cout << "\n";
		}
		const auto imported = world_import::ReadTowns(world);
		EXPECT_EQ(imported.dances.gameList.size(), world.OfType(VanillaType::Dance).size()) << path;
		std::cout << " dances in the list:";
		for (const auto number : imported.dances.gameList)
		{
			std::cout << " " << number;
		}
		std::cout << "\n";
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
}

// The artefacts of the towns come with their values. Each town takes its artefacts in the order of its list, the newest
// first, so the artefacts of a town come in the reverse order of its list.
TEST(VanillaWorldImport, TheArtefactsOfTheTownsComeWithTheirValuesInTheOrderOfTheirTown)
{
	auto fake = GameAtTurn(100);
	// 2: a player, 3: a town that lists 5 then 4, 4 and 5: its artefacts, 6: an artefact that no town lists
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1);
	fake.Record(VanillaType::Town).Refs("artifacts", {5, 4});
	fake.Record(VanillaType::TownArtifact)
	    .Ref("artefactObject", 20)
	    .Ref("artefactTown", 3)
	    .Ref("artefactPlayer", 2)
	    .Value("artefactValue", 1.5f)
	    .Ref("artefactDance", 30)
	    .Ref("artefactEffect", 0)
	    .Value<uint32_t>("artefactDancing", 1)
	    .Value<uint32_t>("artefactDanceStarted", 90);
	fake.Record(VanillaType::TownArtifact).Ref("artefactObject", 21).Ref("artefactTown", 3).Ref("artefactPlayer", 0);
	fake.Record(VanillaType::TownArtifact).Ref("artefactObject", 22).Ref("artefactTown", 3);
	const auto artefacts = world_import::ReadTowns(fake.Make()).artefacts;
	ASSERT_EQ(artefacts.size(), 3U);
	EXPECT_EQ(artefacts[0].number, 4U);
	EXPECT_EQ(artefacts[0].object, 20U);
	EXPECT_EQ(artefacts[0].town, 3U);
	EXPECT_EQ(artefacts[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(artefacts[0].value, 1.5f);
	EXPECT_EQ(artefacts[0].dance, 30U);
	EXPECT_TRUE(artefacts[0].dancing);
	EXPECT_EQ(artefacts[0].danceStarted, 90U);
	EXPECT_TRUE(artefacts[0].inTown);
	EXPECT_EQ(artefacts[1].number, 5U);
	EXPECT_FALSE(artefacts[1].player.has_value());
	EXPECT_TRUE(artefacts[1].inTown);
	// An artefact that its town does not list is not in its town
	EXPECT_EQ(artefacts[2].number, 6U);
	EXPECT_FALSE(artefacts[2].inTown);
}

// A dancer of the dance of an artefact keeps its state when the import makes its group, as the dancers of other dances
TEST(VanillaWorldImport, TheStatesOfTheDanceOfAnArtefactNeedADanceOfTheImport)
{
	world_import::ImportedVillager villager;
	for (const auto state : {VillagerStates::ArtifactDance, VillagerStates::WaitForArtifactDance})
	{
		villager.states = {static_cast<uint8_t>(state), static_cast<uint8_t>(VillagerStates::DecideWhatToDo), 0};
		villager.importedDancer = true;
		EXPECT_EQ(world_import::StartStates(villager), villager.states);
		villager.importedDancer = false;
		EXPECT_NE(world_import::StartStates(villager), villager.states);
	}
}

// The reader names every value of a reaction. The reactions come the oldest first: the reverse of the game's list of the
// reactions, then the reactions that the list does not have, in the order of the file.
TEST(VanillaWorldImport, TheReactionsComeTheOldestFirstWithEveryValueOfTheFile)
{
	auto fake = GameAtTurn(500);
	for (size_t list = 0; list < world_import::k_GameObjectLists; ++list)
	{
		fake.Refs("lists",
		          list == reaction_import::k_GameReactionList ? std::vector<uint32_t> {3, 2} : std::vector<uint32_t> {});
	}
	// 2 and 3: reactions of the list, 4: a reaction that the list does not have, 5: the player
	fake.Record(VanillaType::Reaction)
	    .Ref("reactionTarget", 40)
	    .Refs("reactionTakers", {41, 42})
	    .Value<uint32_t>("", 0)
	    .Value<uint32_t>("reactionType", static_cast<uint32_t>(Reaction::ReactToTownCelebration))
	    .Ref("reactionDance", 43)
	    .Value<uint32_t>("reactionFirstTaken", 450)
	    .Value<uint32_t>("reactionSpreads", 0)
	    .Ref("reactionPlayer", 5)
	    .Value<float>("reactionReach", 35.5f);
	fake.Record(VanillaType::Reaction)
	    .Ref("reactionTarget", 44)
	    .Refs("reactionTakers", {})
	    .Value<uint32_t>("reactionType", static_cast<uint32_t>(Reaction::LookAtObject))
	    .Ref("reactionDance", 0)
	    .Value<uint32_t>("reactionFirstTaken", 0)
	    .Value<uint32_t>("reactionSpreads", 1)
	    .Ref("reactionPlayer", 0)
	    .Value<float>("reactionReach", 1.0f);
	fake.Record(VanillaType::Reaction).Ref("reactionTarget", 45);
	fake.Record(VanillaType::Player).Value<uint8_t>("number", 1);
	const auto reactions = reaction_import::ReadReactions(fake.Make());
	ASSERT_EQ(reactions.size(), 3U);
	EXPECT_EQ(reactions[0].number, 2U);
	EXPECT_EQ(reactions[1].number, 3U);
	EXPECT_EQ(reactions[2].number, 4U);
	const auto& celebration = reactions[0];
	EXPECT_EQ(celebration.target, 40U);
	EXPECT_EQ(celebration.takers, (std::vector<uint32_t> {41, 42}));
	EXPECT_EQ(celebration.type, static_cast<uint32_t>(Reaction::ReactToTownCelebration));
	EXPECT_EQ(celebration.dance, 43U);
	EXPECT_EQ(celebration.firstTaken, std::optional<uint32_t>(450));
	EXPECT_FALSE(celebration.spreads);
	EXPECT_EQ(celebration.player, PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(celebration.reach, 35.5f);
	// No first taker and no player
	EXPECT_EQ(reactions[1].firstTaken, std::nullopt);
	EXPECT_EQ(reactions[1].player, std::nullopt);
	EXPECT_TRUE(reactions[1].spreads);
}

// A reaction of the file goes on from the thing that the import made, with the time, the reach and the spread of the
// file. A reaction of no player impresses no one.
TEST(VanillaWorldImport, AReactionOfTheFileGoesOnFromTheThingThatTheImportMade)
{
	constexpr auto k_Thing = static_cast<entt::entity>(9);
	const reaction_import::Initiator thing {.entity = k_Thing, .position = {10.0f, 2.0f, 30.0f}, .impressiveValue = 3.5f};
	const reaction_import::ImportedReaction file {.type = static_cast<uint32_t>(Reaction::ReactToNewBuilding),
	                                              .firstTaken = 940,
	                                              .spreads = false,
	                                              .player = PlayerNames::PLAYER_ONE,
	                                              .reach = 42.0f};
	const auto taken = reaction_import::ActiveOf(file, thing, 1000);
	EXPECT_EQ(taken.source.initiator, k_Thing);
	EXPECT_EQ(taken.source.type, Reaction::ReactToNewBuilding);
	EXPECT_EQ(taken.source.player, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(taken.source.playerless);
	EXPECT_EQ(taken.source.position, glm::vec3(10.0f, 2.0f, 30.0f));
	EXPECT_FLOAT_EQ(taken.source.impressiveValue, 3.5f);
	EXPECT_FLOAT_EQ(taken.source.power, 1.0f);
	EXPECT_FLOAT_EQ(taken.source.strength, 1.0f);
	EXPECT_EQ(taken.source.magicType, MagicType::None);
	EXPECT_TRUE(taken.source.casterCreature == entt::null);
	EXPECT_EQ(taken.source.reach, std::nullopt);
	EXPECT_FLOAT_EQ(taken.reach, 42.0f);
	EXPECT_FALSE(taken.spreads);
	// Its time started 60 turns before the turn of the file
	EXPECT_EQ(taken.age, 60U);
	EXPECT_EQ(taken.firstTaken, std::optional<uint32_t>(0));
	// No living thing took it up, and no player made it
	const auto waiting = reaction_import::ActiveOf({.type = 1, .reach = 5.0f}, thing, 1000);
	EXPECT_EQ(waiting.age, 0U);
	EXPECT_EQ(waiting.firstTaken, std::nullopt);
	EXPECT_EQ(waiting.source.player, PlayerNames::NEUTRAL);
	EXPECT_TRUE(waiting.source.playerless);
	EXPECT_TRUE(waiting.spreads);
}

// The reaction to a spell goes on with the strength, the magic type and the caster of the spell, and the balance of
// the miracles
TEST(VanillaWorldImport, AReactionToASpellGoesOnWithTheValuesOfTheSpell)
{
	constexpr auto k_Spell = static_cast<entt::entity>(9);
	constexpr auto k_Caster = static_cast<entt::entity>(4);
	const reaction_import::Initiator spell {.entity = k_Spell,
	                                        .position = {1.0f, 2.0f, 3.0f},
	                                        .impressiveValue = 2.0f,
	                                        .strength = 0.5f,
	                                        .magicType = MagicType::Water,
	                                        .casterCreature = k_Caster,
	                                        .balance = openblack::ecs::systems::ReactionSystemInterface::Balance::Miracles};
	const auto active = reaction_import::ActiveOf(
	    {.type = static_cast<uint32_t>(Reaction::LookAtSpell), .player = PlayerNames::PLAYER_ONE, .reach = 30.0f}, spell, 1000);
	EXPECT_EQ(active.source.initiator, k_Spell);
	EXPECT_FLOAT_EQ(active.source.strength, 0.5f);
	EXPECT_EQ(active.source.magicType, MagicType::Water);
	EXPECT_EQ(active.source.casterCreature, k_Caster);
	EXPECT_EQ(active.source.balance, openblack::ecs::systems::ReactionSystemInterface::Balance::Miracles);
	EXPECT_FLOAT_EQ(active.source.impressiveValue, 2.0f);
}

// A living thing remembers the kinds of reaction of the file, the oldest first. The game keeps only the newest three.
TEST(VanillaWorldImport, ALivingThingRemembersTheNewestThreeKindsOfReaction)
{
	auto fake = GameAtTurn(100);
	fake.Record(VanillaType::Villager);
	for (const auto& [type, turn] : std::array<std::pair<uint32_t, uint32_t>, 4> {{{7, 10}, {12, 20}, {1, 30}, {9, 40}}})
	{
		fake.Value<uint32_t>("reactionDoneType", type).Value<uint32_t>("reactionDoneTurn", turn);
	}
	fake.Record(VanillaType::Villager);
	const auto world = fake.Make();
	const auto memory = reaction_import::ReadMemory(world, world.Records()[1]);
	ASSERT_EQ(memory.size(), 3U);
	EXPECT_EQ(memory[0].type, 12U);
	EXPECT_EQ(memory[0].turn, 20U);
	EXPECT_EQ(memory[2].type, 9U);
	EXPECT_EQ(memory[2].turn, 40U);
	EXPECT_TRUE(reaction_import::ReadMemory(world, world.Records()[2]).empty());
}

// The saves have no reaction of a kind that the reaction table does not have, and the memory of each living thing has
// at most three kinds of reaction
TEST(VanillaWorldImport, SavedGamesHaveReactionsAndMemoriesThatTheImportReads)
{
	size_t reactions = 0;
	size_t memories = 0;
	const auto files = ForEachSave([&reactions, &memories](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto read = reaction_import::ReadReactions(world);
		for (const auto& reaction : read)
		{
			EXPECT_LT(reaction.type, reaction_import::k_ReactionKinds) << path << " " << reaction.number;
			const auto* target = world.Record(reaction.target);
			std::cout << path.filename() << " reaction " << reaction.number << " kind " << reaction.type << " target "
			          << reaction.target << " of type " << (target != nullptr ? target->type : 0) << " takers "
			          << reaction.takers.size() << " first taken " << reaction.firstTaken.value_or(0) << '\n'
			          << std::flush;
		}
		reactions += read.size();
		for (const auto& record : world.Records())
		{
			const auto memory = reaction_import::ReadMemory(world, record);
			EXPECT_EQ(memory.size(), world.Fields(record, "reactionDoneType").size()) << path << " " << record.number;
			memories += memory.empty() ? 0 : 1;
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << "Reactions " << reactions << ", living things that remember reactions " << memories << '\n' << std::flush;
}

// The arenas come the oldest first, with their places, their radii and their fights. An arena with no radius is left out.
TEST(VanillaWorldImport, TheArenasComeTheOldestFirstWithTheirFights)
{
	auto fake = GameAtTurn(500);
	for (size_t list = 0; list < world_import::k_GameObjectLists; ++list)
	{
		fake.Refs("lists", list == script_import::k_GameArenaList ? std::vector<uint32_t> {3, 2} : std::vector<uint32_t> {});
	}
	// 2 and 3: arenas of the list, 4: an arena that the list does not have, 5: an arena with no radius
	fake.Record(VanillaType::Arena)
	    .Position(1000, 2000)
	    .Value<float>("arenaOne", 1.0f)
	    .Value<float>("arenaRadius", 30.0f)
	    .Value<uint32_t>("arenaFightOn", 1)
	    .Refs("arenaCreatures", {40, 41})
	    .Ref("arenaNone", 0);
	fake.Record(VanillaType::Arena)
	    .Position(3000, 4000)
	    .Value<float>("arenaOne", 1.0f)
	    .Value<float>("arenaRadius", 45.5f)
	    .Value<uint32_t>("arenaFightOn", 0)
	    .Refs("arenaCreatures", {0, 0})
	    .Ref("arenaNone", 0);
	fake.Record(VanillaType::Arena)
	    .Position(5000, 6000)
	    .Value<float>("arenaOne", 1.0f)
	    .Value<float>("arenaRadius", 20.0f)
	    .Value<uint32_t>("arenaFightOn", 0)
	    .Refs("arenaCreatures", {0, 0})
	    .Ref("arenaNone", 0);
	fake.Record(VanillaType::Arena).Position(7000, 8000);
	const auto arenas = script_import::ReadArenas(fake.Make());
	ASSERT_EQ(arenas.size(), 3U);
	EXPECT_EQ(arenas[0].number, 2U);
	EXPECT_EQ(arenas[1].number, 3U);
	EXPECT_EQ(arenas[2].number, 4U);
	EXPECT_EQ(arenas[0].position.x, 1000);
	EXPECT_EQ(arenas[0].position.z, 2000);
	EXPECT_FLOAT_EQ(arenas[0].radius, 30.0f);
	EXPECT_TRUE(arenas[0].fightOn);
	EXPECT_EQ(arenas[0].creatures, (std::array<uint32_t, 2> {40, 41}));
	EXPECT_FLOAT_EQ(arenas[1].radius, 45.5f);
	EXPECT_FALSE(arenas[1].fightOn);
	EXPECT_EQ(arenas[1].creatures, (std::array<uint32_t, 2> {0, 0}));
}

// The saved games of the user have arenas that the import reads. Each fight reaction of a save is for an arena.
TEST(VanillaWorldImport, SavedGamesHaveArenasThatTheImportReads)
{
	size_t arenas = 0;
	const auto files = ForEachSave([&arenas](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto read = script_import::ReadArenas(world);
		EXPECT_EQ(read.size(), world.OfType(VanillaType::Arena).size()) << path;
		for (const auto& arena : read)
		{
			const auto* record = world.Record(arena.number);
			ASSERT_NE(record, nullptr);
			EXPECT_EQ(world.Float(*record, "arenaOne"), std::optional(1.0f)) << path << " " << arena.number;
			EXPECT_EQ(world.Ref(*record, "arenaNone"), std::optional(0U)) << path << " " << arena.number;
			std::cout << path.filename() << " arena " << arena.number << " at " << arena.position.x << " " << arena.position.z
			          << " radius " << arena.radius << " fight " << arena.fightOn << " creatures " << arena.creatures[0] << " "
			          << arena.creatures[1] << '\n'
			          << std::flush;
		}
		for (const auto& reaction : reaction_import::ReadReactions(world))
		{
			if (reaction.type == static_cast<uint32_t>(Reaction::ReactToFight))
			{
				const auto* target = world.Record(reaction.target);
				ASSERT_NE(target, nullptr) << path << " " << reaction.number;
				EXPECT_TRUE(target->Is(VanillaType::Arena)) << path << " " << reaction.number;
			}
		}
		arenas += read.size();
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << "Arenas " << arenas << '\n' << std::flush;
}

TEST(VanillaWorldImport, AGlideOfTheCameraTakesTheNumbersOfTheFileButTheFifth)
{
	const auto glide = GlideOfNumbers({1.0f, 2.0f, 3.0f, 4.0f, 99.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f});
	EXPECT_EQ(glide.value, 1.0f);
	EXPECT_EQ(glide.target, 2.0f);
	EXPECT_EQ(glide.targetSpeed, 3.0f);
	EXPECT_EQ(glide.speed, 4.0f);
	EXPECT_EQ(glide.elapsed, 6.0f);
	EXPECT_EQ(glide.duration, 7.0f);
	EXPECT_EQ(glide.start, 8.0f);
	EXPECT_EQ(glide.startSpeed, 9.0f);
	EXPECT_EQ(glide.c2, 10.0f);
	EXPECT_EQ(glide.c3, 11.0f);
	EXPECT_EQ(glide.c4, 12.0f);
}

TEST(VanillaWorldImport, TheCameraModesOfTheFileAreTheModesThatTheGameWrites)
{
	EXPECT_EQ(camera_import::ModeOf(0), camera_import::Mode::World);
	EXPECT_EQ(camera_import::ModeOf(0x46), camera_import::Mode::Follow);
	EXPECT_EQ(camera_import::ModeOf(0x48), camera_import::Mode::FollowAndTurn);
	EXPECT_EQ(camera_import::ModeOf(0x65), camera_import::Mode::Free);
	EXPECT_FALSE(camera_import::ModeOf(0x47).has_value());
}

TEST(VanillaWorldImport, AGlideOfTheCameraThatTheSaveStoppedContinuesAsTheGlideOfTheGame)
{
	// Each coordinate glides over two seconds, and the save stopped it after half a second
	const std::array<std::pair<float, float>, 6> ways {
	    {{10.0f, 50.0f}, {5.0f, 8.0f}, {-20.0f, 40.0f}, {100.0f, 60.0f}, {0.0f, 3.0f}, {7.0f, -7.0f}}};
	std::array<camera::Zoomer, 6> game {};
	std::array<camera_import::Glide, 6> file {};
	for (size_t i = 0; i < ways.size(); ++i)
	{
		game.at(i) = GlideFrom(ways.at(i).first, ways.at(i).second, 2.0f, 0.5f);
		file.at(i) = GlideOfNumbers(GlideNumbers(game.at(i)));
	}
	const auto motion = camera_import::MotionOf({file[0], file[1], file[2]}, {file[3], file[4], file[5]});
	ASSERT_TRUE(motion.has_value());
	EXPECT_EQ(motion->time, std::chrono::milliseconds(500));
	EXPECT_EQ(motion->duration, std::chrono::seconds(2));
	EXPECT_EQ(motion->focusTime, std::chrono::milliseconds(500));
	EXPECT_EQ(motion->focusDuration, std::chrono::seconds(2));
	// Then it goes on as the glide of the game goes on, to the end, where it stops at the destination
	for (const float at : {0.5f, 1.2f, 1.9f, 2.0f})
	{
		for (size_t i = 0; i < ways.size(); ++i)
		{
			auto zoomer = game.at(i);
			zoomer.Update(at - 0.5f);
			const auto& interpolator = i < 3 ? motion->origin : motion->focus;
			EXPECT_NEAR(interpolator.PositionAt(at / 2.0f)[static_cast<glm::length_t>(i % 3)], zoomer.Value(), 1.0e-3f)
			    << "coordinate " << i << " at " << at;
		}
	}
}

TEST(VanillaWorldImport, ACameraPointWhoseCoordinatesDoNotGlideTogetherHasNoMotion)
{
	const auto x = GlideOfNumbers(GlideNumbers(GlideFrom(0.0f, 10.0f, 2.0f, 0.5f)));
	const auto y = GlideOfNumbers(GlideNumbers(GlideFrom(0.0f, 10.0f, 1.0f, 0.5f)));
	EXPECT_FALSE(camera_import::MotionOf({x, x, x}, {x, y, x}).has_value());
	EXPECT_TRUE(camera_import::MotionOf({x, x, x}, {x, x, x}).has_value());
}

TEST(VanillaWorldImport, TheLensOfTheCameraContinuesItsGlide)
{
	const auto game = GlideFrom(1.2f, 0.8f, 0.5f, 0.2f);
	auto lens = camera_import::LensOf(GlideOfNumbers(GlideNumbers(game)));
	EXPECT_EQ(lens.Value(), game.Value());
	EXPECT_EQ(lens.Destination(), game.Destination());
	auto expected = game;
	lens.Update(0.1f);
	expected.Update(0.1f);
	EXPECT_EQ(lens.Value(), expected.Value());
	EXPECT_EQ(lens.Speed(), expected.Speed());
}

TEST(VanillaWorldImport, TheCameraOfTheFileReadsWithTheValuesOfItsMode)
{
	std::array<float, 36> eye {};
	std::array<float, 36> focus {};
	for (size_t axis = 0; axis < 3; ++axis)
	{
		const auto numbers = GlideNumbers(GlideFrom(static_cast<float>(axis), 10.0f, 1.0f, 0.25f));
		std::ranges::copy(numbers, eye.begin() + static_cast<std::ptrdiff_t>(axis * 12));
		std::ranges::copy(numbers, focus.begin() + static_cast<std::ptrdiff_t>(axis * 12));
	}
	auto world = GameAtTurn(1);
	world.Record(VanillaType::Camera)
	    .Value("cameraFocusGlide", focus)
	    .Value("cameraEyeGlide", eye)
	    .Value("cameraLensGlide", GlideNumbers(GlideFrom(1.0f, 1.0f, 0.0f, 0.0f)))
	    .Value<uint8_t>("cameraBelowLand", 1)
	    .Value<uint32_t>("mode", 0x46)
	    .Value("followHeading", 0.5f)
	    .Value("followPitch", 0.25f)
	    .Value("followDistance", 40.0f)
	    .Value("followTimeScale", 0.2f)
	    .Value<uint32_t>("followRelativeHeading", 1)
	    .Value<uint32_t>("followUseOffsets", 0)
	    .Value("followFocusOffset", glm::vec3 {1.0f, 2.0f, 3.0f})
	    .Value("followEyeOffset", glm::vec3 {4.0f, 5.0f, 6.0f})
	    .Ref("followTarget", 9)
	    .Value<uint32_t>("followAutoReposition", 1);
	const auto camera = camera_import::ReadCamera(world.Make());
	ASSERT_TRUE(camera.has_value());
	EXPECT_EQ(camera->mode, camera_import::Mode::Follow);
	EXPECT_TRUE(camera->belowLand);
	EXPECT_EQ(camera->eye[2].start, 2.0f);
	EXPECT_EQ(camera->focus[1].elapsed, 0.25f);
	ASSERT_TRUE(camera->follow.has_value());
	EXPECT_EQ(camera->follow->heading, 0.5f);
	EXPECT_EQ(camera->follow->pitch, 0.25f);
	EXPECT_EQ(camera->follow->distance, 40.0f);
	EXPECT_EQ(camera->follow->timeScale, 0.2f);
	EXPECT_TRUE(camera->follow->relativeHeading);
	EXPECT_FALSE(camera->follow->useOffsets);
	EXPECT_EQ(camera->follow->focusOffset, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(camera->follow->eyeOffset, glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_EQ(camera->follow->target, 9u);
	EXPECT_TRUE(camera->follow->autoReposition);
	// A world with no camera has none
	EXPECT_FALSE(camera_import::ReadCamera(GameAtTurn(1).Make()).has_value());
}

TEST(VanillaWorldImport, TheFreeCameraOfTheFileReadsItsSpeedAndItsKeys)
{
	auto world = WorldWithCameraMode(0x65);
	world.Value("freeSpeed", 3.5f)
	    .Value<int32_t>("freeMoveX", -6)
	    .Value<int32_t>("freeMoveY", 4)
	    .Value<uint32_t>("freeKeys", camera_import::k_CtrlKey);
	const auto camera = camera_import::ReadCamera(world.Make());
	ASSERT_TRUE(camera.has_value());
	EXPECT_EQ(camera->mode, camera_import::Mode::Free);
	EXPECT_FALSE(camera->belowLand);
	EXPECT_FALSE(camera->follow.has_value());
	ASSERT_TRUE(camera->free.has_value());
	EXPECT_EQ(camera->free->speed, 3.5f);
	EXPECT_EQ(camera->free->across, -6);
	EXPECT_EQ(camera->free->along, 4);
	EXPECT_TRUE(camera->free->ctrl);
	EXPECT_FALSE(camera->free->shift);
}

TEST(VanillaWorldImport, TheTurningFollowCameraOfTheFileReadsItsTurnAndItsThing)
{
	auto world = WorldWithCameraMode(0x48);
	world.Value("followHeading", 0.75f)
	    .Value("followPitch", 0.3f)
	    .Value("followDistance", 60.0f)
	    .Value("followTurnSpeed", 0.01f)
	    .Ref("followTarget", 12);
	const auto camera = camera_import::ReadCamera(world.Make());
	ASSERT_TRUE(camera.has_value());
	EXPECT_EQ(camera->mode, camera_import::Mode::FollowAndTurn);
	EXPECT_FALSE(camera->free.has_value());
	ASSERT_TRUE(camera->follow.has_value());
	EXPECT_EQ(camera->follow->heading, 0.75f);
	EXPECT_EQ(camera->follow->pitch, 0.3f);
	EXPECT_EQ(camera->follow->distance, 60.0f);
	EXPECT_EQ(camera->follow->turnSpeed, 0.01f);
	EXPECT_EQ(camera->follow->target, 12u);
}
