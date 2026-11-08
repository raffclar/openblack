/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Who holds a seed, and how a hand gets a seed: from a worship site's icon, from a summon or from a globe.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <glm/gtx/euler_angles.hpp>

#include "ECS/Archetypes/SpellSeedArchetype.h"
#include "ECS/Components/HandHeldSeed.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GamePlayers.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "Help/HelpProfile.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
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
ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

/// The left hand of a player, none for a player with no hands
[[nodiscard]] entt::entity LeftHandOf(PlayerNames player)
{
	if (!Locator::handSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return entt::null;
	}
	const auto hands = Locator::handSystem::value().GetHandsOf(player);
	if (!hands.has_value())
	{
		return entt::null;
	}
	const auto hand = (*hands)[static_cast<size_t>(HandSystemInterface::Side::Left)];
	return Locator::entitiesRegistry::value().Valid(hand) ? hand : entt::null;
}
} // namespace

PlayerNames MagicSystem::OwnPlayer()
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
}

bool MagicSystem::OwnSendWaits()
{
	return Locator::handSystem::has_value() && Locator::handSystem::value().OwnSendWaits();
}

void MagicSystem::MarkOwnSend()
{
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().MarkOwnSend();
	}
}

std::optional<entt::entity> MagicSystem::HeldOf(PlayerNames player) const
{
	const auto hand = LeftHandOf(player);
	if (hand == entt::null)
	{
		return std::nullopt;
	}
	const auto* held = EntityRegistry().TryGet<const HandHeldSeed>(hand);
	return held != nullptr && held->seed != entt::null ? std::optional(held->seed) : std::nullopt;
}

bool MagicSystem::HasActingHand() const
{
	return LeftHandOf(ActingPlayer()) != entt::null;
}

void MagicSystem::SetHeld(std::optional<entt::entity> seed)
{
	const auto hand = LeftHandOf(ActingPlayer());
	if (hand == entt::null)
	{
		return;
	}
	auto& registry = EntityRegistry();
	if (auto* held = registry.TryGet<HandHeldSeed>(hand))
	{
		held->seed = seed.value_or(entt::entity {entt::null});
	}
	else if (seed.has_value())
	{
		registry.Assign<HandHeldSeed>(hand, *seed);
	}
}

std::optional<PlayerNames> MagicSystem::HolderOf(entt::entity seed) const
{
	if (seed == entt::null)
	{
		return std::nullopt;
	}
	for (const auto player : ecs::game_players::Gods())
	{
		if (HeldOf(player) == seed)
		{
			return player;
		}
	}
	return std::nullopt;
}

std::optional<glm::vec3> MagicSystem::ReportedHandOf(PlayerNames player) const
{
	const auto hand = LeftHandOf(player);
	const auto* reported = hand != entt::null ? EntityRegistry().TryGet<const ReportedHand>(hand) : nullptr;
	// All the values are zero until the first report, and off the land
	if (reported == nullptr || reported->handPoint == glm::vec3(0.0f))
	{
		return std::nullopt;
	}
	return reported->handPoint;
}

entt::entity MagicSystem::GiveSeedToHand(PlayerNames player, SpellSeedType seedType, int powerUp, float multiplier)
{
	// The seed goes into the hand of the player
	const ActingAs acting(*this, player);
	if (!HasActingHand() || Held().has_value() || !Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	// The seed starts at the hand of this computer, or where the last report put the hand of another player
	const auto start = ActsOnOwnHand() ? _hand.handPosition : ReportedHandOf(player).value_or(glm::vec3(0.0f));
	const auto entity = archetypes::SpellSeedArchetype::Create(start, seedType, player, powerUp, multiplier);
	if (entity == entt::null)
	{
		return entt::null;
	}
	auto& registry = EntityRegistry();
	auto& seed = registry.Get<SpellSeed>(entity);
	const auto& seedInfo = magic::GetSpellSeedInfo(Info(), seedType);
	const auto type = magic::GetMagicTypeFromPowerUpLevel(seedInfo, powerUp);
	// Charged in full, for nothing, and ready at once; made at the player's icon for the seed if they have one
	seed.chantStore = magic::ChantNeeded(magic::GetChantsRequiredToCreate(Info(), type), 0.0f);
	seed.origin = magic::SeedOrigin::Bubble;
	seed.hasIcon = PlayerHasSpellIcon(player, seedType);
	seed.ready = magic::ReadyAtOnce(seed.origin);
	seed.power = magic::SeedPower(seed.chantStore, magic::GetChantsRequiredToCreate(Info(), type));
	SetHeld(entity);
	// The player's last miracle is now this one: the R gesture and the R key ask for it again
	EntityRegistry().Each<components::Player>([player, seedType](entt::entity, components::Player& record) {
		if (record.name == player)
		{
			record.lastSeedGained = seedType;
		}
	});
	// The hand of another player shows no effect on this computer
	if (!ActsOnOwnHand())
	{
		ShowHeldSeed();
		return entity;
	}
	_input = {};
	// A miracle that goes into the hand makes the hand start the path of its gesture again
	if (Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().ForgetPath();
	}
	// The help counts the new seed in the hand, then the hand that took a thing
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(help::profile::k_SeedInHand);
		Locator::helpProfileSystem::value().Trigger(help::profile::k_PickUp);
	}
	// The hand takes hold of it afresh, its pour at rest; a tribe's power behind it rings the hand with its name
	_pour = {};
	StartHandEffect(entity);
	ShowHeldSeed();
	if (const auto tribe = TribalPowerTribe(player, type); tribe.has_value() && Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().StartTribalPowerRing(*tribe);
	}
	// The hand shows the name of the miracle, then takes it with its bands and bracelets
	AnnounceHeldMiracle(seed);
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().SeedInHand(powerUp, powerUp);
	}
	return entity;
}

entt::entity MagicSystem::SummonSeed(PlayerNames player, SpellSeedType seedType, int powerUp)
{
	return Summon(player, seedType, powerUp, entt::null);
}

entt::entity MagicSystem::SummonSeedAtSite(entt::entity site, SpellSeedType seedType, int powerUp)
{
	const auto& registry = EntityRegistry();
	const auto* worshipSite = registry.Valid(site) ? registry.TryGet<const WorshipSite>(site) : nullptr;
	return worshipSite != nullptr ? Summon(worshipSite->player, seedType, powerUp, site) : entt::null;
}

entt::entity MagicSystem::TakeIconSeed(entt::entity site, PlayerNames holder, SpellSeedType seedType, int powerUp, float chants)
{
	const auto& registry = EntityRegistry();
	if (!registry.Valid(site) || !registry.AllOf<WorshipSite>(site))
	{
		return entt::null;
	}
	// The seed is of the hand that takes it, also when the site is of another player
	const ActingAs acting(*this, holder);
	const auto entity = GiveSeedToHand(holder, seedType, powerUp, 1.0f);
	if (entity == entt::null)
	{
		return entt::null;
	}
	auto& seed = EntityRegistry().Get<SpellSeed>(entity);
	const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(Info(), seedType), powerUp);
	// It holds what its icon stored, made at the site's icon, and comes from the player's worship
	seed.worshipSite = site;
	seed.chantStore = chants;
	seed.power = magic::SeedPower(seed.chantStore, magic::GetChantsRequiredToCreate(Info(), type));
	seed.origin = magic::SeedOrigin::Worship;
	seed.hasIcon = true;
	seed.ready = magic::ReadyAtOnce(seed.origin);
	seed.turnsInHand = 0;
	ShowHeldSeed();
	return entity;
}

entt::entity MagicSystem::GiveOneShotSeedToHand(PlayerNames player, SpellSeedType seedType, int powerUp, float multiplier)
{
	// Find the icon before the seed goes into the hand: the icon gives a seed only to a free hand
	const auto icon =
	    Locator::spellIconSystem::has_value() ? Locator::spellIconSystem::value().BestIconFor(player, seedType) : entt::null;
	const auto entity = GiveSeedToHand(player, seedType, powerUp, multiplier);
	if (entity == entt::null)
	{
		return entt::null;
	}
	auto& seed = EntityRegistry().Get<SpellSeed>(entity);
	seed.fromGlobe = true;
	seed.hasIcon = icon != entt::null;
	if (seed.hasIcon)
	{
		auto& icons = Locator::spellIconSystem::value();
		seed.worshipSite = icons.SiteOfIcon(icon);
		icons.SeedMadeAt(icon, entity);
	}
	return entity;
}

entt::entity MagicSystem::Summon(PlayerNames player, SpellSeedType seedType, int powerUp, entt::entity site)
{
	const ActingAs acting(*this, player);
	const auto entity = GiveSeedToHand(player, seedType, powerUp, 1.0f);
	if (entity == entt::null)
	{
		return entt::null;
	}
	auto& seed = EntityRegistry().Get<SpellSeed>(entity);
	seed.worshipSite = site;
	const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(Info(), seedType), powerUp);
	const float cost = magic::GetChantsRequiredToCreate(Info(), type);
	// The worship charges it, as much as it has, and it isn't ready at once
	seed.chantStore = DrawForSeed(seed, cost);
	seed.power = magic::SeedPower(seed.chantStore, cost);
	seed.origin = magic::SeedOrigin::Worship;
	seed.hasIcon = true;
	seed.ready = magic::ReadyAtOnce(seed.origin);
	seed.turnsInHand = 0;
	// Its miracle runs in the hand from now, but shows only once it is ready
	ShowHeldSeed();
	return entity;
}

bool MagicSystem::TapGlobe(entt::entity orb, PlayerNames player)
{
	const ActingAs acting(*this, player);
	auto& registry = EntityRegistry();
	if (!registry.Valid(orb) || !registry.AllOf<OneOffSpellSeed>(orb))
	{
		return false;
	}
	const auto component = registry.Get<const OneOffSpellSeed>(orb);
	const auto seed = GiveOneShotSeedToHand(player, component.seedType, component.powerUp, component.multiplier);
	if (seed == entt::null)
	{
		return false;
	}
	if (ActsOnOwnHand())
	{
		// The hand takes the globe itself, and a globe does not show the name of its miracle
		_heldMiracleToolTip.reset();
		// The pop is heard where the hand took it
		if (Locator::sounds::has_value())
		{
			audio::StartGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_SpellBubblePop_04),
			                            {.position = _hand.handPosition, .owner = seed});
		}
		_lastHandResult = HandResult::TookMiracle;
	}
	DestroyOrb(orb);
	return true;
}
