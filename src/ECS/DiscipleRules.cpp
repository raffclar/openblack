/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DiscipleRules.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>
#include <numeric>

#include "Common/GUtilsDistance.h"
#include "Gui/ScreenFit.h"

using namespace openblack;
using namespace openblack::disciple;

namespace
{
using enum VillagerDisciple;

/// The game's table of disciples, by kind
constexpr std::array<Kind, 13> k_Kinds {{
    /* None */ {},
    /* Farmer */
    {.leadsFollowers = true, .glows = true, .waitsWhenIdle = true, .desire = TownDesireInfo::ForFood, .movesIntoTown = true},
    /* Forester */
    {.landingState = VillagerStates::ForesterArrivesAtForest,
     .leadsFollowers = true,
     .glows = true,
     .waitsWhenIdle = true,
     .desire = TownDesireInfo::ForWood,
     .goesForAnyWood = true},
    /* Fisherman */
    {.leadsFollowers = true, .glows = true, .waitsWhenIdle = true, .desire = TownDesireInfo::ForFood},
    /* Builder */
    {.leadsFollowers = true,
     .glows = true,
     .waitsWhenIdle = true,
     .desire = TownDesireInfo::ToBuild,
     .movesIntoTown = true,
     .goesForAnyWood = true},
    /* Breeder */
    {.landingState = VillagerStates::BreederJustLanded,
     .glows = true,
     .waitsWhenIdle = true,
     .desire = TownDesireInfo::ForChildren,
     .movesIntoTown = true},
    /* Protection: in the table, never made */
    {.leadsFollowers = true, .glows = true, .waitsWhenIdle = true},
    /* Missionary */
    {.landingState = VillagerStates::MissionaryDisciple, .glows = true},
    /* Craftsman */
    {.leadsFollowers = true, .glows = true, .waitsWhenIdle = true, .movesIntoTown = true, .goesForAnyWood = true},
    /* Trader */
    {.landingState = VillagerStates::DecideWhatToDo, .glows = true, .waitsWhenIdle = true},
    /* ChangeHouse */
    {.landingState = VillagerStates::DecideWhatToDo, .glows = true},
    /* Worship */
    {.landingState = VillagerStates::GotoWorshipSiteForWorship},
    /* FromVortex */
    {.landingState = VillagerStates::ScriptInCrowd},
}};

/// Any building of a town: preaching to another player's town, building up one's own
VillagerDisciple OfferedByBuilding(const NearFacts& near, const HeldFacts& held)
{
	if (near.hasTown && !near.townHabitable)
	{
		return None;
	}
	if (held.hand != held.villager)
	{
		return None;
	}
	if (near.owner != held.hand)
	{
		return near.hasTown ? Missionary : None;
	}
	return !near.built || !near.repaired ? Builder : None;
}

/// A building of the abodes: as any building, else one of the player's own houses with room takes in a villager of
/// another home
VillagerDisciple OfferedByAbode(const NearFacts& near, const HeldFacts& held)
{
	const auto offered = OfferedByBuilding(near, held);
	if (offered == None && near.kind == NearKind::House && near.functional && near.owner == held.hand && near.roomForAdults &&
	    !near.heldVillagersHome && near.hasTown && near.townHabitable)
	{
		return ChangeHouse;
	}
	return offered;
}
} // namespace

float disciple::Interest(float edgeGap, float kindDesire)
{
	return gutils::GetDistanceModifier(edgeGap > 0.0f ? edgeGap : 0.0f, k_InterestReach) * kindDesire;
}

std::vector<size_t> disciple::OrderByInterest(std::span<const float> interests)
{
	std::vector<size_t> order;
	order.reserve(interests.size());
	for (size_t i = 0; i < interests.size(); ++i)
	{
		if (interests[i] > k_LeastInterest)
		{
			order.push_back(i);
		}
	}
	std::ranges::stable_sort(order, [interests](size_t a, size_t b) { return interests[a] > interests[b]; });
	return order;
}

VillagerDisciple disciple::Offered(const NearFacts& near, const HeldFacts& held)
{
	const bool ownVillager = held.hand == held.villager;
	switch (near.kind)
	{
	case NearKind::Building:
	case NearKind::House:
		return OfferedByAbode(near, held);
	case NearKind::StoragePit:
	{
		// Another player's storage pit is traded with rather than preached to
		const auto offered = OfferedByAbode(near, held);
		return offered == Missionary ? Trader : offered;
	}
	case NearKind::TownCentre:
	{
		// The centre of a town other than its own takes it in, when the town has room
		const auto offered = OfferedByAbode(near, held);
		if (offered != Missionary && offered != Builder && !near.heldVillagersTown)
		{
			return near.townHasRoom ? ChangeHouse : None;
		}
		return offered;
	}
	case NearKind::Workshop:
	{
		const auto offered = OfferedByAbode(near, held);
		if (offered == None && ownVillager && near.hasTown && near.townHabitable)
		{
			return Craftsman;
		}
		return offered;
	}
	case NearKind::WorshipSite:
	{
		const auto offered = OfferedByBuilding(near, held);
		return offered != Builder && near.heldVillagersWorshipSite ? Worship : offered;
	}
	case NearKind::Field:
		// Any field, whoever's, though one not yet built up is built
		if (!ownVillager)
		{
			return None;
		}
		return near.built ? Farmer : Builder;
	case NearKind::Tree:
	case NearKind::Forest:
		return ownVillager ? Forester : None;
	case NearKind::FishFarm:
		return ownVillager ? Fisherman : None;
	case NearKind::Villager:
		// One of the player's own of an age to make love, by one of the other sex
		if (near.villagerFunctional && near.owner == held.hand && held.villager == near.owner && held.sexuallyActive &&
		    near.villagerSexuallyActive && held.male != near.villagerMale)
		{
			return Breeder;
		}
		return None;
	}
	return None;
}

Choice disciple::ChooseWhileHeld(std::span<const NearFacts> ordered, const HeldFacts& held, VillagerDisciple atPickUp)
{
	const bool refused = held.child || held.otherwiseRefused;
	for (size_t i = 0; i < ordered.size(); ++i)
	{
		const auto offered = Offered(ordered[i], held);
		if (offered == None)
		{
			continue;
		}
		if (refused && !(offered == Worship && held.child))
		{
			continue;
		}
		return {.type = offered, .from = i};
	}
	return {.type = atPickUp, .from = std::nullopt};
}

VillagerDisciple disciple::AtPickUp(VillagerDisciple current)
{
	return current == Missionary ? None : current;
}

std::optional<VillagerDisciple> disciple::FromScript(int32_t number)
{
	if (number < static_cast<int32_t>(VillagerDisciple::None) || number > static_cast<int32_t>(VillagerDisciple::FromVortex))
	{
		return std::nullopt;
	}
	return static_cast<VillagerDisciple>(number);
}

Kind disciple::KindOf(VillagerDisciple type)
{
	const auto index = static_cast<size_t>(type);
	return index < k_Kinds.size() ? k_Kinds.at(index) : Kind {};
}

std::optional<uint32_t> disciple::SymbolCell(VillagerDisciple type)
{
	// The symbols are the fifth row of the sheet and the start of the sixth
	constexpr uint32_t k_FirstCell = 32;
	switch (type)
	{
	case Missionary:
		return k_FirstCell + 0;
	case Farmer:
		return k_FirstCell + 1;
	case Breeder:
		return k_FirstCell + 2;
	case Forester:
		return k_FirstCell + 3;
	case Builder:
		return k_FirstCell + 4;
	case Protection:
	case Trader:
		return k_FirstCell + 5;
	case Fisherman:
		return k_FirstCell + 6;
	case Craftsman:
		return k_FirstCell + 7;
	case ChangeHouse:
		return k_FirstCell + 8;
	default:
		return std::nullopt;
	}
}

float disciple::SymbolSize(float distance, int32_t screenWidth, int32_t screenHeight)
{
	// The game's symbol is 16 pixels across half the screen's width, at half that size
	constexpr float k_SymbolPixels = 16.0f;
	constexpr float k_LeastSize = 0.0001f;
	// The camera shows the same view across a 4:3 screen, or the 4:3 middle of a wider one, as the game does across its
	// 4:3 screen, so the symbol's pixels are measured against the half of that middle's width, and grown with it
	const float scale = gui::ReferenceScale(screenWidth, screenHeight);
	const float halfWidth = std::max(scale * static_cast<float>(gui::k_ReferenceWidth) * 0.5f, 1.0f);
	return std::max(distance * (k_SymbolPixels * scale / halfWidth) * 0.5f, k_LeastSize);
}

std::optional<uint32_t> disciple::MadeSample(VillagerDisciple type, float playerAlignment, float chance)
{
	// A player leaning past these is evil or good to the guidance
	constexpr float k_EvilBelow = -0.55f;
	constexpr float k_Even = 0.5f;
	switch (type)
	{
	case Farmer:
		return 6;
	case Craftsman:
		return 7;
	case Forester:
		return 8;
	case Trader:
		return 9;
	case Builder:
		return 10;
	case Fisherman:
		return 11;
	case Breeder:
		return 12;
	case Missionary:
		return 13;
	case ChangeHouse:
		// The good and the neutral are said it the same way
		if (playerAlignment < k_EvilBelow)
		{
			return chance <= k_Even ? 16u : 15u;
		}
		return chance <= k_Even ? 18u : 17u;
	default:
		return std::nullopt;
	}
}

float disciple::AlignmentForMaking(std::optional<float> townDesire, float evilBelow, float goodAbove, float change)
{
	if (!townDesire.has_value())
	{
		return 0.0f;
	}
	if (*townDesire > goodAbove)
	{
		return change;
	}
	return *townDesire < evilBelow ? -change : 0.0f;
}

float disciple::DampedAlignment(float playerAlignment, float change)
{
	const float lean = std::abs(playerAlignment * 0.5f);
	const bool sameWay = (playerAlignment >= 0.0f) == (change >= 0.0f);
	return (sameWay ? 1.0f - lean : 1.0f + lean) * change;
}

WaitStep disciple::WaitStepFromCentre(float towardsDisciple, float centreRadius, const std::function<float(float)>& random)
{
	constexpr float k_Spread = std::numbers::pi_v<float> * 0.5f;
	constexpr float k_Beyond = 4.0f;
	const float turn = random(k_Spread) - (k_Spread * 0.5f);
	const float beyond = random(k_Beyond);
	return {.radians = towardsDisciple + turn, .metres = centreRadius + beyond};
}
