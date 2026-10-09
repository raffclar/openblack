/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashOrders.h"

#include <cmath>

#include <algorithm>
#include <numbers>
#include <utility>

#include "Creature/CreatureFootprints.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"

using namespace openblack;
using namespace openblack::creature_leash_orders;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
using creature_mind::Step;

namespace
{
Step Walk(glm::vec2 point, float arrival, bool run)
{
	Step step {.kind = Step::Kind::Move};
	step.movement = {.kind = Movement::Kind::ToPoint, .point = point, .run = run, .maxDistance = arrival};
	return step;
}

Step Wait(float seconds)
{
	return {.kind = Step::Kind::Wait, .seconds = seconds};
}

Step Hands(ObjectOrder order)
{
	Step step {.kind = Step::Kind::Object};
	step.order = order;
	return step;
}

/// Seconds cut down to whole game turns, worked out as the game does in more than single precision, so that 2.1 seconds
/// at ten turns a second is 20 turns
float WholeTurns(float seconds, float turnsPerSecond)
{
	if (turnsPerSecond <= 0.0f)
	{
		return seconds;
	}
	const auto turns = std::floor(static_cast<double>(seconds) * static_cast<double>(turnsPerSecond));
	return static_cast<float>(turns / static_cast<double>(turnsPerSecond));
}
} // namespace

Tap creature_leash_orders::OnTap(const Leash& leash, const Tapped& tapped)
{
	if (!leash.worn || leash.tiedTo.has_value() || leash.playingGame)
	{
		return Tap::Normal;
	}
	if (!tapped.object.has_value())
	{
		return Tap::OrderOnLand;
	}
	if (*tapped.object == leash.creature || !tapped.leashTarget)
	{
		return Tap::Normal;
	}
	return Tap::OrderOnThing;
}

Tap creature_leash_orders::OnDoubleTap(const Leash& leash, const Tapped& tapped)
{
	if (!leash.worn)
	{
		return Tap::Normal;
	}
	if (leash.tiedTo.has_value())
	{
		return tapped.object.has_value() && (*tapped.object == *leash.tiedTo || *tapped.object == leash.creature)
		           ? Tap::Untie
		           : Tap::Nothing;
	}
	if (!tapped.object.has_value() || *tapped.object == leash.creature)
	{
		return Tap::Nothing;
	}
	if (tapped.miracleBubble)
	{
		return Tap::Normal;
	}
	return tapped.leashTarget ? Tap::Tie : Tap::Nothing;
}

bool DoubleTaps::OnPress(uint32_t milliseconds, glm::vec2 screen)
{
	const auto last = std::exchange(_last, Press {.milliseconds = milliseconds, .screen = screen});
	if (!last.has_value())
	{
		return false;
	}
	const bool quick = milliseconds - last->milliseconds <= k_DoubleTapMilliseconds;
	const bool still =
	    std::abs(screen.x - last->screen.x) <= k_DoubleTapSlop && std::abs(screen.y - last->screen.y) <= k_DoubleTapSlop;
	if (quick && still)
	{
		// A third press starts afresh
		_last.reset();
		return true;
	}
	return false;
}

std::optional<uint32_t> creature_leash_orders::ToolTipFor(const Leash& leash, const Hovered& hovered)
{
	if (!leash.worn)
	{
		return std::nullopt;
	}
	if (leash.tiedTo.has_value())
	{
		return hovered.object == leash.tiedTo ? std::optional(k_DetachLeashTip) : std::nullopt;
	}
	if (!hovered.object.has_value())
	{
		return k_FocusCreatureTip;
	}
	if (hovered.developmentPhase > k_TyingPhase && *hovered.object != leash.creature && hovered.leashTarget &&
	    !hovered.miracleBubble)
	{
		return k_AttachLeashTip;
	}
	return std::nullopt;
}

bool creature_leash_orders::TakesOrders(const Body& body)
{
	return body.leashWorks && body.life > 0.0f && body.exhaustion < k_TooExhausted;
}

GroundOrder creature_leash_orders::OnGround(glm::vec2 place, const Ground& ground)
{
	if (ground.carrying)
	{
		// Carrying something, it takes it there to put down, or on the aggression leash, or where it can't stand,
		// throws it there from near enough
		if (ground.leash != LeashType::Evil && ground.placeReachable)
		{
			return {.kind = GroundOrder::Kind::PutDownAt, .point = place, .arrival = ground.height};
		}
		return {.kind = GroundOrder::Kind::ThrowAt, .point = place, .arrival = ground.height * k_ThrowReachHeights};
	}
	if (!ground.reachable.has_value())
	{
		return {.kind = GroundOrder::Kind::Inaccessible, .point = place, .acknowledged = false, .marked = false};
	}
	const auto point = *ground.reachable;
	if (ground.water)
	{
		return {.kind = ground.thirsty ? GroundOrder::Kind::Drink : GroundOrder::Kind::LookAtReflection, .point = point};
	}
	if (ground.field)
	{
		return {.kind = GroundOrder::Kind::ActOnField, .point = point};
	}
	if (ground.playerHasCitadel && ground.exhaustion > k_SleepExhaustion && !ground.sleeping &&
	    ground.distanceFromHome.has_value() && *ground.distanceFromHome < k_HomeSleepReach)
	{
		return {.kind = GroundOrder::Kind::SleepAtHome, .point = point};
	}
	return {.kind = GroundOrder::Kind::MoveTo,
	        .point = point,
	        .arrival = std::min(ground.height, k_MaxArrival),
	        .runAndWait = ground.orderInForce};
}

std::vector<Attempt> creature_leash_orders::OnThing(const Thing& thing)
{
	if (thing.forest)
	{
		return {Attempt::GoToForest};
	}
	if (thing.fishFarm && thing.hungry && thing.knowsFishing && thing.energy < 1.0f)
	{
		return {Attempt::FishAndEat};
	}
	const bool fight = thing.leash == LeashType::Evil && thing.creature && thing.distance < thing.height * k_FightReachHeights;
	const auto fightOrDesires = fight ? Attempt::Fight : Attempt::Desires;
	const auto fallback = thing.liftable ? Attempt::Hold : Attempt::Look;
	if (thing.carrying)
	{
		return {Attempt::DesiresUsingCarried, fightOrDesires, fallback};
	}
	if (!thing.liftable)
	{
		return {fightOrDesires, fallback};
	}
	return {fallback};
}

bool creature_leash_orders::Acknowledges(const std::vector<Attempt>& attempts)
{
	return attempts.empty() || attempts.front() != Attempt::FishAndEat;
}

std::vector<Step> creature_leash_orders::MoveTo(glm::vec2 point, float arrival, bool run, float waitSeconds)
{
	std::vector<Step> agenda {Walk(point, arrival, run)};
	if (run)
	{
		agenda.push_back(Wait(waitSeconds));
	}
	return agenda;
}

float creature_leash_orders::RunWaitSeconds(float extraSeconds, float turnsPerSecond)
{
	return WholeTurns(k_RunWaitSeconds + std::clamp(extraSeconds, 0.0f, k_RunWaitExtraSeconds), turnsPerSecond);
}

std::vector<Step> creature_leash_orders::SleepAtHome(glm::vec2 home, float height, std::vector<Step> sleep)
{
	// A yawn first, when the sleep has one, then off home, then the sleep itself
	std::vector<Step> agenda;
	auto rest = sleep.begin();
	if (rest != sleep.end() && rest->kind == Step::Kind::Action)
	{
		agenda.push_back(*rest);
		++rest;
	}
	agenda.push_back(Walk(home, std::min(height, k_MaxSleepArrival), false));
	agenda.insert(agenda.end(), rest, sleep.end());
	return agenda;
}

std::vector<Step> creature_leash_orders::PutDownAt(glm::vec2 point, float height)
{
	return {Walk(point, height, false), Hands({.kind = ObjectOrder::Kind::Discard})};
}

std::vector<Step> creature_leash_orders::ThrowAt(glm::vec2 point, float height)
{
	return {Walk(point, height * k_ThrowReachHeights, false), Hands({.kind = ObjectOrder::Kind::Throw, .point = point})};
}

std::vector<Step> creature_leash_orders::Hold(uint32_t object, bool alreadyHeld, glm::vec2 camera,
                                              std::optional<uint32_t> playWith, float waitSeconds)
{
	std::vector<Step> agenda;
	if (!alreadyHeld)
	{
		agenda.push_back(Hands({.kind = ObjectOrder::Kind::PickUp, .object = object}));
	}
	Step face {.kind = Step::Kind::Move};
	face.movement = {.kind = Movement::Kind::TurnToFace, .point = camera};
	agenda.push_back(face);
	if (playWith.has_value())
	{
		agenda.push_back(Hands({.kind = ObjectOrder::Kind::Keep,
		                        .animation = creature_object_actions::k_FirstKeepAnimation +
		                                     std::min<size_t>(*playWith, creature_object_actions::k_KeepAnimationCount - 1)}));
	}
	agenda.push_back(Wait(waitSeconds));
	return agenda;
}

std::vector<Step> creature_leash_orders::LookAtReflection(glm::vec2 water, glm::vec2 shore, float height, float turnsPerSecond)
{
	constexpr float k_FirstLook = 1.0f;
	constexpr float k_LongLook = 2.1f;
	constexpr float k_ShoreLook = 1.2f;
	constexpr float k_LastLook = 1.1f;
	const auto look = [turnsPerSecond](float seconds) { return Wait(WholeTurns(seconds, turnsPerSecond)); };
	const auto dazed = [] { return Step {.kind = Step::Kind::Action, .animation = creature_layers::animations::k_Confused}; };
	const auto arrival = 2.0f * height;
	return {Walk(water, arrival, false),
	        look(k_FirstLook),
	        dazed(),
	        look(k_LongLook),
	        Walk(shore, arrival, false),
	        look(k_ShoreLook),
	        dazed(),
	        Walk(water, arrival, false),
	        look(k_LastLook),
	        look(k_LongLook)};
}

float creature_leash_orders::HoldSeconds(float extraSeconds, float turnsPerSecond)
{
	return WholeTurns(k_HoldSeconds + std::clamp(extraSeconds, 0.0f, k_HoldExtraSeconds), turnsPerSecond);
}

glm::vec3 creature_leash_orders::MarkerOverThing(glm::vec3 point, float height)
{
	return point + glm::vec3(0.0f, k_MarkerLift + height, 0.0f);
}

glm::vec3 creature_leash_orders::MarkerOverLand(glm::vec3 point)
{
	return point + glm::vec3(0.0f, k_MarkerLift, 0.0f);
}

glm::vec2 creature_leash_orders::MarkerSize(uint32_t clockMs)
{
	constexpr float k_Least = 0.2f;
	constexpr float k_TurnsPerMs = 0.0005f;
	const auto swing = [](uint32_t ms) {
		return std::abs(std::cos(static_cast<float>(ms) * k_TurnsPerMs * 2.0f * std::numbers::pi_v<float>)) + k_Least;
	};
	return {swing(clockMs), swing(clockMs + k_PulseLagMs)};
}

uint8_t creature_leash_orders::FootprintCell(size_t speciesRow)
{
	// The table ends at the chicken; anything past it shows the first picture
	constexpr size_t k_LastRow = 17;
	return speciesRow > k_LastRow ? 0 : creature_footprints::k_SpeciesPrints.at(speciesRow).cell;
}
