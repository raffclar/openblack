/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

/// The testbed scenarios of the computer gods, in one file for each part of the mind. The lands that more than one
/// part uses are here.
namespace openblack::testbed_scenarios
{

/// The lines of a land that give player two a computer mind, a temple and a Celtic town that offers the food miracle,
/// with an empty storage pit and eight abodes of people
inline constexpr std::string_view k_ComputerGodTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "FOOD")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(900, "40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "60.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("56.0,62.0", "60.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("56.0,62.0", "60.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("56.0,62.0", "60.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("56.0,62.0", "60.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "40.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("36.0,82.0", "40.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,82.0", "40.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("36.0,82.0", "40.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,82.0", "40.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "60.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("56.0,82.0", "60.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("56.0,82.0", "60.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("56.0,82.0", "60.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("56.0,82.0", "60.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-60.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-64.0,62.0", "-60.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-64.0,62.0", "-60.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-64.0,62.0", "-60.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-64.0,62.0", "-60.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-40.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-44.0,82.0", "-40.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,82.0", "-40.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-44.0,82.0", "-40.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,82.0", "-40.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-60.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-64.0,82.0", "-60.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-64.0,82.0", "-60.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-64.0,82.0", "-60.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-64.0,82.0", "-60.0,80.0", "CELTIC_FORESTER", 20)
)";

/// The same town 260 metres further east, so that its storage pit is far from the temple and its worship site
inline constexpr std::string_view k_ComputerGodFarTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "260.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
CREATE_NEW_TOWN_SPELL(900, "FOOD")
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "256.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "270.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(900, "300.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("296.0,62.0", "300.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("296.0,62.0", "300.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("296.0,62.0", "300.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("296.0,62.0", "300.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "320.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("316.0,62.0", "320.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("316.0,62.0", "320.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("316.0,62.0", "320.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("316.0,62.0", "320.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "300.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("296.0,82.0", "300.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("296.0,82.0", "300.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("296.0,82.0", "300.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("296.0,82.0", "300.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "320.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("316.0,82.0", "320.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("316.0,82.0", "320.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("316.0,82.0", "320.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("316.0,82.0", "320.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "220.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("216.0,62.0", "220.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("216.0,62.0", "220.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("216.0,62.0", "220.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("216.0,62.0", "220.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "200.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,62.0", "200.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "220.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("216.0,82.0", "220.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("216.0,82.0", "220.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("216.0,82.0", "220.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("216.0,82.0", "220.0,80.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "200.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("196.0,82.0", "200.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,82.0", "200.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("196.0,82.0", "200.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("196.0,82.0", "200.0,80.0", "CELTIC_FORESTER", 20)
)";

/// The lines of a land that give player two a computer mind, a temple and a Celtic town by the lake, with an empty
/// storage pit and two abodes of people. A fish farm of the town stands on the shore.
inline constexpr std::string_view k_ComputerGodFishFarmTownScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "0.0,100.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,101.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "30.0,115.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(900, "-30.0,90.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-34.0,92.0", "-30.0,90.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-34.0,92.0", "-30.0,90.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-34.0,92.0", "-30.0,90.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-34.0,92.0", "-30.0,90.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-30.0,70.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-34.0,72.0", "-30.0,70.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-34.0,72.0", "-30.0,70.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-34.0,72.0", "-30.0,70.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-34.0,72.0", "-30.0,70.0", "CELTIC_FORESTER", 20)
)";

/// The lines of a land that give player two a computer mind, a temple and two Celtic towns. The east town has no food
/// in its storage pit. The west town, 160 metres away, has 5000 food and 5000 wood in its pit.
inline constexpr std::string_view k_ComputerGodTwoTownsFoodScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_TWO", 5690, 1000)
CREATE_TOWN(900, "80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(900, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(900, "76.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "90.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 0, 3000)
CREATE_ABODE(900, "110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,62.0", "110.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "110.0,80.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("106.0,82.0", "110.0,80.0", "CELTIC_FORESTER", 20)
CREATE_TOWN(901, "-80.0,40.0", "PLAYER_TWO", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(901, "PLAYER_TWO", 2.0)
SET_TOWN_BELIEF(901, "PLAYER_TWO", 1.0)
CREATE_TOWN_CENTRE(901, "-84.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(901, "-70.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 5000, 5000)
CREATE_ABODE(901, "-110.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-114.0,62.0", "-110.0,60.0", "CELTIC_FORESTER", 20)
)";

/// At the start, the god of player two gets a weight for each node that has the name, as a personality of the god
/// gives it
inline Command ComputerWeight(std::string_view node, float weight)
{
	return {.kind = Command::Kind::ComputerPersonality, .player = PlayerNames::PLAYER_TWO, .action = node, .amount = weight};
}

/// The scenarios of the start of a computer god and the hand's own actions
void AddComputerGodCoreScenarios(std::vector<Scenario>& all);
/// The scenarios of the food and the wood of the towns
void AddComputerGodTownsScenarios(std::vector<Scenario>& all);
/// The scenarios of the worshippers, the building sites and the people of the towns
void AddComputerGodWorshipScenarios(std::vector<Scenario>& all);
/// The scenarios of the workshops and the scaffolds of the god
void AddComputerGodScaffoldScenarios(std::vector<Scenario>& all);
/// The scenarios of the choice, the charge and the cast of the miracles
void AddComputerGodMiraclesScenarios(std::vector<Scenario>& all);
/// The scenarios of the care of the god's creature
void AddComputerGodCreatureCareScenarios(std::vector<Scenario>& all);
/// The scenarios of the play with the god's creature and its lessons
void AddComputerGodCreaturePlayScenarios(std::vector<Scenario>& all);
/// The scenarios of the towns that the god takes
void AddComputerGodConquestScenarios(std::vector<Scenario>& all);
/// The scenarios of the attacks on other players
void AddComputerGodWarScenarios(std::vector<Scenario>& all);
/// The scenarios of the defence of the god's towns
void AddComputerGodDefenceScenarios(std::vector<Scenario>& all);
/// The scenarios of what the god thinks of each player
void AddComputerGodAttitudesScenarios(std::vector<Scenario>& all);

} // namespace openblack::testbed_scenarios
