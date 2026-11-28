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
namespace footpath_import = openblack::ecs::vanilla_footpath_import;

// A rule that did not make the ball yet, or did not take the spin yet, starts again. Data of a kind that the import
// does not know stops the import of the effect.
TEST(VanillaParticleImport, ARuleThatDidNotStartStartsAgainAndUnknownDataStopsTheEffect)
{
	const auto make = [](VanillaType kind) {
		auto fake = GameAtTurn(5);
		fake.Record(VanillaType::ParticleSystem).Ref("systemManager", 3);
		fake.Record(VanillaType::ParticleManager)
		    .Refs("managerRoots", {4})
		    .Value("managerInfo", std::array<float, 15> {})
		    .Ref("managerSpell", 20);
		fake.Record(VanillaType::AtomCollection).Refs("ruleData", {}).Refs("collectionAtoms", {5});
		const std::array<float, 14> draw {};
		fake.Record(VanillaType::AtomCore)
		    .Refs("atomRuleData", {7})
		    .Refs("atomSounds", {})
		    .Value("atomRotation", std::array<float, 12> {})
		    .Value("atomPrevious", draw)
		    .Value("atomCurrent", draw)
		    .Value("atomAnimation", std::array<uint32_t, 3> {})
		    .Ref("atomRender", 6);
		fake.Record(VanillaType::PointParticle);
		fake.Record(kind)
		    .Value("atomDataRule", uint32_t {3})
		    .Value("fireBallNotMade", uint8_t {1})
		    .Ref("fireBallObject", 0)
		    .Value("sideSpinNotStarted", uint8_t {1});
		return fake.Make();
	};
	for (const auto kind : {VanillaType::AtomDataAttachFireBall, VanillaType::AtomDataSideSpin})
	{
		const auto world = make(kind);
		const auto read = particle_import::ReadSpellEffect(world, 2, 20, SameNumber);
		ASSERT_TRUE(read.effect.has_value()) << read.why;
		const auto& data = read.effect->state.roots.front().atoms.front().ruleData.front().data;
		EXPECT_FALSE(data.started);
		EXPECT_TRUE(data.object == entt::null);
	}
	const auto world = make(VanillaType::EmitterRuleData);
	const auto read = particle_import::ReadSpellEffect(world, 2, 20, SameNumber);
	EXPECT_FALSE(read.effect.has_value());
	EXPECT_NE(read.why.find("data of a rule"), std::string::npos) << read.why;
}

TEST(VanillaParticleImport, SavedGamesOfLandOneKeepEveryValueOfTheirParticleContainers)
{
	Checked checked;
	const auto files = ForEachLandOneSave([&checked](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto read = particle_import::ReadContainers(world, SameNumber);
		EXPECT_TRUE(read.skipped.empty()) << path << " " << (read.skipped.empty() ? "" : read.skipped.front());
		const auto list = world.Refs(*world.Fields(world.Game(), "lists").at(particle_import::k_ParticleContainerList));
		ASSERT_EQ(read.containers.size(), list.size()) << path;
		for (size_t i = 0; i < list.size(); ++i)
		{
			// The import takes the oldest container first, the file keeps the newest first
			const auto& container = read.containers[i];
			const auto* record = world.Record(list[list.size() - 1 - i]);
			ASSERT_NE(record, nullptr);
			EXPECT_EQ(container.number, record->number);
			++checked.containers;
			EXPECT_EQ(container.turns, FileValue<int32_t>(world, *record, "containerTurns"));
			EXPECT_EQ(container.scale, FileValue<float>(world, *record, "containerScale"));
			EXPECT_EQ(container.singleSort, FileValue<uint8_t>(world, *record, "containerSingleSort") == 1);
			EXPECT_EQ(container.follows, world.Ref(*record, "containerFollows").value_or(0));
			CheckEffect(world, world.Ref(*record, "containerSystem").value_or(0), container.effect, checked);
			std::cout << path.generic_string() << " container " << container.number << ": type "
			          << static_cast<uint32_t>(container.effect.type) << ", turns " << container.turns << " (file "
			          << FileValue<int32_t>(world, *record, "containerTurns") << "), scale " << container.scale << " (file "
			          << FileValue<float>(world, *record, "containerScale") << "), follows " << container.follows << ", age "
			          << container.effect.state.age << ", atoms " << container.effect.state.atomCount << "\n";
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	std::cout << "checked " << checked.containers << " containers, " << checked.effects << " effects, " << checked.collections
	          << " collections, " << checked.atoms << " atoms, " << checked.emitters << " emitters\n";
	EXPECT_GT(checked.containers, 0U);
	EXPECT_GT(checked.atoms, 0U);
	EXPECT_GT(checked.emitters, 0U);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneGiveEachTempleHeartItsContainers)
{
	uint32_t beams = 0;
	const auto files = ForEachLandOneSave([&beams](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto read = particle_import::ReadContainers(world, SameNumber);
		for (const auto& temple : ecs::vanilla_script_import::ReadTemples(world))
		{
			for (const auto number : {temple.beams, temple.destructionBeams, temple.destructionGlow})
			{
				if (number == 0)
				{
					continue;
				}
				// Each container of a heart is a container of the game list, so the import starts its effect
				EXPECT_TRUE(std::ranges::any_of(read.containers, [number](const auto& c) { return c.number == number; }))
				    << path << " " << number;
				++beams;
				std::cout << path.generic_string() << " heart " << temple.heart << " holds container " << number << "\n";
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(beams, 0U);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneKeepEveryValueOfTheirVorticesThatDrawThingsIn)
{
	uint32_t vortices = 0;
	Checked checked;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto read = particle_import::ReadVorticesIn(world, SameNumber);
		EXPECT_TRUE(read.skipped.empty()) << path;
		for (const auto& vortex : read.vortices)
		{
			const auto* record = world.Record(vortex.number);
			ASSERT_NE(record, nullptr);
			++vortices;
			EXPECT_EQ(vortex.centre, FilePoint(world, *record, "vortexCentre"));
			EXPECT_EQ(static_cast<uint32_t>(vortex.state), FileValue<uint32_t>(world, *record, "vortexState"));
			// A vortex that draws things in is of the first kind
			EXPECT_EQ(FileValue<uint32_t>(world, *record, "vortexType"), static_cast<uint32_t>(VortexType::In));
			EXPECT_EQ(vortex.altitude, FileValue<float>(world, *record, "vortexAltitude"));
			EXPECT_EQ(vortex.groundAverage, FileValue<float>(world, *record, "vortexGroundAverage"));
			EXPECT_EQ(vortex.groundHeights.size(), FileValue<uint32_t>(world, *record, "vortexGroundCount"));
			// The five words that only the save and the load use are 0
			const auto* words = world.Field(*record, "vortexWords");
			ASSERT_NE(words, nullptr);
			EXPECT_EQ(words->count, 5U);
			const auto bytes = world.Raw(*record, "vortexWords");
			EXPECT_TRUE(std::ranges::all_of(bytes, [](uint8_t b) { return b == 0; })) << path;
			ASSERT_TRUE(vortex.mover.has_value());
			CheckEffect(world, world.Ref(*record, "vortexMover").value_or(0), *vortex.mover, checked);
			std::cout << path.generic_string() << " vortex " << vortex.number << ": state "
			          << static_cast<uint32_t>(vortex.state) << ", altitude " << vortex.altitude << ", mover type "
			          << static_cast<uint32_t>(vortex.mover->type) << ", crossing bytes "
			          << (vortex.crossing.has_value() ? vortex.crossing->size() : 0) << "\n";
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(vortices, 0U);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneHaveEachAtomOfTheFileInAnEffectThatTheImportStarts)
{
	size_t atoms = 0;
	const auto files = ForEachLandOneSave([&atoms](const std::filesystem::path& path, const VanillaWorld& world) {
		size_t imported = 0;
		const auto count = [&imported](const particles::RestoredEffect& state) {
			for (const auto& root : state.roots)
			{
				imported += CountAtoms(root);
			}
		};
		for (const auto& container : particle_import::ReadContainers(world, SameNumber).containers)
		{
			count(container.effect.state);
		}
		for (const auto& vortex : particle_import::ReadVorticesIn(world, SameNumber).vortices)
		{
			if (vortex.mover.has_value())
			{
				count(vortex.mover->state);
			}
		}
		EXPECT_EQ(imported, world.OfType(VanillaType::AtomCore).size()) << path;
		atoms += imported;
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(atoms, 0U);
}

TEST(VanillaParticleImport, SavedGamesOfLandOneHaveEveryContainerAndVortexThatTheScriptsHold)
{
	uint32_t held = 0;
	const auto files = ForEachLandOneSave([&held](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto table = ecs::vanilla_script_import::ReadScriptTable(world);
		ASSERT_TRUE(table.has_value());
		const auto containers = particle_import::ReadContainers(world, SameNumber);
		const auto vortices = particle_import::ReadVorticesIn(world, SameNumber);
		for (const auto& place : table->places)
		{
			const auto* record = place.number != 0 ? world.Record(place.number) : nullptr;
			if (record == nullptr)
			{
				continue;
			}
			if (record->Is(VanillaType::ParticleContainer))
			{
				++held;
				EXPECT_TRUE(
				    std::ranges::any_of(containers.containers, [&place](const auto& c) { return c.number == place.number; }))
				    << path << " " << place.number;
			}
			else if (record->Is(VanillaType::LandscapeVortexIn))
			{
				++held;
				EXPECT_TRUE(
				    std::ranges::any_of(vortices.vortices, [&place](const auto& v) { return v.number == place.number; }))
				    << path << " " << place.number;
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(held, 0U);
}

// Each footpath of a Land 1 save keeps every value of the file: its nodes in order, the place, the flags and the
// walkers of each node, and the word that lets walkers take it. Each link of a thing keeps its footpaths in order.
TEST(VanillaFootpathImport, SavedGamesOfLandOneKeepEveryValueOfTheirFootpaths)
{
	uint32_t paths = 0;
	uint32_t nodes = 0;
	uint32_t links = 0;
	std::map<uint32_t, uint32_t> holderTypes;
	std::map<uint8_t, uint32_t> flags;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = footpath_import::ReadFootpaths(world);
		EXPECT_TRUE(imported.skipped.empty()) << path << " " << (imported.skipped.empty() ? "" : imported.skipped.front());
		EXPECT_EQ(imported.finders, 0U) << path;
		// Every footpath and every node of the file is in a footpath of the list, once
		std::set<uint32_t> listed;
		std::set<uint32_t> listedNodes;
		for (const auto& footpath : imported.paths)
		{
			EXPECT_TRUE(listed.insert(footpath.number).second) << path;
			const auto* record = world.Record(footpath.number);
			ASSERT_NE(record, nullptr) << path;
			EXPECT_EQ(footpath.walkable, world.Word(*record, "walkable")) << path;
			const auto refs = world.Refs(*record, "nodes");
			ASSERT_EQ(footpath.nodes.size(), refs.size()) << path;
			for (size_t i = 0; i < refs.size(); ++i)
			{
				const auto& node = footpath.nodes[i];
				EXPECT_EQ(node.number, refs[i]) << path;
				EXPECT_TRUE(listedNodes.insert(node.number).second) << path;
				const auto& nodeRecord = *world.Record(node.number);
				const auto place = world.Raw(nodeRecord, "pos");
				ASSERT_EQ(place.size(), sizeof(node.coords)) << path;
				// NOLINTNEXTLINE(bugprone-suspicious-memory-comparison): the compare must be bit exact
				EXPECT_EQ(std::memcmp(place.data(), &node.coords, sizeof(node.coords)), 0) << path;
				EXPECT_EQ(node.flags, world.Byte(nodeRecord, "nodeFlags")) << path;
				const auto followers = world.Refs(nodeRecord, "followers");
				EXPECT_TRUE(std::ranges::equal(node.followers, followers)) << path;
				// openblack keeps every flag of the file but the first, which the game sets on each node it makes
				EXPECT_EQ(node.flags & ~footpath_import::k_NodeKeptFlags, 0) << path;
				EXPECT_NE(node.flags & footpath_import::k_NodeMade, 0) << path;
				const auto made = footpath_import::NodeOf(node, 3.5f);
				EXPECT_EQ(made.place, glm::ivec2(node.coords.x, node.coords.z)) << path;
				EXPECT_EQ(made.planned, (node.flags & footpath_import::k_NodePlanned) != 0) << path;
				EXPECT_EQ(made.hidden, (node.flags & footpath_import::k_NodeHidden) != 0) << path;
				EXPECT_EQ(made.marker, (node.flags & footpath_import::k_NodeMarker) != 0) << path;
				EXPECT_EQ(made.position.y, node.coords.altitude + 3.5f) << path;
				++flags[node.flags];
				++nodes;
			}
			++paths;
		}
		uint32_t footpathRecords = 0;
		uint32_t nodeRecords = 0;
		uint32_t linkRecords = 0;
		for (const auto& record : world.Records())
		{
			footpathRecords += record.Is(VanillaType::Footpath) ? 1 : 0;
			nodeRecords += record.Is(VanillaType::FootpathNode) ? 1 : 0;
			linkRecords += record.Is(VanillaType::FootpathLink) ? 1 : 0;
		}
		EXPECT_EQ(listed.size(), footpathRecords) << path;
		EXPECT_EQ(listedNodes.size(), nodeRecords) << path;
		// Every link of the file is the link of one thing, and its footpaths are footpaths of the list
		std::set<uint32_t> linked;
		for (const auto& link : imported.links)
		{
			EXPECT_TRUE(linked.insert(link.number).second) << path;
			EXPECT_TRUE(std::ranges::equal(link.footpaths, world.Refs(*world.Record(link.number), "footpaths"))) << path;
			for (const auto footpath : link.footpaths)
			{
				EXPECT_TRUE(listed.contains(footpath)) << path << " link " << link.number;
			}
			++holderTypes[world.Record(link.holder)->type];
			++links;
		}
		EXPECT_EQ(linked.size(), linkRecords) << path;
		std::cout << path.parent_path().filename().string() << " footpaths " << imported.paths.size() << " links "
		          << imported.links.size() << '\n'
		          << std::flush;
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(paths, 0U);
	std::cout << "Footpaths " << paths << ", nodes " << nodes << ", links " << links << '\n' << std::flush;
	for (const auto& [type, count] : holderTypes)
	{
		std::cout << "Links on type 0x" << std::hex << type << std::dec << ": " << count << '\n' << std::flush;
	}
	for (const auto& [value, count] : flags)
	{
		std::cout << "Node flags 0x" << std::hex << static_cast<int>(value) << std::dec << ": " << count << '\n' << std::flush;
	}
}

// Each villager of a Land 1 save that walks a footpath keeps its walk: the footpath, the node that it walks to, the way
// and the footpath after it are footpaths and nodes of the file. The walkers of each node are the villagers that walk
// to it.
TEST(VanillaFootpathImport, SavedGamesOfLandOneKeepTheFootpathWalksOfTheirVillagers)
{
	constexpr auto k_OnPath = static_cast<uint8_t>(VillagerStates::MoveOnPath);
	uint32_t walks = 0;
	uint32_t kept = 0;
	uint32_t withNext = 0;
	uint32_t towardsStart = 0;
	uint32_t noNode = 0;
	uint32_t stale = 0;
	std::map<uint8_t, uint32_t> finals;
	std::map<uint8_t, uint32_t> moveStates;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto footpaths = footpath_import::ReadFootpaths(world);
		const auto imported = world_import::ReadTowns(world);
		std::map<uint32_t, const footpath_import::ImportedFootpath*> byNumber;
		for (const auto& footpath : footpaths.paths)
		{
			byNumber.emplace(footpath.number, &footpath);
		}
		std::map<uint32_t, std::set<uint32_t>> walkersOf;
		for (const auto& villager : imported.villagers)
		{
			if (villager.states[0] != k_OnPath)
			{
				continue;
			}
			++walks;
			const auto& walk = villager.footpathWalk;
			// The values of the state are the footpath and the node of the living thing
			ASSERT_EQ(villager.topValues.size(), 2U) << path;
			EXPECT_EQ(villager.topValues[0].object.value_or(0), walk.footpath) << path << " " << villager.number;
			EXPECT_EQ(villager.topValues[1].object.value_or(0), walk.node) << path << " " << villager.number;
			const auto footpath = byNumber.find(walk.footpath);
			ASSERT_NE(footpath, byNumber.end()) << path << " " << villager.number;
			if (walk.node == 0)
			{
				++noNode;
			}
			else
			{
				EXPECT_TRUE(footpath_import::NodeIndexOf(*footpath->second, walk.node).has_value())
				    << path << " " << villager.number;
				walkersOf[walk.node].insert(villager.number);
			}
			if (walk.next != 0)
			{
				EXPECT_TRUE(byNumber.contains(walk.next)) << path << " " << villager.number;
				++withNext;
			}
			towardsStart += walk.towardsStart ? 1 : 0;
			++finals[villager.states[1]];
			++moveStates[villager.moveState];
			kept += world_import::StartStates(villager) == villager.states ? 1 : 0;
		}
		// The walkers of each node: a walker that walks to the node, or a walker that went on and that the game drops
		// from the list when it next looks at the list
		for (const auto& footpath : footpaths.paths)
		{
			for (const auto& node : footpath.nodes)
			{
				const auto& walkers = walkersOf[node.number];
				for (const auto follower : node.followers)
				{
					stale += walkers.contains(follower) ? 0 : 1;
				}
				for (const auto walker : walkers)
				{
					EXPECT_NE(std::ranges::find(node.followers, walker), node.followers.end()) << path << " " << walker;
				}
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(walks, 0U);
	std::cout << "Walks " << walks << ", kept " << kept << ", with a next footpath " << withNext << ", towards the start "
	          << towardsStart << ", no node " << noNode << ", walkers of nodes that walk on " << stale << '\n'
	          << std::flush;
	for (const auto& [state, count] : finals)
	{
		std::cout << "Final state " << static_cast<int>(state) << ": " << count << '\n' << std::flush;
	}
	for (const auto& [state, count] : moveStates)
	{
		std::cout << "Move state 0x" << std::hex << static_cast<int>(state) << std::dec << ": " << count << '\n' << std::flush;
	}
}

TEST(VanillaWorldImport, AScriptStateKeepsThePlaceTheNumberAndThePairOfItsScript)
{
	const std::vector values = {BytesValue({0x00, 0x00, 0x01, 0x00, 0x00, 0x80, 0x02, 0x00}),
	                            BytesValue({0x00, 0x00, 0x20, 0x41}), BytesValue({0x8D, 0x01, 0x00, 0x00}),
	                            BytesValue({0x03, 0x00, 0x00, 0x00})};
	const auto script = world_import::ScriptValuesOf(static_cast<uint8_t>(VillagerStates::InScript), values);
	ASSERT_TRUE(script.has_value());
	EXPECT_EQ(script->x, 0x10000);
	EXPECT_EQ(script->z, 0x28000);
	EXPECT_EQ(script->number, 10.0f);
	EXPECT_EQ(script->first, 397U);
	EXPECT_EQ(script->second, 3U);
	for (const auto state :
	     {VillagerStates::ScriptWanderAroundPosition, VillagerStates::ScriptPlayAnim, VillagerStates::ScriptInCrowd})
	{
		EXPECT_TRUE(world_import::ScriptValuesOf(static_cast<uint8_t>(state), values).has_value());
	}
	// Another state, or values of other sizes, give nothing
	EXPECT_FALSE(world_import::ScriptValuesOf(static_cast<uint8_t>(VillagerStates::Dead), values).has_value());
	const std::vector three(values.begin(), values.begin() + 3);
	EXPECT_FALSE(world_import::ScriptValuesOf(static_cast<uint8_t>(VillagerStates::InScript), three).has_value());
}

TEST(VanillaWorldImport, APlaceInMetresGivesTheMapUnitsAgain)
{
	// The game makes a place of a script from metres, so each place of a file is the map units of a float
	for (const int32_t units : {0, 1, 65536, 0x28000, 8090858, map_coords::ToFixed(4999.37f), map_coords::ToFixed(-3.3f)})
	{
		EXPECT_EQ(map_coords::ToFixed(world_import::MetresOfMapUnits(units)), units) << units;
	}
}

TEST(VanillaWorldImport, AShepherdStateKeepsItsCountdownAndTheStateAfterItsWait)
{
	const std::vector values = {BytesValue({0xFF, 0xFF}), BytesValue({0x5F, 0x00})};
	const auto shepherd =
	    world_import::ShepherdValuesOf(static_cast<uint8_t>(VillagerStates::ShepherdCheckAnimalForSlaughter), values);
	ASSERT_TRUE(shepherd.has_value());
	EXPECT_EQ(shepherd->countdown, -1);
	EXPECT_EQ(shepherd->pending, static_cast<uint16_t>(VillagerStates::ShepherdDecideWhatToDoWithFlock));
	EXPECT_TRUE(world_import::ShepherdValuesOf(static_cast<uint8_t>(VillagerStates::ShepherdLookForFlock), values));
	EXPECT_FALSE(world_import::ShepherdValuesOf(static_cast<uint8_t>(VillagerStates::InScript), values));
}

TEST(VanillaWorldImport, TheDeadStateKeepsTheReasonForTheDeath)
{
	constexpr auto k_Dead = static_cast<uint8_t>(VillagerStates::Dead);
	EXPECT_EQ(world_import::DeathReasonOf(k_Dead, {BytesValue({0x09})}), DeathReason::OldAge);
	EXPECT_EQ(world_import::DeathReasonOf(k_Dead, {BytesValue({0x00})}), DeathReason::None);
	// A reason that the game does not have, or another state, gives nothing
	EXPECT_FALSE(world_import::DeathReasonOf(k_Dead, {BytesValue({0x0A})}).has_value());
	EXPECT_FALSE(world_import::DeathReasonOf(static_cast<uint8_t>(VillagerStates::Dying), {BytesValue({0x09})}).has_value());
	// The pose on its back is pose 2 in bits 4 and 5 of the living status
	EXPECT_TRUE(world_import::LiesOnBack(0x0029));
	EXPECT_FALSE(world_import::LiesOnBack(0x0039));
	EXPECT_FALSE(world_import::LiesOnBack(0x0019));
	EXPECT_FALSE(world_import::LiesOnBack(0x0000));
}

TEST(VanillaWorldImport, ATradingDiscipleWithNothingToDoKeepsItsResourceAndItsTown)
{
	constexpr auto k_Idle = static_cast<uint8_t>(VillagerStates::DiscipleNothingToDo);
	constexpr auto k_Trader = static_cast<uint8_t>(VillagerDisciple::Trader);
	const std::vector values = {BytesValue({0x01, 0x00, 0x00, 0x00}), ObjectValue(42)};
	const auto trade = world_import::TradeOf(k_Idle, k_Trader, values);
	ASSERT_TRUE(trade.has_value());
	EXPECT_EQ(trade->resource, ResourceType::Wood);
	EXPECT_EQ(trade->town, 42U);
	// Another disciple keeps no values, and another state gives nothing
	EXPECT_FALSE(world_import::TradeOf(k_Idle, static_cast<uint8_t>(VillagerDisciple::Fisherman), {}).has_value());
	EXPECT_FALSE(world_import::TradeOf(static_cast<uint8_t>(VillagerStates::InScript), k_Trader, values).has_value());
}

TEST(VanillaWorldImport, TheVillagerFlagsGiveADiscipleOrAFollower)
{
	const auto disciple = world_import::DiscipleOf(3, 0x1200);
	ASSERT_TRUE(disciple.has_value());
	EXPECT_EQ(disciple->type, VillagerDisciple::Fisherman);
	EXPECT_FALSE(disciple->follower);
	const auto follower = world_import::DiscipleOf(0, 0x0400);
	ASSERT_TRUE(follower.has_value());
	EXPECT_EQ(follower->type, VillagerDisciple::None);
	EXPECT_TRUE(follower->follower);
	// With neither flag the villager is not a disciple, and a type that the game does not have gives nothing
	EXPECT_FALSE(world_import::DiscipleOf(3, 0x1000).has_value());
	EXPECT_FALSE(world_import::DiscipleOf(13, 0x0200).has_value());
}

// No villager of a Land 1 save decides again after the import: each keeps its states, and the values of its shepherd,
// script, dead and disciple states are the bytes of the file
TEST(VanillaWorldImport, SavedGamesOfLandOneHaveNoVillagerThatDecidesAgain)
{
	uint32_t villagers = 0;
	uint32_t decideAgain = 0;
	uint32_t shepherds = 0;
	uint32_t scripts = 0;
	uint32_t dead = 0;
	uint32_t idleDisciples = 0;
	uint32_t disciples = 0;
	uint32_t followers = 0;
	const auto files = ForEachLandOneSave([&](const std::filesystem::path& path, const VanillaWorld& world) {
		const auto imported = world_import::ReadTowns(world);
		for (const auto& villager : imported.villagers)
		{
			++villagers;
			const auto* record = world.Record(villager.number);
			ASSERT_NE(record, nullptr);
			EXPECT_EQ(villager.discipleType, world.Byte(*record, "discipleType").value_or(0xFF));
			EXPECT_EQ(villager.villagerFlags, world.Half(*record, "villagerFlags").value_or(0xFFFF));
			EXPECT_EQ(villager.status, world.Half(*record, "status").value_or(0xFFFF));
			if (const auto disciple = world_import::DiscipleOf(villager.discipleType, villager.villagerFlags);
			    disciple.has_value())
			{
				EXPECT_EQ(static_cast<uint8_t>(disciple->type), villager.discipleType) << path << " " << villager.number;
				disciples += disciple->follower ? 0 : 1;
				followers += disciple->follower ? 1 : 0;
			}
			else
			{
				EXPECT_EQ(villager.discipleType, 0) << path << " " << villager.number;
			}
			if (world_import::StartStates(villager) != villager.states)
			{
				++decideAgain;
				ADD_FAILURE() << path << " " << villager.number << " decides again in state "
				              << static_cast<int>(villager.states[0]) << "/" << static_cast<int>(villager.states[1]);
				continue;
			}
			const std::array<std::pair<uint8_t, const std::vector<world_import::ImportedStateValue>*>, 2> states = {
			    std::pair {villager.states[0], &villager.topValues}, std::pair {villager.states[1], &villager.finalValues}};
			for (const auto& [state, values] : states)
			{
				if (const auto script = world_import::ScriptValuesOf(state, *values); script.has_value())
				{
					++scripts;
					const auto place = BytesAs<std::array<int32_t, 2>>((*values)[0]);
					EXPECT_EQ(script->x, place[0]);
					EXPECT_EQ(script->z, place[1]);
					EXPECT_EQ(std::bit_cast<uint32_t>(script->number), BytesAs<uint32_t>((*values)[1]));
					EXPECT_EQ(script->first, BytesAs<uint32_t>((*values)[2]));
					EXPECT_EQ(script->second, BytesAs<uint32_t>((*values)[3]));
					EXPECT_EQ(map_coords::ToFixed(world_import::MetresOfMapUnits(script->x)), script->x);
					EXPECT_EQ(map_coords::ToFixed(world_import::MetresOfMapUnits(script->z)), script->z);
				}
				if (const auto shepherd = world_import::ShepherdValuesOf(state, *values); shepherd.has_value())
				{
					++shepherds;
					EXPECT_EQ(shepherd->countdown, BytesAs<int16_t>((*values)[0]));
					EXPECT_EQ(shepherd->pending, BytesAs<uint16_t>((*values)[1]));
				}
				if (const auto reason = world_import::DeathReasonOf(state, *values); reason.has_value())
				{
					++dead;
					EXPECT_EQ(static_cast<uint8_t>(*reason), (*values)[0].bytes[0]);
				}
				if (state == static_cast<uint8_t>(VillagerStates::DiscipleNothingToDo))
				{
					++idleDisciples;
					EXPECT_TRUE(values->empty() || world_import::TradeOf(state, villager.discipleType, *values).has_value());
				}
			}
		}
	});
	if (!files.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	EXPECT_GT(villagers, 0U);
	EXPECT_EQ(decideAgain, 0U);
	EXPECT_GT(shepherds, 0U);
	EXPECT_GT(scripts, 0U);
	EXPECT_GT(dead, 0U);
	EXPECT_GT(idleDisciples, 0U);
	EXPECT_GT(disciples, 0U);
	std::cout << "Villagers " << villagers << ", shepherd values " << shepherds << ", script values " << scripts << ", dead "
	          << dead << ", idle disciples " << idleDisciples << ", disciples " << disciples << ", followers " << followers
	          << '\n'
	          << std::flush;
}

// A dance of the file keeps its groups with their values and dancers, the order in which newcomers try them, and its key
// frames with their actions
TEST(VanillaWorldImport, ReadsTheGroupsAndTheKeyFramesOfADance)
{
	auto fake = GameAtTurn(100);
	// 2: the dance, 3 and 4: its groups, 5: a key frame, 6: its action
	fake.Record(VanillaType::Dance)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::CitadelDance_1))
	    .Refs("danceKeyFrames", {5})
	    .Refs("danceGroups", {3, 4})
	    .Refs("danceLimitedGroups", {4})
	    .Refs("danceSharedGroups", {3})
	    .Value<uint8_t>("danceRound", 1)
	    .Value<uint32_t>("danceRoundLength", 2);
	std::array<char, 64> name {};
	name[0] = 'M';
	fake.Record(VanillaType::DanceGroup)
	    .Ref("groupDance", 2)
	    .Ref("groupBehaviour", 2)
	    .Refs("groupDancers", {7, 8})
	    .Value<uint32_t>("groupId", 0)
	    .Value<uint32_t>("groupFlag", 0)
	    .Value<uint32_t>("groupFormation", 2)
	    .Value<uint32_t>("groupShape", 3)
	    .Value("groupOffset", std::array<float, 2> {1.5f, -2.0f})
	    .Value<float>("groupRadius", 12.0f)
	    .Value<float>("groupRotation", 512.0f)
	    .Value("groupMove", std::array<uint32_t, 4> {11, std::bit_cast<uint32_t>(8.0f), 0, 300})
	    .Value<float>("groupSpin", 64.5f)
	    .Ref("groupParent", 0)
	    .Value<uint8_t>("groupIndexInParent", 0)
	    .Value<uint32_t>("groupLimitedDancers", 0)
	    .Value<uint32_t>("groupMoved", 1)
	    .Value<uint32_t>("groupDanceType", 0)
	    .Value<uint32_t>("groupInFormation", 1)
	    .Value("groupName", name)
	    .Value("groupMembership", std::array<uint32_t, 3> {0, 50, 1})
	    .Words("groupChildren", {1})
	    .Value<uint32_t>("groupSexes", 1);
	fake.Record(VanillaType::DanceGroup)
	    .Refs("groupDancers", {})
	    .Value<uint32_t>("groupId", 1)
	    .Ref("groupParent", 3)
	    .Value<uint8_t>("groupIndexInParent", 0)
	    .Value("groupMembership", std::array<uint32_t, 3> {4, 4, 1})
	    .Words("groupChildren", {})
	    .Value<uint32_t>("groupSexes", 2);
	fake.Record(VanillaType::DanceKeyFrame)
	    .Value<float>("keyFrameTime", 25.0f)
	    .Refs("keyFrameActions", {6})
	    .Value<uint32_t>("keyFrameFlags", 1);
	std::array<uint32_t, 14> arguments {};
	arguments[0] = 7;
	fake.Record(VanillaType::DanceKeyAction)
	    .Words("keyActionGroups", {1, 0})
	    .Value<uint32_t>("keyActionType", 3)
	    .Value("keyActionArguments", arguments);
	const auto world = fake.Make();
	const auto dance = world_import::ReadDance(world, *world.Record(2));
	ASSERT_TRUE(dance.has_value());
	ASSERT_EQ(dance->groups.size(), 2U);
	const auto& first = dance->groups[0];
	EXPECT_EQ(first.number, 3U);
	EXPECT_EQ(first.name, "M");
	EXPECT_EQ(first.dancers, (std::vector<uint32_t> {7, 8}));
	EXPECT_FALSE(first.limited);
	EXPECT_EQ(first.quota, 50U);
	EXPECT_EQ(first.weight, 1U);
	EXPECT_EQ(first.sexes, 1U);
	EXPECT_EQ(first.formation, 2U);
	EXPECT_EQ(first.inFormation, 1U);
	EXPECT_FALSE(first.flag);
	EXPECT_EQ(first.shape, 3U);
	EXPECT_FLOAT_EQ(first.radius, 12.0f);
	EXPECT_FLOAT_EQ(first.offset.x, 1.5f);
	EXPECT_FLOAT_EQ(first.offset.y, -2.0f);
	EXPECT_FLOAT_EQ(first.rotation, 512.0f);
	EXPECT_FLOAT_EQ(first.spin, 64.5f);
	EXPECT_EQ(first.move.action, 11U);
	EXPECT_EQ(first.move.first, std::bit_cast<uint32_t>(8.0f));
	EXPECT_EQ(first.move.end, 300U);
	EXPECT_TRUE(first.moved);
	EXPECT_EQ(first.parent, 0U);
	EXPECT_EQ(first.children, (std::vector<uint32_t> {1}));
	const auto& second = dance->groups[1];
	EXPECT_EQ(second.id, 1U);
	EXPECT_TRUE(second.limited);
	EXPECT_EQ(second.quota, 4U);
	EXPECT_EQ(second.parent, 3U);
	EXPECT_EQ(dance->limitedGroups, (std::vector<size_t> {1}));
	EXPECT_EQ(dance->sharedGroups, (std::vector<size_t> {0}));
	EXPECT_EQ(dance->round, 1U);
	EXPECT_EQ(dance->roundLength, 2U);
	ASSERT_EQ(dance->keyFrames.size(), 1U);
	EXPECT_FLOAT_EQ(dance->keyFrames[0].time, 25.0f);
	EXPECT_EQ(dance->keyFrames[0].flags, 1U);
	ASSERT_EQ(dance->keyFrames[0].actions.size(), 1U);
	EXPECT_EQ(dance->keyFrames[0].actions[0].groups, (std::vector<uint32_t> {1, 0}));
	EXPECT_EQ(dance->keyFrames[0].actions[0].type, 3U);
	EXPECT_EQ(dance->keyFrames[0].actions[0].arguments, arguments);
}

// A dance takes the groups of the file. A dancer that the import does not keep leaves its group, and the places of the
// others close up.
TEST(VanillaWorldImport, ADanceTakesTheGroupsOfTheFile)
{
	constexpr auto k_Kept = static_cast<entt::entity>(40);
	constexpr auto k_Other = static_cast<entt::entity>(41);
	world_import::ImportedDance file {.round = 1, .roundLength = 3};
	file.groups.push_back({.number = 3,
	                       .id = 0,
	                       .name = "M",
	                       .dancers = {7, 8},
	                       .quota = 50,
	                       .weight = 1,
	                       .sexes = 1,
	                       .radius = 12.0f,
	                       .offset = {1.0f, 2.0f},
	                       .spin = 10.0f,
	                       .move = {.action = 1, .first = 0, .second = 0, .end = 200},
	                       .children = {1}});
	file.groups.push_back({.number = 4,
	                       .id = 1,
	                       .dancers = {9, 10},
	                       .limited = true,
	                       .quota = 4,
	                       .limitedDancers = 2,
	                       .parent = 3,
	                       .indexInParent = 0});
	file.limitedGroups = {1};
	file.sharedGroups = {0};
	ecs::components::Dance dance;
	dance.dancers = 5;
	const std::unordered_map<uint32_t, entt::entity> kept {{8, k_Kept}, {10, k_Other}};
	const auto placed = world_import::PutDanceGroups(dance, file, kept);

	ASSERT_EQ(dance.groups.all.size(), 2U);
	const auto& first = dance.groups.all[0];
	EXPECT_EQ(first.name, "M");
	EXPECT_EQ(first.dancers, (std::vector<entt::entity> {k_Kept}));
	EXPECT_EQ(first.quota, 50U);
	EXPECT_EQ(first.weight, 1U);
	EXPECT_EQ(first.sexes, 1U);
	EXPECT_FLOAT_EQ(first.radius, 12.0f);
	EXPECT_FLOAT_EQ(first.offset.y, 2.0f);
	EXPECT_FLOAT_EQ(first.spin, 10.0f);
	EXPECT_EQ(first.move.end, 200U);
	// The file keeps no rate of a move
	EXPECT_FLOAT_EQ(first.spinRate, 0.0f);
	EXPECT_EQ(first.children, (std::vector<std::size_t> {1}));
	EXPECT_FALSE(first.parent.has_value());
	const auto& second = dance.groups.all[1];
	EXPECT_EQ(second.dancers, (std::vector<entt::entity> {k_Other}));
	// One of its two dancers left, so it took one dancer
	EXPECT_EQ(second.limitedDancers, 1U);
	EXPECT_EQ(second.parent, std::optional<std::size_t> {0});
	EXPECT_EQ(dance.groups.limited, (std::vector<std::size_t> {1}));
	EXPECT_EQ(dance.groups.shared, (std::vector<std::size_t> {0}));
	EXPECT_EQ(dance.groups.round, 1U);
	EXPECT_EQ(dance.groups.roundLength, 3U);
	EXPECT_EQ(dance.dancers, 2U);
	ASSERT_EQ(placed.size(), 2U);
	EXPECT_EQ(placed[0].dancer, k_Kept);
	EXPECT_EQ(placed[0].group, 0U);
	EXPECT_EQ(placed[1].dancer, k_Other);
	EXPECT_EQ(placed[1].group, 1U);
}

// The saved game keeps the groups of an action in the order of the dance file
TEST(VanillaWorldImport, TheKeyFramesOfADanceAreTheKeyFramesOfItsFile)
{
	dance::DanceFile file;
	file.keyFrames.push_back({.time = 0.0f, .flags = 1, .actions = {{.groups = {0, 1}, .type = 6, .arguments = {0, 50}}}});
	std::vector<world_import::ImportedDanceKeyFrame> saved {
	    {.time = 0.0f, .flags = 1, .actions = {{.groups = {0, 1}, .type = 6, .arguments = {0, 50}}}}};
	EXPECT_TRUE(world_import::SameKeyFrames(file, saved));
	saved[0].actions[0].groups = {1, 0};
	EXPECT_FALSE(world_import::SameKeyFrames(file, saved));
	saved[0].actions[0].groups = {0, 1};
	saved[0].time = 5.0f;
	EXPECT_FALSE(world_import::SameKeyFrames(file, saved));
	saved.clear();
	EXPECT_FALSE(world_import::SameKeyFrames(file, saved));
}

// A villager keeps a state of a dance only as a dancer of a group that the import makes, and a state of worship only
// at a site that the import makes
TEST(VanillaWorldImport, DancersAndWorshippersKeepTheirStatesOnlyInADanceAndASiteOfTheImport)
{
	constexpr auto k_Worship = static_cast<uint8_t>(VillagerStates::WorshippingAtWorshipSite);
	constexpr auto k_ToDancePlace = static_cast<uint8_t>(VillagerStates::MoveToDancePos);
	constexpr auto k_Hiding = static_cast<uint8_t>(VillagerStates::HidingAtWorshipSite);
	constexpr auto k_InDance = static_cast<uint8_t>(VillagerStates::InDance);
	const auto decide = world_import::DecideAgain();
	world_import::ImportedVillager villager {.states = {k_Worship, k_Worship, 0}, .moveState = 0x01};
	EXPECT_EQ(world_import::StartStates(villager), decide);
	villager.importedWorshipper = true;
	EXPECT_EQ(world_import::StartStates(villager), decide);
	villager.importedDancer = true;
	EXPECT_EQ(world_import::StartStates(villager), villager.states);
	villager.states = {k_ToDancePlace, k_Worship, 0};
	EXPECT_EQ(world_import::StartStates(villager), villager.states);
	villager.importedWorshipper = false;
	EXPECT_EQ(world_import::StartStates(villager), decide);
	villager.states = {k_InDance, k_InDance, 0};
	EXPECT_EQ(world_import::StartStates(villager), villager.states);
	villager.importedDancer = false;
	EXPECT_EQ(world_import::StartStates(villager), decide);
	villager.states = {k_Hiding, k_Hiding, 0};
	EXPECT_EQ(world_import::StartStates(villager), decide);
	villager.importedWorshipper = true;
	EXPECT_EQ(world_import::StartStates(villager), villager.states);
	// A walk that the import cannot keep still decides again
	villager.moveState = 0x05;
	EXPECT_EQ(world_import::StartStates(villager), decide);
}

// The import marks a villager as a dancer when a group of the dance of a site has it and its group is that group
TEST(VanillaWorldImport, TheImportFindsTheDancersAndTheWorshippersOfASite)
{
	auto fake = GameAtTurn(100);
	// 2: the site, 3: its dance, 4: a group, 5 to 7: villagers
	fake.Record(VanillaType::WorshipSite)
	    .Ref("partCitadel", 30)
	    .Value<uint32_t>("siteTribe", world_import::k_FirstTribeRowNumber - static_cast<uint32_t>(Tribe::NORSE))
	    .Ref("siteDance", 3)
	    .Refs("siteWorshippers", {5, 6})
	    .Value<uint8_t>("sitePlace", 1);
	fake.Record(VanillaType::Dance)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::CitadelDance_1))
	    .Refs("danceGroups", {4});
	fake.Record(VanillaType::DanceGroup).Refs("groupDancers", {5, 7});
	for (const uint32_t group : {4U, 0U, 4U})
	{
		fake.Record(VanillaType::Villager)
		    .Position(1, 2)
		    .Value("info", VillagerRowNumber(VillagerInfo::NorseForesterMale))
		    .Value("action", std::array<uint8_t, 6> {})
		    .Ref("danceGroup", group);
	}
	const auto imported = world_import::ReadTowns(fake.Make());
	ASSERT_EQ(imported.villagers.size(), 3U);
	EXPECT_TRUE(imported.villagers[0].importedDancer);
	EXPECT_TRUE(imported.villagers[0].importedWorshipper);
	EXPECT_FALSE(imported.villagers[1].importedDancer);
	EXPECT_TRUE(imported.villagers[1].importedWorshipper);
	EXPECT_TRUE(imported.villagers[2].importedDancer);
	EXPECT_FALSE(imported.villagers[2].importedWorshipper);
}

// The import makes each dance that is not the dance of a worship site: the dances of the scripts, the reactions, the
// artefacts of the towns and the creatures
TEST(VanillaWorldImport, TheImportMakesTheDancesOfCreaturesReactionsArtefactsAndScripts)
{
	auto fake = GameAtTurn(100);
	// 2: a site, 3: its dance, 4: a dance of a script, 5: a dance of a reaction, 6: the reaction, 7: a dance of an
	// artefact, 8: the artefact, 9: a dance of a creature
	fake.Record(VanillaType::WorshipSite).Ref("siteDance", 3);
	fake.Record(VanillaType::Dance).Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::CitadelDance_1));
	fake.Record(VanillaType::Dance)
	    .Position(1000, -2000)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::DanceAroundPerson))
	    .Ref("danceOwner", 30)
	    .Value<uint32_t>("danceDuration", 400)
	    .Value<uint32_t>("danceCameraOff", 1);
	fake.Record(VanillaType::Dance).Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::BuildingSmall));
	fake.Record(VanillaType::Reaction).Ref("", 40).Ref("reactionDance", 5);
	fake.Record(VanillaType::Dance)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::DanceAroundObjectDiameterLess_7));
	fake.Record(VanillaType::TownArtifact).Ref("artefactObject", 41).Ref("artefactTown", 42).Ref("artefactDance", 7);
	fake.Record(VanillaType::Dance)
	    .Value<uint32_t>("danceRow", static_cast<uint32_t>(DanceInfo::CreatureDanceToImpressVillagers))
	    .Ref("danceOwner", 31);
	const auto dances = world_import::ReadOtherDances(fake.Make());
	ASSERT_EQ(dances.made.size(), 4U);
	const auto& script = dances.made[0];
	EXPECT_EQ(script.number, 4U);
	EXPECT_EQ(script.type, DanceInfo::DanceAroundPerson);
	EXPECT_EQ(script.owner, 30U);
	EXPECT_EQ(script.place.x, 1000);
	EXPECT_EQ(script.place.z, -2000);
	EXPECT_EQ(script.duration, 400U);
	EXPECT_TRUE(script.madeByScript);
	// The dance of the reaction and the dance of the artefact are made with them
	EXPECT_EQ(dances.made[1].number, 5U);
	EXPECT_EQ(dances.made[2].number, 7U);
	EXPECT_EQ(dances.made[3].number, 9U);
	EXPECT_EQ(dances.made[3].owner, 31U);
	EXPECT_FALSE(dances.made[3].madeByScript);
}

// A dance that the import makes on its own has the row, the place, the length and the angle of the file, is for the
// thing that the import made, and keeps whether a script made it
TEST(VanillaWorldImport, ADanceOfTheFileIsMadeWithItsPlaceItsThingAndItsLength)
{
	constexpr auto k_Owner = static_cast<entt::entity>(12);
	const world_import::ImportedDance file {.type = DanceInfo::DanceAroundPerson,
	                                        .place = {.x = 3000, .z = 4000, .altitude = 2.5f},
	                                        .madeByScript = true,
	                                        .angle = 1.25f,
	                                        .duration = 600};
	const auto setup = world_import::SetupOf(file, k_Owner, true);
	EXPECT_EQ(setup.type, DanceInfo::DanceAroundPerson);
	EXPECT_TRUE(setup.autostart);
	EXPECT_EQ(setup.place, (map_coords::MapCoords {.x = 3000, .z = 4000, .altitude = 2.5f}));
	EXPECT_EQ(setup.owner, k_Owner);
	EXPECT_EQ(setup.duration, 600U);
	EXPECT_TRUE(setup.madeByScript);
	EXPECT_FLOAT_EQ(setup.angle, 1.25f);
	EXPECT_FALSE(world_import::SetupOf({}, entt::null, false).madeByScript);
}
