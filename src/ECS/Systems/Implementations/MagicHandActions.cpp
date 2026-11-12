/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The actions of the hand on each interface turn: the fireballs that the hand catches, holds and throws, and
// the press and the release of the action button.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "Hand/HandGrabRules.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellSeedRules.h"
#include "MagicSystem.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

void MagicSystem::InterfaceTurn()
{
	if (!Held().has_value() || !EntityRegistry().Valid(*Held()) || !Locator::infoConstants::has_value())
	{
		return;
	}
	// A miracle held locked in the hand that the hand moved outside its cast rule is let go, as the button would
	LetGoIfOutsideCastRule();
	// While the button stays down a locked miracle is sent again where the hand is, at the turn that starts now; moved
	// somewhere it may not be cast it isn't sent, and stays locked until the button comes up
	if (_input.state == magic::CastInput::State::Locked)
	{
		const bool valid = _input.onObject ? HeldSeedTarget().has_value() : HandPointValid();
		if (const auto apply = magic::Tick(_input, _turn + 1, valid); apply.cast.has_value())
		{
			SendCast(*apply.cast);
		}
	}
}

bool MagicSystem::TapFireBall(entt::entity ball)
{
	if (!Locator::infoConstants::has_value() || Held().has_value() || !EntityRegistry().Valid(ball) ||
	    !EntityRegistry().AllOf<MagicFireBall>(ball) || !EmptyHandInInfluence())
	{
		return false;
	}
	SendFireBallCommand(ball, false);
	return true;
}

std::optional<entt::entity> MagicSystem::PickedUnderHand() const
{
	// What the cursor picked, as anything else under the hand is found
	if (!_hand.overWorld || !Locator::pickingSystem::has_value())
	{
		return std::nullopt;
	}
	const auto picked = Locator::pickingSystem::value().GetPick().object;
	if (!picked.has_value() || !EntityRegistry().Valid(*picked))
	{
		return std::nullopt;
	}
	return picked;
}

std::optional<entt::entity> MagicSystem::OrbUnderHand() const
{
	const auto picked = PickedUnderHand();
	return picked.has_value() && EntityRegistry().AllOf<OneOffSpellSeed>(*picked) ? picked : std::nullopt;
}

bool MagicSystem::EmptyHandInInfluence() const
{
	// The hand is this computer's player's
	return _hand.point.has_value() && _world.HandInInfluence(OwnPlayer(), *_hand.point);
}

bool MagicSystem::PressFireBall(uint32_t nowMs, uint32_t turn)
{
	_fireBallPress.reset();
	if (!Locator::infoConstants::has_value() || Held().has_value())
	{
		return false;
	}
	const auto picked = PickedUnderHand();
	const auto ball = picked.has_value() && EntityRegistry().AllOf<MagicFireBall>(*picked) ? picked : std::nullopt;
	// Out of the player's influence the hand neither taps nor takes it
	if (!ball.has_value() || !EmptyHandInInfluence())
	{
		return false;
	}
	// The player's own fireball cannot go into the hand, so the press taps it at once: the help counts the tap, and the
	// ball slips through the hand
	const auto& fireBall = EntityRegistry().Get<const MagicFireBall>(*ball);
	if (hand_grab::PressOnFireBall(fireBall.hasPlayer && fireBall.player == OwnPlayer()) == hand_grab::FireBallPress::TapAtOnce)
	{
		if (Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Report(help::profile::k_TapThing);
		}
		return true;
	}
	// The press starts to take another's fireball: a quick release taps it, and the help counts that tap. Held as long
	// as a tap can last, the hand takes it, which catches it the same way, but the help counts no tap.
	_fireBallPress = FireBallPress {.ball = *ball, .pressMs = nowMs, .pressTurn = turn};
	return true;
}

void MagicSystem::HoldFireBall(uint32_t nowMs, uint32_t turn)
{
	if (!_fireBallPress.has_value() ||
	    !hand_grab::TakesFireBallAtHold(hand_grab::ElapsedMs(nowMs, _fireBallPress->pressMs, turn, _fireBallPress->pressTurn)))
	{
		return;
	}
	const auto ball = _fireBallPress->ball;
	_fireBallPress.reset();
	// The hand sends the take, which catches the ball at the turn
	if (!Held().has_value() && EntityRegistry().Valid(ball) && EntityRegistry().AllOf<MagicFireBall>(ball) &&
	    EmptyHandInInfluence())
	{
		SendFireBallCommand(ball, true);
	}
}

void MagicSystem::ReleaseFireBall(uint32_t nowMs, uint32_t turn)
{
	if (!_fireBallPress.has_value())
	{
		return;
	}
	const auto press = *_fireBallPress;
	_fireBallPress.reset();
	const bool quick = hand_grab::TapsFireBallAtRelease(hand_grab::ElapsedMs(nowMs, press.pressMs, turn, press.pressTurn));
	if (!quick || Held().has_value() || !EntityRegistry().Valid(press.ball) || !EmptyHandInInfluence())
	{
		return;
	}
	// The quick tap catches another player's fireball, and the help counts the tap first
	if (EntityRegistry().AllOf<MagicFireBall>(press.ball))
	{
		if (Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Report(help::profile::k_TapThing);
		}
		// The hand sends the tap, which catches the ball at the turn
		SendFireBallCommand(press.ball, false);
	}
}

void MagicSystem::SendFireBallCommand(entt::entity ball, bool take)
{
	if (!Locator::playerCommandSystem::has_value())
	{
		DoCatchFireBall(OwnPlayer(), ball);
		return;
	}
	auto& commands = Locator::playerCommandSystem::value();
	if (take)
	{
		commands.Submit(player_commands::HandTake {.object = ball});
	}
	else
	{
		commands.Submit(player_commands::HandTap {.object = ball});
	}
}

void MagicSystem::DoCatchFireBall(PlayerNames player, entt::entity ball)
{
	// Every computer catches it into the hand of the player that sent the tap or the take, if the hand is still empty.
	// The press asked for the player's influence.
	const ActingAs acting(*this, player);
	if (!Locator::infoConstants::has_value() || Held().has_value() || !EntityRegistry().Valid(ball) ||
	    !EntityRegistry().AllOf<MagicFireBall>(ball))
	{
		return;
	}
	CatchFireBall(ball);
}

bool MagicSystem::CatchFireBall(entt::entity ball)
{
	auto& registry = EntityRegistry();
	const auto& fireBall = registry.Get<const MagicFireBall>(ball);
	// Only another's ball is caught: the player's own slips through the hand
	const auto catcher = ActingPlayer();
	if (fireBall.hasPlayer && fireBall.player == catcher)
	{
		return false;
	}
	// It becomes a fireball seed in the hand, charged in full and ready, and the ball goes
	if (GiveSeedToHand(catcher, SpellSeedType::Fire, magic::k_BasePowerUpLevel, 1.0f) == entt::null)
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: the hand caught fireball #{}", entt::to_integral(ball));
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().Forget(ball);
	}
	registry.Destroy(ball);
	return true;
}

bool MagicSystem::AbsorbsFireBalls() const
{
	const auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return false;
	}
	const auto& seed = registry.Get<const SpellSeed>(*Held());
	return seed.ready && seed.seedType == SpellSeedType::Fire;
}

bool MagicSystem::DoApplySeed(PlayerNames player, entt::entity target)
{
	// A miracle in the hand of the player takes the command, also when the thing is gone by the turn
	const ActingAs acting(*this, player);
	if (!Held().has_value())
	{
		return false;
	}
	if (EntityRegistry().Valid(target) && EntityRegistry().AllOf<MagicFireBall>(target))
	{
		AbsorbFireBall(target);
	}
	return true;
}

bool MagicSystem::AbsorbFireBall(entt::entity ball)
{
	auto& registry = EntityRegistry();
	if (!AbsorbsFireBalls())
	{
		return false;
	}
	auto& seed = registry.Get<SpellSeed>(*Held());
	// The catch uses the row of the fireball table that the ball uses
	const auto row = EntityRegistry().Get<const MagicFireBall>(ball).infoRow;
	seed.power *= 1.0f + Info().magicFireBall.at(row).catchIncreaseFactor;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: the fire seed took in fireball #{}, its power now {:.4f}",
	                    entt::to_integral(ball), seed.power);
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().Forget(ball);
	}
	registry.Destroy(ball);
	return true;
}

bool MagicSystem::PressAction()
{
	if (!_hand.overWorld || !Locator::infoConstants::has_value() || !Held().has_value())
	{
		return false;
	}
	_actionDown = true;
	// A ready fire seed put to a fireball, anyone's, takes it in: the seed grows stronger and stays in the hand. The hand
	// sends the use of the seed on the ball, which the turn does.
	if (const auto ball = PickedUnderHand();
	    ball.has_value() && EntityRegistry().AllOf<MagicFireBall>(*ball) && AbsorbsFireBalls())
	{
		// While the hand waits for its last send, the press does nothing more
		if (OwnSendWaits())
		{
			return true;
		}
		if (Locator::playerCommandSystem::has_value())
		{
			Locator::playerCommandSystem::value().Submit(player_commands::HandApply {.target = *ball});
			MarkOwnSend();
		}
		else
		{
			AbsorbFireBall(*ball);
		}
		return true;
	}
	const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
	const auto& seedInfo = magic::GetSpellSeedInfo(Info(), seed.seedType);
	// Outside the player's influence a press does nothing at all
	const magic::PressContext context {
	    .inInfluence = IsHandInInfluence(),
	    .seedReady = seed.ready,
	    .onValidObject = HeldSeedTarget().has_value(),
	    .pointValid = HandPointValid() || seedInfo.sizingGesture == GestureType::Circle,
	    .turn = _turn,
	};
	// A seed armed on a thing keeps the thing of the press, and casts at it at the let go
	const auto target = HeldSeedTarget();
	_pressThing = entt::null;
	Do(magic::Press(_input, {.castType = seedInfo.castType, .castOnObject = seedInfo.castOnObject != 0}, context));
	if (_input.state == magic::CastInput::State::Armed && _input.onObject && target.has_value())
	{
		_pressThing = *target;
	}
	return true;
}

void MagicSystem::ReleaseAction()
{
	_actionDown = false;
	if (Held().has_value())
	{
		Do(magic::Release(_input));
	}
}

void MagicSystem::StopActionAtLockEnd()
{
	// The press of the hand of this computer stops
	const ActingAs acting(*this, OwnPlayer());
	Do(magic::Cancel(_input));
	_pressThing = entt::null;
}

void MagicSystem::UnlockHeldMiracle()
{
	auto& registry = EntityRegistry();
	const bool own = ActsOnOwnHand();
	if (own)
	{
		magic::StopPour(_pour);
	}
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return;
	}
	auto& seed = registry.Get<SpellSeed>(*Held());
	// The seed keeps what its miracle had left, also a miracle that closed down. A seed with no miracle keeps nothing
	// and always has enough.
	bool enough = true;
	if (auto* spell = FindSpell(seed.spell))
	{
		enough = StoreFromSpell(*Held(), seed, *spell);
	}
	else
	{
		ForgetStoredSpell(seed);
	}
	if (own)
	{
		_lastHandResult = HandResult::Released;
	}
	if (!magic::KeptAtUnlock(magic::GetSpellSeedInfo(Info(), seed.seedType).deleteSeedOnceCast != 0, enough))
	{
		LetGoOfSeed(true);
	}
}
