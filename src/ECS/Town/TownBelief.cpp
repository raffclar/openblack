/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBelief.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Components/Town.h"
#include "ECS/Events/Publish.h"
#include "ECS/Events/TownBeliefEvents.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TownStateSystemInterface.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "Game/GameStats.h"
#include "GameClock.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Particles/Rules/SurfRevol.h"

// The town's belief (TownBelief.h). The original computes on stored 32-bit floats: the port keeps its order in float
// and uses double where the original keeps an unstored extended-precision intermediate (approximate: double is not
// 80-bit, the products of two floats are exact in both)

namespace openblack::ecs::town_belief
{
using components::Town;
using components::TownBelief;

namespace
{
/// The neutral player is always slot 7
constexpr PlayerNames k_Neutral = PlayerNames::NEUTRAL;
/// The local player. (inferred) PLAYER_ONE, as TownDesire's isLocalPlayer
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

/// The belief-sprite queue (Locator::townStateSystem)
std::vector<BeliefSprite>& BeliefSprites()
{
	if (!Locator::townStateSystem::has_value())
	{
		std::fputs("ecs::town_belief: no townStateSystem in the locator (Locator::townStateSystem)\n", stderr);
		std::abort();
	}
	return Locator::townStateSystem::value().BeliefSprites();
}

constexpr size_t Slot(PlayerNames player)
{
	return static_cast<size_t>(player);
}

constexpr bool InRange(PlayerNames player)
{
	return static_cast<size_t>(player) < k_Players;
}

Town* TownOf(entt::entity town)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

bool Valid(entt::entity object)
{
	return object != entt::null && Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(object);
}

/// f x 10000 truncated toward 0 (the product of a float and 10000 is exact in a double)
int32_t Amount(float f)
{
	return static_cast<int32_t>(static_cast<double>(f) * k_AmountScale);
}

/// The player's colour from psys::surf_revol's table (the land remap is not applied there either)
uint32_t PlayerColour(PlayerNames player)
{
	return psys::surf_revol::PlayerColour(static_cast<int>(player));
}

/// The object's position on the ground (map_coords::ToWorld of its MapCoords)
glm::vec3 GroundPoint(entt::entity object)
{
	return map_coords::ToWorld(object::MapCoordsOf(object));
}

void Help(detail::HelpSprite kind, entt::entity town)
{
	events::Publish(events::TownBeliefHelp {.kind = kind, .town = town});
}

/// The game's handler of TownBeliefHelp: the help sprite of the town
void PlayHelp(const events::TownBeliefHelp& event)
{
	const auto kind = event.kind;
	const auto town = event.town;
	switch (kind)
	{
	case detail::HelpSprite::LosingBelief:
		// Played when the help sprite may play now and the town has adults + children -> list 10
		if (const auto* t = TownOf(town); t != nullptr)
		{
			audio::guidance::WarnLosingBelief(t->stats.adults + t->stats.children);
		}
		break;
	case detail::HelpSprite::GeneralBad:
		audio::guidance::RemarkGeneralBad(GroundPoint(town)); // at the town's position
		break;
	case detail::HelpSprite::GeneralGood:
		audio::guidance::RemarkGeneralGood(GroundPoint(town)); // at the town's position
		break;
	}
}

/// ReduceBelief on a town component already found
void Reduce(Town& t, PlayerNames player, float r)
{
	if (!InRange(player))
	{
		return;
	}
	auto& b = t.belief;
	const size_t n = Slot(player);
	b.belief.at(n) -= r; // not clamped: the belief can go below 0
	if (Valid(t.centre))
	{
		b.reduceAccumulator.at(n) += r;
		if (static_cast<double>(b.reduceAccumulator.at(n)) > k_ReduceDrawThreshold)
		{
			// DrawBelief(-acc, town centre, P); a negative amount draws nothing
			DrawBelief(-b.reduceAccumulator.at(n), t.centre, player);
			b.reduceAccumulator.at(n) = 0.0f;
		}
	}
}

} // namespace

// ---- Small methods --------------------------------------------------------------------------------------------------

void ResetDesireThresholds(TownBelief& belief)
{
	// Each desire's desireAffectsBeliefAfter from the town desire info
	if (!Locator::infoConstants::has_value())
	{
		belief.desireThreshold.fill(0.0f); // (openblack) no info: the zeroed allocation
		return;
	}
	const auto& info = Locator::infoConstants::value().townDesire;
	for (size_t d = 0; d < k_Desires; ++d)
	{
		belief.desireThreshold.at(d) = info.at(d).desireAffectsBeliefAfter;
	}
}

void Init(Town& town)
{
	auto& b = town.belief;
	// The 8 slots
	b.belief.fill(0.0f);
	b.cap.fill(k_InitialCap);
	b.pending.fill(0.0f);
	b.reduceAccumulator.fill(0.0f);
	b.lastAddedTurn.fill(0);
	// belief[neutral] = beliefInNeutralPlayer (written raw, no SetBelief)
	b.belief.at(Slot(k_Neutral)) = b.beliefInNeutralPlayer;
	b.boredom.fill(k_InitialBoredom);
	ResetDesireThresholds(b);
}

void SetBelief(TownBelief& belief, PlayerNames player, float value)
{
	if (!InRange(player)) // (openblack) a guard of the array
	{
		return;
	}
	const size_t n = Slot(player);
	// cap < v -> cap
	belief.belief.at(n) = belief.cap.at(n) < value ? belief.cap.at(n) : value;
}

float GetBeliefInPlayer(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.belief.at(Slot(player)) : 0.0f;
}

float GetBeliefInPlayer(entt::entity town, PlayerNames player)
{
	const auto* t = TownOf(town);
	return t != nullptr ? GetBeliefInPlayer(t->belief, player) : 0.0f;
}

void SetCap(TownBelief& belief, PlayerNames player, float cap)
{
	if (InRange(player))
	{
		belief.cap.at(Slot(player)) = cap;
	}
}

float GetCap(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.cap.at(Slot(player)) : 0.0f;
}

float GetAddedThisPeriod(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.addedThisPeriod.at(Slot(player)) : 0.0f;
}

float GetAddedThisPeriodRatio(const TownBelief& belief, PlayerNames player)
{
	if (!InRange(player))
	{
		return 0.0f;
	}
	const float added = belief.addedThisPeriod.at(Slot(player));
	if (added == 0.0f)
	{
		return 0.0f;
	}
	const auto mine = static_cast<double>(belief.belief.at(Slot(player)));
	return static_cast<float>(static_cast<double>(added) / (mine * 10.0));
}

void SetBeliefInPlayer(Town& town, PlayerNames player, float value)
{
	if (player == k_Neutral)
	{
		town.belief.beliefInNeutralPlayer = value;
	}
	SetBelief(town.belief, player, value);
}

void ReduceBelief(entt::entity town, PlayerNames player, float r)
{
	if (auto* t = TownOf(town); t != nullptr)
	{
		Reduce(*t, player, r);
	}
}

void AddToBoredomMultiplier(TownBelief& belief, size_t reaction, float f)
{
	if (reaction >= k_Reactions)
	{
		return;
	}
	auto& value = belief.boredom.at(reaction);
	const float b = value + f;
	value = b < 0.0f ? 0.0f : b; // below 0 -> 0
}

float GetBoredomMultiplier(entt::entity town, size_t reaction)
{
	const auto* t = TownOf(town);
	if (t == nullptr || reaction >= k_Reactions)
	{
		return 1.0f; // no town -> 1.0
	}
	return t->belief.boredom.at(reaction);
}

float GetMaxBeliefMeNotIncluded(const TownBelief& belief, PlayerNames player)
{
	float best = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		if (i != Slot(player) && belief.belief.at(i) > best)
		{
			best = belief.belief.at(i);
		}
	}
	return best;
}

float RelativeBelief(const TownBelief& belief, PlayerNames player, PlayerNames thingPlayer)
{
	if (thingPlayer != player)
	{
		return GetBeliefInPlayer(belief, thingPlayer);
	}
	const float mine = GetBeliefInPlayer(belief, player);
	float bestDifference = k_RelativeStart;
	float result = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		if (i == Slot(player))
		{
			continue;
		}
		const float difference = belief.belief.at(i) - mine;
		if (difference > bestDifference)
		{
			bestDifference = difference;
			result = belief.belief.at(i);
		}
	}
	return result;
}

float RivalRecentRatio(const TownBelief& belief, PlayerNames player, const std::function<bool(PlayerNames)>& allied)
{
	// The slots 0..5
	constexpr size_t k_RivalSlots = 6;
	float best = k_RivalRecentStart;
	std::optional<size_t> rival;
	for (size_t i = 0; i < k_RivalSlots; ++i)
	{
		if (i == Slot(player) || (allied && allied(static_cast<PlayerNames>(i))))
		{
			continue;
		}
		if (belief.recent.at(i) > best)
		{
			best = belief.recent.at(i);
			rival = i;
		}
	}
	const float mine = GetBeliefInPlayer(belief, player);
	if (!rival.has_value() || mine == 0.0f)
	{
		return 0.0f;
	}
	// x = 2 belief[i] / belief[P]; x^2, at most 1
	const double x = 2.0 * static_cast<double>(belief.belief.at(*rival)) / static_cast<double>(mine);
	const double squared = x * x;
	return squared < 1.0 ? static_cast<float>(squared) : 1.0f;
}

float BeliefRatioVsRivals(const TownBelief& belief, PlayerNames player, const std::function<bool(PlayerNames)>& allied)
{
	float best = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		const auto other = static_cast<PlayerNames>(i);
		if (other == player || other == k_Neutral || (allied && allied(other)))
		{
			continue;
		}
		if (belief.belief.at(i) > best)
		{
			best = belief.belief.at(i);
		}
	}
	const float mine = GetBeliefInPlayer(belief, player);
	return mine == 0.0f ? 0.0f : static_cast<float>(static_cast<double>(best) / static_cast<double>(mine));
}

float LostTownScale()
{
	return land_balance::LostTownScale();
}

// ---- The fold -------------------------------------------------------------------------------------------------------

void Fold(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	auto& b = t->belief;
	const PlayerNames owner = t->owner;
	const float lost = LostTownScale();

	// 1. pending -> belief, every player
	for (size_t i = 0; i < k_Players; ++i)
	{
		const float d = b.beliefScale * b.pending.at(i);
		if (d == 0.0f || std::isnan(d)) // skipped, and pending[i] is NOT cleared
		{
			continue;
		}
		b.addedThisPeriod.at(i) += d;
		if (Valid(t->centre))
		{
			// A belief sprite at the town centre: d x 10000 (truncated) in player i's colour
			QueueBeliefSprite(GroundPoint(t->centre), Amount(d), PlayerColour(static_cast<PlayerNames>(i)));
		}
		ResetDesireThresholds(b);                                      // whoever the player
		SetBelief(b, static_cast<PlayerNames>(i), b.belief.at(i) + d); // capped
		b.pending.at(i) = 0.0f;
	}

	// 2. the boredom of every reaction: x = lost-town scale x the reaction's boredom addition + boredom[k]
	for (size_t k = 0; k < k_Reactions; ++k)
	{
		const double x = static_cast<double>(lost) * static_cast<double>(info.reaction.at(k).additionToTownBoredomMultipliers) +
		                 static_cast<double>(b.boredom.at(k));
		if (x < static_cast<double>(k_BoredomCeiling)) // below 1 -> store; otherwise unchanged
		{
			b.boredom.at(k) = static_cast<float>(x);
		}
	}

	// 3. the desires that cost the owner belief, not for the neutral player
	if (owner != k_Neutral)
	{
		for (size_t j = 0; j < k_Desires; ++j)
		{
			const auto& desireInfo = info.townDesire.at(j);
			auto& threshold = b.desireThreshold.at(j);
			// The sum of the desire's three parts. (approximate) GetDesire is rounded to float, the original compares
			// the unrounded sum
			const float s = town_desire::GetDesire(t->desire, j);
			if (s > threshold)
			{
				// (s - threshold) x desireToBeliefScale
				const auto r = static_cast<float>((static_cast<double>(s) - static_cast<double>(threshold)) *
				                                  static_cast<double>(desireInfo.desireToBeliefScale));
				Reduce(*t, owner, r);
			}
			// Above the belief info's minimumThreshold (0.25) -> decays (it can step below once)
			if (threshold > info.belief.minimumThreshold)
			{
				threshold -= desireInfo.desireToBeliefThresholdDecay;
			}
		}
	}

	// 4. the neutral belief pinned to beliefInNeutralPlayer, through SetBelief (capped)
	SetBelief(b, k_Neutral, b.beliefInNeutralPlayer);

	// 5. the strongest player and the decay of recent. The slots 0..7 in order; only < skips, so a tie goes to the
	//    higher slot (the owner included)
	size_t best = Slot(owner);
	float bestValue = b.belief.at(best);
	const float decay = info.player.computerPlayerBeliefChangeDecay; // the player info's decay (0.997)
	for (size_t n = 0; n < k_Players; ++n)
	{
		if (b.belief.at(n) >= bestValue)
		{
			bestValue = b.belief.at(n);
			best = n;
		}
		b.recent.at(n) *= decay;
	}

	// 6. the conversion when the strongest is not the owner
	if (best == Slot(t->owner))
	{
		return;
	}
	const auto newOwner = static_cast<PlayerNames>(best);
	// SetBelief(best, claimedTownBeliefMultiplier (1.5) x belief[best])
	SetBelief(b, newOwner,
	          static_cast<float>(static_cast<double>(info.belief.claimedTownBeliefMultiplier) *
	                             static_cast<double>(b.belief.at(best))));
	if (owner != k_Neutral)
	{
		// Every town of P (this one is still in the list):
		// SetBelief(P, lostATownBeliefInPlayerMultiplier (0.9) x belief[P] x lost-town scale)
		for (const auto other : map_cells::TownsOf(owner))
		{
			if (auto* o = TownOf(other); o != nullptr)
			{
				const double value = static_cast<double>(info.belief.lostATownBeliefInPlayerMultiplier) *
				                     static_cast<double>(o->belief.belief.at(Slot(owner))) * static_cast<double>(lost);
				SetBelief(o->belief, owner, static_cast<float>(value));
			}
		}
	}
	// The old owner local -> GeneralBad help sprite, else the new one local -> GeneralGood; then TakeOverTown
	if (owner == k_LocalPlayer)
	{
		Help(detail::HelpSprite::GeneralBad, town);
	}
	else if (newOwner == k_LocalPlayer)
	{
		Help(detail::HelpSprite::GeneralGood, town);
	}
	TakeOverTown(newOwner, town);
}

void TakeOverTown(PlayerNames player, entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	// The empty countdown 0, then the new owner.
	// (pending) the rest: see the header
	const auto previous = t->owner;
	t->emptyCountdown = 0;
	t->owner = player;
	// Out of the old owner's list, at the tail of the new owner's
	t->ownerListStamp = town_queries::NextOwnerListStamp();
	// The new player's GameStats: its taken-over population += adults + children
	game_stats::TownTakenOver(Slot(player), t->stats.adults + t->stats.children);
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "town_belief: town {} taken over by player {} (was {})", t->id, static_cast<int>(player),
		                    static_cast<int>(previous));
	}
}

void SetTownEmpty(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	Init(*t);
	for (size_t i = 0; i < k_Players; ++i)
	{
		SetBelief(t->belief, static_cast<PlayerNames>(i), t->belief.beliefInNeutralPlayer);
	}
	if (t->owner != k_Neutral)
	{
		TakeOverTown(k_Neutral, town);
	}
}

// ---- ProcessOncePerTurn ---------------------------------------------------------------------------------------------

void ProcessOncePerTurn()
{
	// Only on turn % 10 == 0
	if (game_clock::Turn() % k_ProcessEvery != 0 || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const size_t me = Slot(k_LocalPlayer);
	float myAdded = 0.0f;
	// The newest town first (towns are inserted at the head of the list)
	const auto towns = town_queries::TownsNewestFirst();
	for (size_t p = 0; p < k_Players; ++p)
	{
		float sum = 0.0f;
		for (const auto town : towns)
		{
			auto& b = registry.Get<Town>(town).belief;
			const float a = b.addedThisPeriod.at(p);
			if (p == me && a > 0.0f)
			{
				myAdded += a;
			}
			b.addedThisPeriod.at(p) = 0.0f;
			sum = sum + b.belief.at(p); // stored as a float on every step
		}
		// After the towns (also with none): the player's world belief max / min
		game_stats::WorldBelief(p, sum);
	}
	if (myAdded > k_ToolTipMinimum)
	{
		// v = myAdded x 1000; the "believers gained" tooltip
		const float v = myAdded * k_ToolTipScale;
		events::Publish(events::TownBeliefToolTip {.text = k_ToolTipBelieversGained, .value = v});
		// The original also shows a "%4.0f" floating number of v at the point under the hand, coloured 00 FF FF FF.
		// (pending) openblack has no value spinner
	}
	CheckLosingBelief(k_LocalPlayer); // inside the % 10 block
}

void CheckLosingBelief(PlayerNames player)
{
	// Nothing on land 1
	if (influence::LandNumber() == 1 || !InRange(player) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto warn = static_cast<double>(Locator::infoConstants::value().belief.beliefLeftWhenHelpSpritesWarn);
	const size_t n = Slot(player);
	for (const auto town : map_cells::TownsOf(player))
	{
		const auto* t = TownOf(town);
		if (t == nullptr)
		{
			continue;
		}
		const double mine = static_cast<double>(t->belief.belief.at(n)) - warn;
		for (size_t i = 0; i < k_Players; ++i) // the neutral slot included
		{
			if (i == n)
			{
				continue;
			}
			if (mine < static_cast<double>(t->belief.belief.at(i)))
			{
				if (player == k_LocalPlayer) // P is the local player
				{
					Help(detail::HelpSprite::LosingBelief, town);
				}
				return; // the first town found only
			}
		}
	}
}

// ---- DrawBelief and the belief sprites ------------------------------------------------------------------------------

void DrawBelief(float f, entt::entity thing, std::optional<PlayerNames> player)
{
	const int32_t v = Amount(f);
	if (v <= 0)
	{
		return;
	}
	glm::vec3 position {0.0f}; // (openblack) 0: the original leaves it uninitialised without a thing
	if (Valid(thing))
	{
		// The ground point plus the thing's height
		position = GroundPoint(thing);
		position.y += object::GetHeight(thing);
	}
	// A debug flag would show a "B n" value spinner. (not ported) no one sets it
	const uint32_t colour = player.has_value() ? PlayerColour(*player) : k_NoPlayerColour;
	QueueBeliefSprite(position, v, colour);
}

void QueueBeliefSprite(const glm::vec3& position, int32_t amount, uint32_t colour)
{
	// Nothing once the queue is full (the original's other condition is a constant flag that is always set)
	if (BeliefSprites().size() >= k_MaxBeliefSprites)
	{
		return;
	}
	BeliefSprites().push_back({position, amount, colour});
}

std::optional<BeliefSprite> PopBeliefSprite()
{
	auto& sprites = BeliefSprites();
	if (sprites.empty())
	{
		return std::nullopt;
	}
	const auto last = sprites.back(); // the last entry
	sprites.pop_back();
	return last;
}

size_t BeliefSpriteCount()
{
	return BeliefSprites().size();
}

void AddBeliefEventHandlers(EventManager& manager)
{
	manager.AddHandler<events::TownBeliefHelp>(PlayHelp);
	// The tooltip of the believers gained
	manager.AddHandler<events::TownBeliefToolTip>(
	    [](const events::TownBeliefToolTip& event) { help::tooltips::Force(event.text, event.value); });
}

void detail::ClearForTests()
{
	BeliefSprites().clear();
}
} // namespace openblack::ecs::town_belief
