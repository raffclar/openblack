/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Scaffold.h"
#include "ECS/ScaffoldOfferRules.h"
#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::scaffold_offer;

namespace
{

ScaffoldOffer Offer(AbodeNumber number, uint32_t mesh, float scale = 1.0f)
{
	return {.info = AbodeInfo::None, .number = number, .mesh = static_cast<MeshId>(mesh), .yAngle = 0.0f, .scale = scale};
}

Candidate Want(AbodeNumber number, uint32_t mesh, uint32_t required, float desire)
{
	return {.offer = Offer(number, mesh), .scaffoldsRequired = required, .desire = desire};
}

} // namespace

TEST(ScaffoldOfferRules, NearWorkshopNeedsTheWorkshopNearAndTheFlagOrAPlace)
{
	EXPECT_TRUE(NearWorkshop({.workshop = 50.0f, .flag = 9.9f, .places = {20.0f, 20.0f, 20.0f}}));
	EXPECT_FALSE(NearWorkshop({.workshop = 50.1f, .flag = 1.0f, .places = {1.0f, 1.0f, 1.0f}}));
	EXPECT_FALSE(NearWorkshop({.workshop = 10.0f, .flag = 10.0f, .places = {5.0f, 5.0f, 5.0f}}));
	EXPECT_TRUE(NearWorkshop({.workshop = 10.0f, .flag = 30.0f, .places = {9.0f, 9.0f, 4.9f}}));
}

TEST(ScaffoldOfferRules, BestAtTurnTakesTheFirstOfTheMostWanted)
{
	const std::array candidates {
	    Want(AbodeNumber::A, 1, 1, 0.5f), Want(AbodeNumber::B, 2, 1, 0.7f),
	    Want(AbodeNumber::C, 3, 1, 0.7f), Want(AbodeNumber::D, 4, 3, 0.9f), // needs more than the value
	    Want(AbodeNumber::E, 5, 0, 1.0f),                                   // needs no scaffold
	};
	const auto best = BestAtTurn(candidates, 2, 0.5f, {});
	ASSERT_TRUE(best.has_value());
	EXPECT_EQ(best->number, AbodeNumber::B);
	EXPECT_EQ(best->yAngle, 0.5f);
}

TEST(ScaffoldOfferRules, BestAtTurnSkipsWhatDoesNotFitAndWhatNobodyWants)
{
	const std::array candidates {Want(AbodeNumber::A, 1, 1, 0.0f), Want(AbodeNumber::B, 2, 1, 0.4f)};
	EXPECT_FALSE(BestAtTurn(candidates, 1, 0.0f, [](const ScaffoldOffer&, float) { return false; }).has_value());
	const std::array nobody {Want(AbodeNumber::A, 1, 1, 0.0f)};
	EXPECT_FALSE(BestAtTurn(nobody, 1, 0.0f, {}).has_value());
}

TEST(ScaffoldOfferRules, SearchTownTriesEightTurnsAndStopsAtTheFirstOffer)
{
	const std::array candidates {Want(AbodeNumber::A, 1, 1, 0.5f)};
	std::vector<float> tried;
	const auto fitsThird = [&tried](const ScaffoldOffer&, float yAngle) {
		tried.push_back(yAngle);
		return tried.size() == 3;
	};
	const auto offer = SearchTown(candidates, 1, 1.0f, fitsThird);
	ASSERT_TRUE(offer.has_value());
	ASSERT_EQ(tried.size(), 3U);
	float expected = 1.0f;
	expected += k_TryStep;
	EXPECT_EQ(tried[1], expected);
	expected += k_TryStep;
	EXPECT_EQ(offer->yAngle, expected);

	tried.clear();
	EXPECT_FALSE(SearchTown(candidates, 1, 0.0f, [&tried](const ScaffoldOffer&, float yAngle) {
		             tried.push_back(yAngle);
		             return false;
	             }).has_value());
	EXPECT_EQ(tried.size(), k_Tries);
}

TEST(ScaffoldOfferRules, SearchNewTownStartsAtNoTurnWithFullScale)
{
	auto centre = Want(AbodeNumber::TownCentre, 9, 4, 0.0f);
	centre.offer.scale = 2.0f;
	EXPECT_FALSE(SearchNewTown(centre, 3, {}).has_value());
	const auto offer = SearchNewTown(centre, 4, {});
	ASSERT_TRUE(offer.has_value());
	EXPECT_EQ(offer->yAngle, 0.0f);
	EXPECT_EQ(offer->scale, 1.0f);
	int calls = 0;
	const auto second = SearchNewTown(centre, 4, [&calls](const ScaffoldOffer&, float) { return ++calls == 2; });
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->yAngle, k_TryStep);
}

TEST(ScaffoldOfferRules, StepTurnTurnsThePhantomAndWrapsAtAFullTurn)
{
	ScaffoldPhantom phantom {.yAngle = 6.2f};
	StepTurn(phantom, 0.1f);
	float expected = 6.2f;
	expected += 0.1f;
	expected -= k_FullTurn;
	EXPECT_EQ(phantom.yAngle, expected);
	phantom.fixedAngle = true;
	StepTurn(phantom, 0.1f);
	EXPECT_EQ(phantom.yAngle, expected);
}

TEST(ScaffoldOfferRules, StepTurnShowsTheNextOfferWhenThePhantomIsSmall)
{
	ScaffoldPhantom phantom {.changeMs = 150};
	phantom.next = Offer(AbodeNumber::A, 1, 1.5f);
	phantom.next->yAngle = 2.0f;
	StepTurn(phantom, 0.0f);
	EXPECT_EQ(phantom.changeMs, 50);
	EXPECT_FALSE(phantom.shown.has_value());
	StepTurn(phantom, 0.0f);
	ASSERT_TRUE(phantom.shown.has_value());
	EXPECT_FALSE(phantom.next.has_value());
	EXPECT_TRUE(phantom.grows);
	EXPECT_EQ(phantom.changeMs, k_ChangeMs);
	EXPECT_EQ(phantom.scale, 1.5f);
	EXPECT_EQ(phantom.yAngle, 2.0f);
}

TEST(ScaffoldOfferRules, StepTurnCountsTheTurnsSinceNoOfferShowed)
{
	ScaffoldPhantom phantom {.noOfferTurns = 1};
	for (int turn = 0; turn < 14; ++turn)
	{
		StepTurn(phantom, 0.0f);
	}
	EXPECT_EQ(phantom.noOfferTurns, 15);
	StepTurn(phantom, 0.0f);
	EXPECT_EQ(phantom.noOfferTurns, 0);
	StepTurn(phantom, 0.0f);
	EXPECT_EQ(phantom.noOfferTurns, 0);
}

TEST(ScaffoldOfferRules, TakeNextOfferShowsAtOnceWithNoOfferShown)
{
	ScaffoldPhantom phantom {.yAngle = 1.0f};
	phantom.next = Offer(AbodeNumber::B, 2, 0.8f);
	phantom.next->yAngle = 3.0f;
	TakeNextOffer(phantom);
	ASSERT_TRUE(phantom.shown.has_value());
	EXPECT_TRUE(phantom.grows);
	EXPECT_EQ(phantom.changeMs, k_ChangeMs);
	EXPECT_EQ(phantom.scale, 0.8f);
	EXPECT_EQ(phantom.yAngle, 1.0f);
}

TEST(ScaffoldOfferRules, TakeNextOfferWithTheSameModelKeepsThePhantom)
{
	ScaffoldPhantom phantom {.changeMs = 200, .grows = true, .scale = 1.0f};
	phantom.shown = Offer(AbodeNumber::A, 7, 1.0f);
	phantom.next = Offer(AbodeNumber::A, 7, 2.0f);
	TakeNextOffer(phantom);
	EXPECT_EQ(phantom.shown->scale, 2.0f);
	EXPECT_EQ(phantom.scale, 1.0f);
	EXPECT_EQ(phantom.changeMs, 200);
	EXPECT_TRUE(phantom.grows);

	phantom.next = Offer(AbodeNumber::Wonder, 7, 3.0f);
	TakeNextOffer(phantom);
	EXPECT_EQ(phantom.scale, 3.0f);
}

TEST(ScaffoldOfferRules, TakeNextOfferOfAnotherModelMakesThePhantomSmallFirst)
{
	ScaffoldPhantom phantom {.changeMs = 100, .grows = true, .foundsTown = true};
	phantom.shown = Offer(AbodeNumber::TownCentre, 1);
	phantom.next = Offer(AbodeNumber::A, 2);
	TakeNextOffer(phantom);
	EXPECT_FALSE(phantom.grows);
	EXPECT_EQ(phantom.changeMs, k_ChangeMs - 100);
	EXPECT_FALSE(phantom.foundsTown);
	ASSERT_TRUE(phantom.next.has_value());
	// The next offer shows when the phantom is small
	for (int turn = 0; turn < 3; ++turn)
	{
		StepTurn(phantom, 0.0f);
	}
	EXPECT_EQ(phantom.shown->number, AbodeNumber::A);
	EXPECT_TRUE(phantom.grows);
}

TEST(ScaffoldOfferRules, TakeNextOfferOfNoOfferHidesThePhantom)
{
	ScaffoldPhantom phantom {.changeMs = 0, .grows = true, .foundsTown = true};
	phantom.shown = Offer(AbodeNumber::TownCentre, 1);
	TakeNextOffer(phantom);
	EXPECT_FALSE(phantom.foundsTown);
	EXPECT_FALSE(phantom.grows);
	EXPECT_EQ(phantom.changeMs, k_ChangeMs);
	for (int turn = 0; turn < 4; ++turn)
	{
		StepTurn(phantom, 0.0f);
	}
	EXPECT_FALSE(phantom.shown.has_value());
}

TEST(ScaffoldOfferRules, TakeNextOfferKeepsTheFoundingOfANewCentre)
{
	ScaffoldPhantom phantom {.grows = true, .foundsTown = true};
	phantom.shown = Offer(AbodeNumber::TownCentre, 1);
	phantom.next = Offer(AbodeNumber::TownCentre, 2);
	TakeNextOffer(phantom);
	EXPECT_TRUE(phantom.foundsTown);
}

TEST(ScaffoldOfferRules, HeldTurnLooksOnceWhileTheHandStaysStill)
{
	ScaffoldPhantom phantom;
	int searches = 0;
	const Search search = [&searches]() -> std::optional<Found> {
		++searches;
		return Found {.offer = Offer(AbodeNumber::A, 1)};
	};
	EXPECT_EQ(HeldTurn(phantom, {.handStill = true}, search, {}), HeldEvent::None);
	EXPECT_EQ(searches, 1);
	ASSERT_TRUE(phantom.shown.has_value());
	EXPECT_TRUE(phantom.looked);
	EXPECT_EQ(HeldTurn(phantom, {.handStill = true}, search, {}), HeldEvent::None);
	EXPECT_EQ(searches, 1);
}

TEST(ScaffoldOfferRules, HeldTurnShowsNoOfferOnceAndLooksAgainEachTurn)
{
	ScaffoldPhantom phantom;
	int searches = 0;
	const Search none = [&searches]() -> std::optional<Found> {
		++searches;
		return std::nullopt;
	};
	EXPECT_EQ(HeldTurn(phantom, {.handStill = true}, none, {}), HeldEvent::ShowsNoOffer);
	EXPECT_EQ(phantom.noOfferTurns, 1);
	EXPECT_EQ(HeldTurn(phantom, {.handStill = true}, none, {}), HeldEvent::None);
	EXPECT_EQ(searches, 2);
	// The hand moves and stops again before the count is complete: no puff
	EXPECT_EQ(HeldTurn(phantom, {.handStill = false}, none, {}), HeldEvent::None);
	EXPECT_FALSE(phantom.looked);
	EXPECT_EQ(HeldTurn(phantom, {.handStill = true}, none, {}), HeldEvent::None);
}

TEST(ScaffoldOfferRules, HeldTurnFoundingFromTheSearch)
{
	ScaffoldPhantom phantom;
	const Search centre = []() -> std::optional<Found> {
		return Found {.offer = Offer(AbodeNumber::TownCentre, 1), .foundsTown = true};
	};
	(void)HeldTurn(phantom, {.handStill = true}, centre, {});
	EXPECT_TRUE(phantom.foundsTown);
}

TEST(ScaffoldOfferRules, HeldTurnWhileTheHandMovesKeepsOrDropsTheOffer)
{
	ScaffoldPhantom phantom {.changeMs = 0, .grows = true};
	phantom.shown = Offer(AbodeNumber::A, 1);
	(void)HeldTurn(phantom, {.handStill = false}, {}, [](const ScaffoldPhantom&) { return true; });
	EXPECT_TRUE(phantom.grows);
	(void)HeldTurn(phantom, {.handStill = false}, {}, [](const ScaffoldPhantom&) { return false; });
	EXPECT_FALSE(phantom.grows);
	EXPECT_EQ(phantom.changeMs, k_ChangeMs);
}

TEST(ScaffoldOfferRules, FindFirstTakesTheFirstRowOfTheTribeOrOfNoTribe)
{
	std::vector<GAbodeInfo> rows(4);
	rows[0].tribeType = Tribe::NORSE;
	rows[0].abodeNumber = AbodeNumber::A;
	rows[1].tribeType = Tribe::NONE;
	rows[1].abodeNumber = AbodeNumber::Wonder;
	rows[2].tribeType = Tribe::CELTIC;
	rows[2].abodeNumber = AbodeNumber::A;
	rows[3].tribeType = Tribe::CELTIC;
	rows[3].abodeNumber = AbodeNumber::A;
	EXPECT_EQ(GAbodeInfo::FindFirst(rows, Tribe::CELTIC, AbodeNumber::A), static_cast<AbodeInfo>(2));
	EXPECT_EQ(GAbodeInfo::FindFirst(rows, Tribe::CELTIC, AbodeNumber::Wonder), static_cast<AbodeInfo>(1));
	EXPECT_FALSE(GAbodeInfo::FindFirst(rows, Tribe::CELTIC, AbodeNumber::B).has_value());
}
