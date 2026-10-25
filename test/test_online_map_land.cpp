/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Online/OnlineMapLand.h"

using namespace openblack;

TEST(OnlineMapLand, ALandOfANetworkGameEndsInMapInAnyCase)
{
	EXPECT_TRUE(online_map_land::IsOnlineMap("Online Maps/mpm_2p_1.map"));
	EXPECT_TRUE(online_map_land::IsOnlineMap(R"(Online Maps\MPM_2P_1.MAP)"));
	EXPECT_FALSE(online_map_land::IsOnlineMap("Scripts/Playgrounds/TwoGods.txt"));
	EXPECT_FALSE(online_map_land::IsOnlineMap(".map"));
	EXPECT_FALSE(online_map_land::IsOnlineMap("Online Maps/mpm_2p_1.thm"));
}

TEST(OnlineMapLand, TheScriptLoadsTheLandscapeOfTheMapsName)
{
	// The script of the first network land names its landscape in capitals, with back slashes
	EXPECT_TRUE(online_map_land::IsOwnLandscape("Online Maps/mpm_2p_1.map", R"(.\Online Maps\MPM_2P_1.lnd)"));
	EXPECT_TRUE(online_map_land::IsOwnLandscape("Online Maps/mpm_2p_1.map", "Online Maps/mpm_2p_1.lnd"));
	EXPECT_FALSE(online_map_land::IsOwnLandscape("Online Maps/mpm_2p_1.map", R"(.\Data\Landscape\Land1.lnd)"));
	EXPECT_FALSE(online_map_land::IsOwnLandscape("Online Maps/mpm_2p_1.map", R"(.\Online Maps\MPM_3P_1.lnd)"));
	// Any other land loads its landscape from the game's files
	EXPECT_FALSE(online_map_land::IsOwnLandscape("Scripts/Playgrounds/TwoGods.txt", "Data/Landscape/TwoGods.lnd"));
}
