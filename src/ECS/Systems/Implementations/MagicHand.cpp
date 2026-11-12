/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand that holds a seed: the effect and the hum of the seed, and the rules that tell where the seed can
// go.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <optional>

#include <glm/gtx/euler_angles.hpp>

#include "ECS/Components/Creature.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellSeedRules.h"
#include "MagicSpellLinks.h"
#include "MagicSystem.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"
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

// The hand

void MagicSystem::UpdateHand(const HandFrame& mouseFrame, const input_steps::Steps& steps)
{
	auto frame = _driven.has_value() ? *_driven : mouseFrame;
	// The hand a scenario puts is placed from there as the mouse's hand is: kept where a pour began, lifted by the pour
	// and by what it holds
	if (_driven.has_value())
	{
		frame.handPosition = mouseFrame.handPosition;
	}
	// A miracle coming to the hand starts its movement and spin afresh
	const auto held = Held().value_or(entt::null);
	if (held != _spinningSeed)
	{
		_spinningSeed = held;
		_handSpin = {};
		if (held != entt::null)
		{
			_handVelocity = glm::vec3(0.0f);
		}
	}
	const auto frameStart = _lastFrameHandPosition.value_or(frame.handPosition);
	for (uint32_t i = 0; i < steps.count; ++i)
	{
		const auto position = input_steps::Between(frameStart, frame.handPosition, steps.Point(i));
		if (_lastHandPosition.has_value())
		{
			constexpr float k_Seconds = input_steps::k_StepSeconds;
			const auto step = position - *_lastHandPosition;
			_handVelocity = magic::FilterHandVelocity(_handVelocity, magic::HandStepVelocity(step, k_Seconds), k_Seconds);
			// While it holds a miracle, how the way it moves turns is the spin it gives the miracle
			if (held != entt::null)
			{
				magic::StepHandSpin(_handSpin, step, _handVelocity, k_Seconds);
			}
		}
		_lastHandPosition = position;
	}
	_lastFrameHandPosition = frame.handPosition;
	_hand = frame;
}

void MagicSystem::StartHandEffect(entt::entity entity)
{
	auto& registry = EntityRegistry();
	auto& seed = registry.Get<SpellSeed>(entity);
	const auto& seedInfo = magic::GetSpellSeedInfo(Info(), seed.seedType);
	const auto type = magic::GetMagicTypeFromPowerUpLevel(seedInfo, seed.powerUp);
	// Only this computer shows it. It steps with the frames that this computer draws, so it uses the random numbers of
	// this computer. It draws immediately after the hand, and does not sort with the other effects.
	const auto info = HandEffectInfo();
	seed.handEffect = Locator::effectRequestSystem::value().StartHere({
	    .type = magic::GetMagicInfo(Info(), type).particleTypeInHand,
	    .position = info.handPosition,
	    .magnitude = _handScale,
	    .synced = false,
	    .player = static_cast<int>(seed.player),
	    .direction = glm::vec3(0.0f),
	    .processInfo = info,
	    .sink = _handEffectLink.get(),
	    .drawnWithHand = true,
	    .hidden = !seed.ready,
	});
	if (seed.handEffect != k_NoEffectHandle)
	{
		_handAtStep = info.handPosition;
	}
}

void MagicSystem::StopHandEffect(SpellSeed& seed)
{
	if (seed.handEffect != k_NoEffectHandle && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().Remove(seed.handEffect);
	}
	seed.handEffect = k_NoEffectHandle;
}

void MagicSystem::StartHoldLoop()
{
	StopHoldLoop();
	if (Locator::sounds::has_value() && Held().has_value())
	{
		_holdLoop = audio::StartGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_HandGesture_02),
		                                        {.position = _hand.handPosition,
		                                         .playType = audio::PlayType::Repeat,
		                                         .owner = *Held(),
		                                         .voiceOwner = audio::VoiceOwner::OwnSound,
		                                         .follows = audio::Follows::Place});
	}
}

void MagicSystem::StopHoldLoop()
{
	// Cut at once, as the hand's hum is
	if (_holdLoop != entt::null && Locator::sounds::has_value() && Locator::sounds::value().EmitterExists(_holdLoop))
	{
		Locator::sounds::value().StopEmitter(_holdLoop);
	}
	_holdLoop = entt::null;
}

void MagicSystem::LetGoOfSeed(bool destroy)
{
	auto& registry = EntityRegistry();
	// Only the hand of this computer has a hum, a pour, an input and the effects of the hand
	if (ActsOnOwnHand())
	{
		StopHoldLoop();
		magic::StopPour(_pour);
		_input = {};
		if (Held().has_value() && Locator::miracleFxSystem::has_value())
		{
			Locator::miracleFxSystem::value().SeedLeftHand();
			Locator::miracleFxSystem::value().StopTribalPowerRing();
		}
	}
	if (Held().has_value() && registry.Valid(*Held()))
	{
		auto& seed = registry.Get<SpellSeed>(*Held());
		StopHandEffect(seed);
		if (destroy)
		{
			if (auto* spell = FindSpell(seed.spell); spell != nullptr && spell->seed == *Held())
			{
				spell->seed = entt::null;
			}
			registry.Destroy(*Held());
		}
	}
	SetHeld(std::nullopt);
}

MagicType MagicSystem::HeldMagicType() const
{
	if (!Held().has_value() || !EntityRegistry().Valid(*Held()))
	{
		return MagicType::None;
	}
	const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
	return magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(Info(), seed.seedType), seed.powerUp);
}

bool MagicSystem::IsOwnDispenser(entt::entity object, PlayerNames player) const
{
	return EntityRegistry().Valid(object) && EntityRegistry().AllOf<SpellDispenser>(object) &&
	       world_objects::PlayerOf(object) == player;
}

std::optional<entt::entity> MagicSystem::HeldSeedTarget() const
{
	if (!Held().has_value() || !EntityRegistry().Valid(*Held()))
	{
		return std::nullopt;
	}
	const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
	// The player's own dispenser comes first: it takes a seed that did not cast before, and is no target for any other
	if (const auto picked = PickedUnderHand(); picked.has_value() && IsOwnDispenser(*picked, seed.player))
	{
		return IsHeldSeedTarget(*picked) ? picked : std::nullopt;
	}
	if (!Locator::creatureHandSystem::has_value())
	{
		return std::nullopt;
	}
	const auto target = Locator::creatureHandSystem::value().CreatureUnderCursor();
	if (!target.has_value() || !IsHeldSeedTarget(*target))
	{
		return std::nullopt;
	}
	return target;
}

bool MagicSystem::IsHeldSeedTarget(entt::entity object) const
{
	const auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()) || !registry.Valid(object))
	{
		return false;
	}
	const auto& seed = registry.Get<const SpellSeed>(*Held());
	// The player's own dispenser takes a seed that did not cast before
	if (IsOwnDispenser(object, seed.player))
	{
		return seed.lastCastType == MagicType::None && !registry.AllOf<ScriptHighlight>(object);
	}
	// Any other target is a creature, for a seed that is cast on things
	return magic::GetSpellSeedInfo(Info(), seed.seedType).castOnObject != 0 && registry.AllOf<Creature>(object) &&
	       CanCastOn(HeldMagicType(), object);
}

bool MagicSystem::PressThingGone() const
{
	if (_pressThing == entt::null || _input.state != magic::CastInput::State::Armed)
	{
		return false;
	}
	const auto& registry = EntityRegistry();
	return !registry.Valid(_pressThing) ||
	       (!registry.AllOf<Creature>(_pressThing) && !registry.AllOf<SpellDispenser>(_pressThing));
}

void MagicSystem::LetGoIfOutsideCastRule()
{
	// Only the local hand's own miracle that runs in it; over nothing the hand keeps it
	if (!Held().has_value() || !_hand.point.has_value() || !EntityRegistry().Valid(*Held()))
	{
		return;
	}
	const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
	const auto* spell = FindSpell(seed.spell);
	if (spell == nullptr || spell->closedDown || !spell->castFromHand || spell->caster.kind != SpellCaster::Kind::Player)
	{
		return;
	}
	if (magic::spells::MeetsCastRule(*this, spell->magicType, spell->caster.player, *_hand.point, _ignoreInfluence))
	{
		return;
	}
	// The interface lock of the hand ends, also when the button is not down. The end lets go of the locked miracle.
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().EndOwnInterfaceLock();
	}
	else
	{
		StopActionAtLockEnd();
	}
}

bool MagicSystem::HandPointValid() const
{
	return _hand.point.has_value() && PointValidForHeld(*_hand.point);
}

entt::entity MagicSystem::IconOfSeed(entt::entity seedEntity, const SpellSeed& seed) const
{
	const auto& registry = EntityRegistry();
	if (seed.worshipSite == entt::null)
	{
		return entt::null;
	}
	entt::entity ofKind = entt::null;
	entt::entity holder = entt::null;
	registry.Each<const WorshipIcon>([&](entt::entity icon, const WorshipIcon& data) {
		if (data.site != seed.worshipSite)
		{
			return;
		}
		if (holder == entt::null && std::ranges::find(data.seedsOut, seedEntity) != data.seedsOut.end())
		{
			holder = icon;
		}
		if (ofKind == entt::null && data.seed == seed.seedType)
		{
			ofKind = icon;
		}
	});
	return holder != entt::null ? holder : ofKind;
}

bool MagicSystem::PointValidForHeld(glm::vec3 point) const
{
	if (!Held().has_value() || !EntityRegistry().Valid(*Held()))
	{
		return false;
	}
	const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
	auto& self = const_cast<MagicSystem&>(*this);
	// Every cast from the hand needs the hand in the player's influence, whatever the miracle's own rule
	if (!_ignoreInfluence && !_world.HandInInfluence(seed.player, point))
	{
		return false;
	}
	return self.CanCastAt(HeldMagicType(), seed.player, point);
}

bool MagicSystem::HeldSpellRunsInHand() const
{
	if (!Held().has_value() || !EntityRegistry().Valid(*Held()))
	{
		return false;
	}
	const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
	const auto* spell = const_cast<MagicSystem&>(*this).FindSpell(seed.spell);
	return spell != nullptr && !spell->closedDown && spell->castFromHand;
}
