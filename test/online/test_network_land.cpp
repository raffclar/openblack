/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include <Online/NetworkLand.h>
#include <gtest/gtest.h>

using namespace openblack::network_land;
using openblack::game_socket::Start;

namespace
{

Start ScriptStart(std::string land, uint32_t seed, std::string opening)
{
	return Start {.land = std::move(land),
	              .testbed = false,
	              .seed = seed,
	              .turn = 0,
	              .opening = std::move(opening),
	              .godsRingMap = false};
}

} // namespace

TEST(NetworkLand, TheNameOfALandIsItsPathUnderTheScriptsFolder)
{
	EXPECT_EQ(WireName("C:/black/Scripts/Land1.txt"), "Land1.txt");
	EXPECT_EQ(WireName("C:/black/Scripts/Playgrounds/construct.txt"), "Playgrounds/construct.txt");
	EXPECT_EQ(WireName("/games/black/scripts/Land2.txt"), "Land2.txt");
	// The last Scripts folder of the path counts
	EXPECT_EQ(WireName("D:/Scripts/black/Scripts/Land3.txt"), "Land3.txt");
	// A land outside a Scripts folder goes by the name of its file
	EXPECT_EQ(WireName("E:/maps/mine.txt"), "mine.txt");
	EXPECT_EQ(WireName("Land1.txt"), "Land1.txt");
}

TEST(NetworkLand, AClientLoadsOnlyALandUnderItsScriptsFolder)
{
	const std::filesystem::path scripts("C:/black/Scripts");
	EXPECT_EQ(PathOf("Land1.txt", scripts), scripts / "Land1.txt");
	EXPECT_EQ(PathOf("Playgrounds/construct.txt", scripts), scripts / "Playgrounds/construct.txt");
	EXPECT_FALSE(PathOf("", scripts).has_value());
	EXPECT_FALSE(PathOf("../Data/x.txt", scripts).has_value());
	EXPECT_FALSE(PathOf("Playgrounds/../../x.txt", scripts).has_value());
	EXPECT_FALSE(PathOf("/etc/x.txt", scripts).has_value());
#if defined(_WIN32)
	EXPECT_FALSE(PathOf("C:/x.txt", scripts).has_value());
#endif
}

TEST(NetworkLand, ALoadedLandIsTheLandOfItsName)
{
	EXPECT_TRUE(IsLand("C:/black/Scripts/Land1.txt", "Land1.txt"));
	EXPECT_TRUE(IsLand("C:/black/Scripts/land1.txt", "Land1.txt"));
	EXPECT_TRUE(IsLand("C:/black/Scripts/Playgrounds/construct.txt", "Playgrounds/construct.txt"));
	EXPECT_FALSE(IsLand("C:/black/Scripts/Land2.txt", "Land1.txt"));
	EXPECT_FALSE(IsLand("C:/black/Scripts/Playgrounds/construct.txt", "construct.txt"));
}

TEST(NetworkLand, ALandOfANetworkGameGoesByItsPathInTheOnlineMapsFolder)
{
	// The server may have the map by a relative or an absolute path, in any case
	EXPECT_EQ(WireName("Online Maps/mpm_2p_1.map"), "Online Maps/mpm_2p_1.map");
	EXPECT_EQ(WireName("C:/black/Online Maps/mpm_2p_1.map"), "Online Maps/mpm_2p_1.map");
	EXPECT_EQ(WireName("/games/black/online maps/MPM_2P_1.MAP"), "Online Maps/MPM_2P_1.MAP");
	// A map that is not in the folder goes by the name of its file, in the folder
	EXPECT_EQ(WireName("E:/maps/mpm_2p_1.map"), "Online Maps/mpm_2p_1.map");
	// A script land of the Scripts folder keeps its name
	EXPECT_EQ(WireName("C:/black/Scripts/Playgrounds/TwoGods.txt"), "Playgrounds/TwoGods.txt");
}

TEST(NetworkLand, AClientFindsALandOfANetworkGameInItsOnlineMapsFolder)
{
	const std::filesystem::path scripts("C:/black/Scripts");
	EXPECT_EQ(PathOf("Online Maps/mpm_2p_1.map", scripts), std::filesystem::path("Online Maps/mpm_2p_1.map"));
	EXPECT_EQ(PathOf("online maps/mpm_2p_1.map", scripts), std::filesystem::path("Online Maps/mpm_2p_1.map"));
	// A map file anywhere else, or a path out of the folder, is not a land
	EXPECT_FALSE(PathOf("mpm_2p_1.map", scripts).has_value());
	EXPECT_FALSE(PathOf("Playgrounds/mpm_2p_1.map", scripts).has_value());
	EXPECT_FALSE(PathOf("Online Maps/../Scripts/x.map", scripts).has_value());
	EXPECT_FALSE(PathOf("/Online Maps/mpm_2p_1.map", scripts).has_value());
}

TEST(NetworkLand, TheServerAndTheClientNameALandOfANetworkGameTheSame)
{
	const auto wire = WireName("C:/black/Online Maps/mpm_2p_1.map");
	const std::filesystem::path scripts("C:/black/Scripts");
	const auto path = PathOf(wire, scripts);
	ASSERT_TRUE(path.has_value());
	// What the client loads is the land of the name that the server sent
	EXPECT_TRUE(IsLand(*path, wire));
	EXPECT_TRUE(IsLand("D:/other/Online Maps/MPM_2P_1.map", wire));
	EXPECT_FALSE(IsLand("D:/other/Online Maps/mpm_3p_1.map", wire));
	EXPECT_FALSE(IsLand("C:/black/Scripts/Land1.txt", wire));
	const Start start = ScriptStart(wire, 7, "");
	const OwnWorld own {.land = *path, .testbed = false, .godsRingMap = false, .seed = 7, .opening = std::nullopt};
	EXPECT_EQ(OwnValuesThatDiffer(own, start), "");
}

TEST(NetworkLand, TheLogNamesTheOwnValuesThatTheServerReplaces)
{
	const auto start = ScriptStart("Land1.txt", 7, "cow");
	const OwnWorld same {.land = "C:/black/Scripts/Land1.txt",
	                     .testbed = false,
	                     .godsRingMap = false,
	                     .seed = 7,
	                     .opening = openblack::new_game_choice::ParseNewGameStart("cow")};
	EXPECT_EQ(OwnValuesThatDiffer(same, start), "");
	auto other = same;
	other.land = "C:/black/Scripts/Land2.txt";
	other.seed = 3;
	other.opening = std::nullopt;
	EXPECT_EQ(OwnValuesThatDiffer(other, start), "land, seed, start");
	// The testbed of this computer is not the server's land
	auto testbed = same;
	testbed.testbed = true;
	EXPECT_EQ(OwnValuesThatDiffer(testbed, start), "land");
	// The server's map for 32 gods has no land script and no start
	const Start map {.land = {}, .testbed = false, .seed = 7, .turn = 0, .opening = {}, .godsRingMap = true};
	auto ownMap = same;
	ownMap.godsRingMap = true;
	ownMap.opening = std::nullopt;
	EXPECT_EQ(OwnValuesThatDiffer(ownMap, map), "");
	EXPECT_EQ(DisplayName(map), "the network map for 32 gods");
	EXPECT_EQ(DisplayName(start), "Land1.txt");
}
