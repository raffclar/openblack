/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Temple/TempleHelp.h"

using namespace openblack;
using namespace openblack::temple_help;

namespace
{
/// Records what the temple asks of the scripts, in order
class FakeScripts final: public Scripts
{
public:
	void Start(std::string_view name) override { calls.push_back("start " + std::string(name)); }
	void StopHelp() override { calls.emplace_back("stop help"); }
	void Stop(std::string_view name) override { calls.push_back("stop " + std::string(name)); }

	std::vector<std::string> calls;
};
} // namespace

TEST(TempleHelp, EnteringARoomStopsTheHelpThenStartsTheRoomsOwn)
{
	FakeScripts scripts;
	EnterRoom(scripts, TempleRoom::CreatureCave, true);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help", "start CitadelCreatureRoomHelp"}));
}

TEST(TempleHelp, WithTheHelpSystemOffARoomOnlyStopsTheHelp)
{
	FakeScripts scripts;
	EnterRoom(scripts, TempleRoom::Main, false);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help"}));
}

TEST(TempleHelp, EachRoomHasItsHelpAndTheOptionsRoomSharesTheMainRooms)
{
	EXPECT_EQ(RoomHelp(TempleRoom::Main), "CitadelWorldRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::Options), "CitadelWorldRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::CreatureCave), "CitadelCreatureRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::Challenge), "CitadelChallengeRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::SaveGame), "CitadelSaveGameRoomHelp");
	EXPECT_EQ(RoomHelp(TempleRoom::Credits), "CitadelCreditsRoomHelp");
	EXPECT_FALSE(RoomHelp(TempleRoom::Multi).has_value());

	FakeScripts scripts;
	EnterRoom(scripts, TempleRoom::Multi, true);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help"}));
}

TEST(TempleHelp, LookingAtAScrollStartsItsHelpWhetherOrNotTheHelpSystemIsOn)
{
	using Content = TempleScrolls::Content;
	FakeScripts scripts;
	LookAtScroll(scripts, Content::World);
	LookAtScroll(scripts, Content::CreatureAttributes);
	LookAtScroll(scripts, Content::CreatureActions);
	LookAtScroll(scripts, Content::CreatureMind);
	LookAtScroll(scripts, Content::CreatureMiracles);
	LookAtScroll(scripts, Content::Challenge);
	LookAtScroll(scripts, Content::SaveGame);
	LookAtScroll(scripts, Content::LibraryStaff);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {
	                             "start CitadelWorldRoomScrollHelp",
	                             "start CitadelCreatureRoomAttributesScroll",
	                             "start CitadelCreatureRoomActionsLearntScroll",
	                             "start CitadelCreatureRoomLikesScroll",
	                             "start CitadelCreatureRoomMagicScroll",
	                             "start CitadelChallengeRoomScrollHelp",
	                             "start CitadelSaveGameRoomScrollHelp",
	                         }));
}

TEST(TempleHelp, LeavingAScrollStopsTheScrollHelpOfTheRoomsCamera)
{
	FakeScripts scripts;
	LeaveScroll(scripts, TempleRoom::Main);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop CitadelWorldRoomScrollHelp"}));

	scripts.calls.clear();
	LeaveScroll(scripts, TempleRoom::SaveGame);
	EXPECT_EQ(scripts.calls,
	          (std::vector<std::string> {"stop CitadelChallengeRoomScrollHelp", "stop CitadelSaveGameRoomScrollHelp"}));

	scripts.calls.clear();
	LeaveScroll(scripts, TempleRoom::CreatureCave);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {
	                             "stop CitadelCreatureRoomAttributesScroll",
	                             "stop CitadelCreatureRoomLikesScroll",
	                             "stop CitadelCreatureRoomMagicScroll",
	                             "stop CitadelCreatureRoomActionsLearntScroll",
	                         }));

	scripts.calls.clear();
	LeaveScroll(scripts, TempleRoom::Credits);
	EXPECT_TRUE(scripts.calls.empty());
}

TEST(TempleHelp, OnlyTheBeltsAndMedalsOfTheCreaturesRoomStartHelp)
{
	using CreatureCaveTargets::Target;
	FakeScripts scripts;
	for (const auto target : {Target::Creature, Target::Belts, Target::Medals, Target::Fourth, Target::Exit})
	{
		ZoomToCaveTarget(scripts, target);
	}
	EXPECT_EQ(scripts.calls,
	          (std::vector<std::string> {"start CitadelCreatureRoomAttackDummies", "start CitadelCreatureRoomMagicPlinths"}));
}

TEST(TempleHelp, LeavingTheTempleStopsTheHelp)
{
	FakeScripts scripts;
	Leave(scripts);
	EXPECT_EQ(scripts.calls, (std::vector<std::string> {"stop help"}));
}

TEST(TempleHelp, ATurnIsDueEachTenthOfASecondAfterTheFirstAsked)
{
	std::optional<uint32_t> last;
	EXPECT_FALSE(TurnDue(last, 5000));
	EXPECT_FALSE(TurnDue(last, 5100));
	EXPECT_TRUE(TurnDue(last, 5101));
	EXPECT_EQ(last, 5100u);
	EXPECT_FALSE(TurnDue(last, 5200));
	EXPECT_TRUE(TurnDue(last, 5250));
	EXPECT_EQ(last, 5200u);
}

TEST(TempleHelp, ALateTurnIsCaughtUpOneAFrameUnlessFarBehind)
{
	std::optional<uint32_t> last = 1000u;
	// 250 ms late: one turn now, and the next on the following frame
	EXPECT_TRUE(TurnDue(last, 1250));
	EXPECT_EQ(last, 1100u);
	EXPECT_TRUE(TurnDue(last, 1250));
	EXPECT_EQ(last, 1200u);
	EXPECT_FALSE(TurnDue(last, 1250));

	// More than two turns behind after one: the clock starts again from now
	last = 1000u;
	EXPECT_TRUE(TurnDue(last, 1400));
	EXPECT_EQ(last, 1400u);
	EXPECT_FALSE(TurnDue(last, 1450));
}
