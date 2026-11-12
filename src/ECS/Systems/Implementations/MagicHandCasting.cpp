/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The casts from the hand: how a cast is made, sent as a command and done on each computer, and what follows
// it.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <utility>

#include <glm/gtx/euler_angles.hpp>

#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellLifetime.h"
#include "Magic/SpellSeedRules.h"
#include "MagicSystem.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The size a miracle cast from the hand takes without a gesture: a sizing miracle its tables' usual size, any other
/// the plain size
constexpr float k_PlainHandMagnitude = 1.0f;

ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

std::optional<player_commands::HandCast> MagicSystem::MakeCast(magic::CastTarget target, entt::entity pressThing)
{
	auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return std::nullopt;
	}
	const auto& seed = registry.Get<const SpellSeed>(*Held());
	const auto& seedInfo = magic::GetSpellSeedInfo(Info(), seed.seedType);
	player_commands::HandCast cast {.point = _hand.point.value_or(glm::vec3(0.0f)),
	                                .size = k_PlainHandMagnitude,
	                                .handPosition = _hand.handPosition,
	                                .cameraForward = _hand.cameraForward,
	                                .handVelocity = _handVelocity,
	                                .spin = _handSpin.spin};
	// A locked miracle that still runs is applied again where it is: the turn checks nothing more
	if (const auto* running = FindSpell(seed.spell); running != nullptr && !running->closedDown)
	{
		if (target == magic::CastTarget::Object)
		{
			cast.object = HeldSeedTarget().value_or(entt::entity {entt::null});
		}
		return cast;
	}
	if (target == magic::CastTarget::Object)
	{
		// A seed armed on a thing casts at the thing of the press, else at the thing under the hand
		const auto object = pressThing != entt::null ? std::optional(pressThing) : HeldSeedTarget();
		if (!object.has_value())
		{
			return std::nullopt;
		}
		cast.object = *object;
	}
	else if (seedInfo.sizingGesture == GestureType::Circle)
	{
		// The storms and shields go where the circle was drawn, as big as it was, while it is remembered
		if (!_circle.has_value() || _turn - _circle->turn > magic::k_CircleTurns)
		{
			_circle.reset();
			_lastHandResult = HandResult::NoCircle;
			return std::nullopt;
		}
		cast.point = _circle->centre;
		cast.size = _circle->radius;
		if ((!_ignoreInfluence && !_world.InInfluence(seed.player, cast.point)) ||
		    !CanCastAt(HeldMagicType(), seed.player, cast.point))
		{
			return std::nullopt;
		}
	}
	else if (!HandPointValid())
	{
		return std::nullopt;
	}
	return cast;
}

void MagicSystem::SendCast(magic::CastTarget target)
{
	// While the hand waits for its last send, a cast at the land does nothing: no check, no fail and no send
	if (target == magic::CastTarget::Point && OwnSendWaits())
	{
		return;
	}
	// A seed armed on a thing casts at the thing of the press. When that thing is no target now, the cast does nothing:
	// no fail and no send. The interface lock ends.
	const auto pressThing = std::exchange(_pressThing, entt::entity {entt::null});
	if (target == magic::CastTarget::Object && pressThing != entt::null && !IsHeldSeedTarget(pressThing))
	{
		_input = {};
		if (ActsOnOwnHand() && Locator::handSystem::has_value())
		{
			Locator::handSystem::value().EndOwnInterfaceLock();
		}
		return;
	}
	const auto cast = MakeCast(target, target == magic::CastTarget::Object ? pressThing : entt::entity {entt::null});
	if (!cast.has_value())
	{
		_input = {};
		FailCast();
		return;
	}
	// A cast at a thing that the seed can act on also does nothing while the hand waits
	if (OwnSendWaits())
	{
		return;
	}
	auto& registry = EntityRegistry();
	const auto& seed = registry.Get<const SpellSeed>(*Held());
	const auto& seedInfo = magic::GetSpellSeedInfo(Info(), seed.seedType);
	if (!HeldSpellRunsInHand())
	{
		// The gesture kept for the cast: at the land it is the circle remembered, at a thing the last gesture drawn
		const auto last =
		    Locator::gestureSystem::has_value() ? Locator::gestureSystem::value().GetLastRecognised() : std::nullopt;
		const auto kept = target != magic::CastTarget::Object ? (_circle.has_value() ? GestureType::Circle : GestureType::None)
		                                                      : (last.has_value() ? last->request.gesture : GestureType::None);
		// A cast that passes the check of its sizing gesture starts the path of the gesture again, as the hand sends it
		if (gesture::PassesSizingCheck(seedInfo.sizingGesture, kept) && Locator::gestureSystem::has_value())
		{
			Locator::gestureSystem::value().ForgetPath();
		}
		// The help counts a cast of a miracle with a power-up, as the hand sends it. The count waits for the next report.
		if (Locator::helpProfileSystem::has_value())
		{
			auto* gestures = Locator::gestureSystem::has_value() ? &Locator::gestureSystem::value() : nullptr;
			const gesture::HandContext::Seed facts {.ready = seed.ready,
			                                        .cast = seed.lastCastType != MagicType::None,
			                                        .sizingGesture = seedInfo.sizingGesture,
			                                        .powerUp = seed.powerUp,
			                                        .askedPowerUp =
			                                            gestures != nullptr ? gestures->GetAskedPowerUp() : GestureType::None,
			                                        .canPowerUp = seed.hasIcon};
			if (gesture::CountsPoweredUpCast(facts, kept))
			{
				// The hand starts its power-ups again, and the help counts the cast
				gestures->PoweredUpCast();
				Locator::helpProfileSystem::value().Report(help::profile::k_GestureCastPoweredUp);
			}
		}
		// The circle drawn is used up by the cast that it sizes
		if (target != magic::CastTarget::Object && seedInfo.sizingGesture == GestureType::Circle)
		{
			_circle.reset();
		}
	}
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(*cast);
		MarkOwnSend();
		return;
	}
	DoCast(OwnPlayer(), *cast);
}

void MagicSystem::SendCastUnlock()
{
	if (!Held().has_value())
	{
		return;
	}
	const player_commands::HandCastUnlock unlock {.seed = *Held()};
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(unlock);
		return;
	}
	DoCastUnlock(OwnPlayer(), unlock.seed);
}

void MagicSystem::DoCast(PlayerNames player, const player_commands::HandCast& cast)
{
	// The cast acts on the miracle in the hand of the player that sent it
	const ActingAs acting(*this, player);
	if (!Held().has_value())
	{
		return;
	}
	auto& registry = EntityRegistry();
	if (!registry.Valid(*Held()) || !Locator::infoConstants::has_value())
	{
		return;
	}
	// A miracle put to the player's own dispenser goes into it as a one-shot globe. The hand does not cast it.
	if (cast.object != entt::null && registry.Valid(cast.object) && registry.AllOf<SpellDispenser>(cast.object))
	{
		const auto& seed = registry.Get<const SpellSeed>(*Held());
		if (IsOwnDispenser(cast.object, seed.player) && seed.lastCastType == MagicType::None &&
		    !registry.AllOf<ScriptHighlight>(cast.object))
		{
			StoreHeldSeedIn(cast.object);
		}
		return;
	}
	const bool own = ActsOnOwnHand();
	const bool locked = own && _input.state == magic::CastInput::State::Locked;
	const auto spell = CastHeldSeed(cast);
	if (spell == entt::null)
	{
		// The turn may not cast it there: the hand of this computer forgets the press
		if (own)
		{
			_input = {};
		}
		return;
	}
	if (own)
	{
		_lastHandResult = locked ? HandResult::CastHeld : HandResult::Cast;
	}
	AfterCast(spell);
}

void MagicSystem::DoCastUnlock(PlayerNames player, entt::entity seed)
{
	// The let go acts on the miracle in the hand of the player that sent it
	const ActingAs acting(*this, player);
	if (!Held().has_value() || *Held() != seed)
	{
		return;
	}
	UnlockHeldMiracle();
}

entt::entity MagicSystem::CastHeldSeed(const player_commands::HandCast& command)
{
	auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return entt::null;
	}
	auto& seed = registry.Get<SpellSeed>(*Held());
	const bool onObject = command.object != entt::null;
	// Using the held miracle on the land counts as a turn more for what the hand keeps of the player's influence past
	// the border, before it is known whether it may be used there
	if (!onObject)
	{
		_world.HeldThingUsedOnLand(seed.player);
	}
	const auto& seedInfo = magic::GetSpellSeedInfo(Info(), seed.seedType);
	const auto recast = magic::RecastOf(FindSpell(seed.spell) != nullptr, seedInfo.castType == SpellCastType::SpellCastInHand,
	                                    seed.linkedByCast);
	// A locked miracle is applied again where it is: it follows the hand
	if (recast == magic::SeedRecast::Reuse)
	{
		return seed.spell;
	}
	const auto type = HeldMagicType();
	std::optional<entt::entity> object;
	glm::vec3 point(0.0f);
	// Without a gesture to size it, a miracle takes the plain size
	float size = k_PlainHandMagnitude;
	if (onObject)
	{
		// The turn checks again that the miracle may be cast on the thing
		if (!registry.Valid(command.object) || seedInfo.castOnObject == 0 || !CanCastOn(type, command.object))
		{
			return entt::null;
		}
		object = command.object;
	}
	else if (seedInfo.sizingGesture == GestureType::Circle)
	{
		// The storms and shields go where the circle was drawn, as big as it was
		point = command.point;
		size = command.size;
		if (!_ignoreInfluence && !_world.InInfluence(seed.player, point))
		{
			return entt::null;
		}
		if (!CanCastAt(type, seed.player, point))
		{
			return entt::null;
		}
	}
	else
	{
		point = command.point;
		if (!PointValidForHeld(point))
		{
			return entt::null;
		}
	}
	// A seed that lost the link of its last cast with no store does not cast. A linked seed lets go of its old miracle.
	// If the new miracle then does not start, the seed has no link and keeps the mark, so it does not cast again.
	if (recast == magic::SeedRecast::Refused)
	{
		return entt::null;
	}
	ReleaseSpellLink(*Held(), seed);
	// A seed that cast before casts the miracle of its last cast again. Any other seed casts the miracle of its
	// power-up level.
	const auto castType = magic::SeedCastMagicType(seed.lastCastType, type);
	auto cast = magic::SeedCastData(Info(), castType, seed.seedType, seed.castMultiplier, size);
	cast.maxObjectsToCreate = seed.storedMaxObjects;
	const bool resuming = seed.storedChants >= 0.0f;
	if (resuming)
	{
		cast.chants = seed.storedChants;
	}
	const auto heldEntity = *Held();
	// A seed made at an icon is topped up by its worship; one made without is the player's own
	const SpellCaster caster {.kind = SpellCaster::Kind::Player,
	                          .player = seed.player,
	                          .entity = entt::null,
	                          .withoutIcon = !seed.hasIcon,
	                          .worshipSite = seed.worshipSite,
	                          .madeBySpellIcon = seed.hasIcon,
	                          .icon = IconOfSeed(heldEntity, seed)};
	// The miracle starts with the hand as the command gives it
	const particles::ProcessInfo hand {
	    .handPosition = command.handPosition,
	    .cameraForward = command.cameraForward,
	    .direction = command.handVelocity,
	    .power = 1.0f,
	    .enabled = true,
	    .spin = command.spin,
	};
	const auto spellEntity =
	    object.has_value() ? CastOn(castType, caster, *object, cast, hand) : Cast(castType, caster, point, cast, hand);
	if (spellEntity == entt::null)
	{
		return entt::null;
	}
	auto& spell = registry.Get<Spell>(spellEntity);
	auto& after = registry.Get<SpellSeed>(heldEntity);
	spell.fromLocalHand = true;
	// Only a miracle held in the hand follows it
	spell.castFromHand = seedInfo.castType == SpellCastType::SpellCastInHand;
	spell.seed = heldEntity;
	if (resuming)
	{
		spell.age = after.storedAge;
	}
	after.spell = spellEntity;
	after.lastCastType = spell.magicType;
	after.linkedByCast = true;
	// The cast empties the seed's charge. The seed keeps what it stored, so a later cast continues from it again.
	after.chantStore = 0.0f;
	// The cast stops the charge of the seed's icon for this player, and the icon's store goes back to its site
	if (Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().CancelSeedIconCharge(heldEntity, after.player);
	}
	// A tribe's power behind the miracle shows for the hand of every player. A miracle cast on a thing starts at the
	// place of the thing.
	glm::vec3 place = point;
	if (object.has_value())
	{
		if (const auto* transform = registry.TryGet<const Transform>(*object))
		{
			place = transform->position;
		}
	}
	AfterSeedCast(after, type, place);
	// The hand of another player shows no other effect and gives no help on this computer
	if (!ActsOnOwnHand())
	{
		return spellEntity;
	}
	// The help counts the cast of this computer's hand, a spell for creatures apart from the other miracles
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(spell.spellClass == magic::SpellClass::Creature
		                                                ? help::profile::k_CastCreatureMiracle
		                                                : help::profile::k_CastMiracle);
	}
	// Food and wood lift and tip the hand as they pour, keeping it where it began; the water raises it eight units and
	// lowers it again over eight seconds, over and over as it rains, the hand free to move
	if (spell.castFromHand && spell.spellClass == magic::SpellClass::Resource)
	{
		magic::StartPour(_pour, magic::k_FoodWoodPour, _hand.handPosition);
	}
	else if (spell.castFromHand && spell.spellClass == magic::SpellClass::Water)
	{
		magic::StartPour(_pour, magic::k_WaterPour, _hand.handPosition);
	}
	return spellEntity;
}

void MagicSystem::AfterCast(entt::entity spellEntity)
{
	auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return;
	}
	auto& seed = registry.Get<SpellSeed>(*Held());
	const auto& seedInfo = magic::GetSpellSeedInfo(Info(), seed.seedType);
	const auto fate = magic::SeedAfterCastOf(seedInfo.isKeptInHand != 0, seedInfo.seedFollowsSpell != 0);
	if (fate == magic::SeedAfterCast::StaysInHand)
	{
		return;
	}
	// Leaving the hand, it shows the cast succeeded where it was cast
	if (const auto* spell = FindSpell(spellEntity); spell != nullptr && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().Show({.type = SpotVisualType::SpellSucceedCast, .position = spell->castPosition});
	}
	// The hand lets go of the seed, so it starts the path of its gesture again. A seed kept in the hand does not do this.
	if (ActsOnOwnHand() && Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().ForgetPath();
	}
	SeedOutOfHandWithSpell(fate == magic::SeedAfterCast::FollowsSpell);
}

void MagicSystem::SeedOutOfHandWithSpell(bool followsSpell)
{
	auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return;
	}
	// Bound to its miracle, out of the hand, and gone with it. Only a seed that follows its miracle moves with it.
	const auto entity = *Held();
	auto& seed = registry.Get<SpellSeed>(entity);
	StopHandEffect(seed);
	seed.followsSpell = followsSpell;
	registry.Remove<Mesh>(entity);
	// The turns read the place of the seed out of the hand. Thus it must not be the place that the frames drew it at.
	if (auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
	{
		const auto holder = HolderOf(entity);
		transform->position =
		    magic::PlaceOutOfHand(holder.has_value() ? ReportedHandOf(*holder) : std::nullopt, transform->position);
	}
	LetGoOfSeed(false);
}

void MagicSystem::FailCast()
{
	const auto point = _hand.point.value_or(_hand.handPosition);
	if (Locator::effectRequestSystem::has_value())
	{
		// Only the hand of this computer failed the cast, so the failure draws on the random numbers of this computer
		Locator::effectRequestSystem::value().Show({.type = SpotVisualType::SpellFailCast, .position = point, .synced = false});
	}
	if (Locator::sounds::has_value())
	{
		audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_SpellCastFailure), std::nullopt);
	}
	if (_lastHandResult != HandResult::NoCircle)
	{
		_lastHandResult = HandResult::CantCastThere;
	}
}

void MagicSystem::Do(const magic::CastActions& actions)
{
	if (actions.notReady)
	{
		_lastHandResult = HandResult::NotReady;
	}
	if (actions.stopHoldLoop)
	{
		StopHoldLoop();
	}
	if (actions.startHoldLoop)
	{
		StartHoldLoop();
		_lastHandResult = HandResult::Readied;
	}
	// The local hand starts the path of its gesture again from the last place of the pointer
	if (actions.restartGesturePath && ActsOnOwnHand() && Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().RestartPathAtArmedPress();
	}
	if (actions.unlock)
	{
		SendCastUnlock();
	}
	if (actions.fail)
	{
		FailCast();
	}
	if (actions.cast.has_value())
	{
		SendCast(*actions.cast);
	}
}
