/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a seed keeps a miracle for one more cast, how a seed is discarded, and how gestures power up the held
// seed.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <utility>

#include <glm/gtx/euler_angles.hpp>

#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
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

bool MagicSystem::StoreFromSpell(entt::entity entity, SpellSeed& seed, Spell& spell)
{
	// The seed keeps what the miracle has left, and how long it has run, for another go. Its charge becomes what the
	// miracle has left.
	seed.storedChants = spell.chants.chants;
	seed.chantStore = spell.chants.chants;
	seed.storedAge = spell.age;
	// A forest keeps the number of trees that it has now, so a cast again makes no more trees than that. The count is
	// the forest's trees at this moment, not the count of the last turn of the forest.
	if (spell.forestPlanted && Locator::forestSystem::has_value())
	{
		spell.objectCount = Locator::forestSystem::value().StandingTrees(seed.spell);
	}
	seed.storedMaxObjects = magic::spells::MaxObjectsToCreate(Info(), spell);
	const bool enough = magic::spells::HasEnoughForRecast(Info(), spell);
	// The seed lets go of the miracle. A miracle that still runs from this seed closes down.
	if (spell.seed == entity)
	{
		spell.seed = entt::null;
		CloseDown(seed.spell);
	}
	seed.spell = entt::null;
	// The store clears the mark of the cast's link, so the seed can cast again
	seed.linkedByCast = false;
	return enough;
}

void MagicSystem::ReleaseSpellLink(entt::entity entity, SpellSeed& seed)
{
	if (auto* spell = FindSpell(seed.spell); spell != nullptr && spell->seed == entity)
	{
		spell->seed = entt::null;
		CloseDown(seed.spell);
	}
	// The mark of the cast's link stays
	seed.spell = entt::null;
}

void MagicSystem::ForgetStoredSpell(SpellSeed& seed)
{
	// A seed with no miracle keeps no prayer power, age or number of objects of a miracle
	seed.storedChants = -1.0f;
	seed.storedAge = 0.0f;
	seed.storedMaxObjects = -1;
	seed.spell = entt::null;
}

void MagicSystem::DiscardHeldSeedOf(PlayerNames player)
{
	const ActingAs acting(*this, player);
	DiscardHeldSeed();
}

void MagicSystem::ShakeOffHeldSeedOf(PlayerNames player)
{
	const ActingAs acting(*this, player);
	DropHeldSeed(false);
}

void MagicSystem::DiscardHeldSeed()
{
	DropHeldSeed(true);
}

void MagicSystem::DropHeldSeed(bool handShakes)
{
	if (!Held().has_value())
	{
		return;
	}
	// The hand of this computer calls off its press. A miracle locked on in the hand stores back what it has left, as the
	// let go of the button does.
	const bool own = ActsOnOwnHand();
	if (own && magic::Cancel(_input).stopHoldLoop)
	{
		StopHoldLoop();
	}
	if (HeldSpellRunsInHand())
	{
		UnlockHeldMiracle();
	}
	auto& registry = EntityRegistry();
	if (Held().has_value() && registry.Valid(*Held()))
	{
		// What the seed still holds goes back to the player's worship
		const auto& seed = registry.Get<const SpellSeed>(*Held());
		ReturnFromSeed(seed, magic::SeedRefund(seed.hasIcon, seed.chantStore));
	}
	// When the hand shakes, its sound plays and a band flies off the hand
	if (handShakes && own && Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().HandShaken();
	}
	LetGoOfSeed(true);
	if (own)
	{
		_lastHandResult = HandResult::Discarded;
	}
}

void MagicSystem::DeleteSeed(entt::entity entity)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(entity) || !registry.AllOf<SpellSeed>(entity))
	{
		return;
	}
	auto& seed = registry.Get<SpellSeed>(entity);
	// The miracle it cast that still runs from it closes down; one that has moved on from it carries on
	if (auto* spell = FindSpell(seed.spell); spell != nullptr && spell->seed == entity)
	{
		spell->seed = entt::null;
		CloseDown(seed.spell);
	}
	seed.spell = entt::null;
	// A seed in the hand of a player leaves that hand
	if (const auto holder = HolderOf(entity); holder.has_value())
	{
		const ActingAs acting(*this, *holder);
		LetGoOfSeed(true);
		return;
	}
	registry.Destroy(entity);
}

entt::entity MagicSystem::IconOfHeldSeed() const
{
	const auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()) || !Locator::spellIconSystem::has_value())
	{
		return entt::null;
	}
	return Locator::spellIconSystem::value().IconOfSeed(*Held());
}

void MagicSystem::AskPowerUpAtIcon(entt::entity icon, int level)
{
	if (!Locator::spellIconSystem::has_value())
	{
		return;
	}
	// The icon of the command charges its seed to the level from the prayer power of its site, in the turns of the site.
	// The seed keeps its level until the icon has charged it. The icon starts the charge only while a hand of the player
	// holds its seed.
	Locator::spellIconSystem::value().StartPowerUpCharge(icon, ActingPlayer(), level);
}

void MagicSystem::ShowHeldSeedPowerUp(int level, int previous)
{
	auto& seed = EntityRegistry().Get<SpellSeed>(*Held());
	// The hand of another player shows no effect on this computer
	if (!ActsOnOwnHand())
	{
		ShowHeldSeed();
		return;
	}
	// Its in-hand effect starts afresh at the new level, shown once the seed is ready
	StopHandEffect(seed);
	StartHandEffect(*Held());
	ShowHeldSeed();
	// The hand shows the name of the miracle at the new level, then a bracelet more, bands flying on and the announcer
	AnnounceHeldMiracle(seed);
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().SeedInHand(level, previous);
	}
}

void MagicSystem::AnnounceHeldMiracle(const SpellSeed& seed)
{
	_heldMiracleToolTip = magic::GetHeldToolTipText(Info(), magic::GetSpellSeedInfo(Info(), seed.seedType), seed.powerUp);
}

std::optional<uint32_t> MagicSystem::TakeHeldMiracleToolTip()
{
	return std::exchange(_heldMiracleToolTip, std::nullopt);
}

void MagicSystem::SetIconSeedPowerUp(entt::entity entity, int powerUp)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(entity) || !registry.AllOf<SpellSeed>(entity))
	{
		return;
	}
	auto& seed = registry.Get<SpellSeed>(entity);
	const int previous = seed.powerUp;
	seed.powerUp = powerUp;
	const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(Info(), seed.seedType), powerUp);
	const float cost = magic::GetChantsRequiredToCreate(Info(), type);
	// What the seed holds past the cost of the new level goes back to the battery of its worship site
	const float spare = -magic::ChantNeeded(cost, seed.chantStore);
	if (spare > 0.0f && registry.Valid(seed.worshipSite) && Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().ReturnChants(seed.worshipSite, spare);
		seed.chantStore = seed.chantStore - spare;
	}
	seed.power = magic::SeedPower(seed.chantStore, cost);
	if (const auto holder = HolderOf(entity); holder.has_value())
	{
		const ActingAs acting(*this, *holder);
		ShowHeldSeedPowerUp(powerUp, previous);
		if (ActsOnOwnHand())
		{
			_lastHandResult = HandResult::PoweredUp;
		}
	}
}

void MagicSystem::TakeGestures()
{
	if (!Locator::gestureEvents::has_value())
	{
		return;
	}
	for (const auto& event : Locator::gestureEvents::value().TakeEvents())
	{
		switch (event.kind)
		{
		case GestureEvent::Kind::Circle:
		{
			auto centre = event.centre;
			centre.y = _world.LandHeight({centre.x, centre.z});
			_circle = Circle {.centre = centre, .radius = event.radius, .turn = _turn};
			break;
		}
		// The gestures on the seed in the hand go as commands of the player, which the turn does
		case GestureEvent::Kind::PowerUp:
			SendSeedGesture({.kind = player_commands::SeedGesture::Kind::PowerUp,
			                 .level = static_cast<int8_t>(event.powerUpLevel),
			                 .icon = IconOfHeldSeed()});
			break;
		case GestureEvent::Kind::Scribble:
			SendSeedGesture({.kind = player_commands::SeedGesture::Kind::Scribble, .level = 0});
			break;
		case GestureEvent::Kind::Shake:
			SendSeedGesture({.kind = player_commands::SeedGesture::Kind::Shake, .level = 0});
			break;
		}
	}
}

void MagicSystem::SendSeedGesture(const player_commands::SeedGesture& gesture)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(gesture);
		return;
	}
	DoSeedGesture(OwnPlayer(), gesture);
}

void MagicSystem::DoSeedGesture(PlayerNames player, const player_commands::SeedGesture& gesture)
{
	// The gesture acts on the miracle in the hand of the player that sent it
	const ActingAs acting(*this, player);
	switch (gesture.kind)
	{
	case player_commands::SeedGesture::Kind::PowerUp:
		// The scribble that calls off the power-up asked for asks for the plain miracle. The hand shakes as it does when
		// the scribble drops the miracle.
		if (gesture.level < 0 && ActsOnOwnHand() && Locator::miracleFxSystem::has_value())
		{
			Locator::miracleFxSystem::value().HandShaken();
		}
		AskPowerUpAtIcon(gesture.icon, gesture.level);
		break;
	case player_commands::SeedGesture::Kind::Scribble:
		// The hand asked for no power-up, so the scribble drops the seed
		DiscardHeldSeed();
		break;
	case player_commands::SeedGesture::Kind::Shake:
		DiscardHeldSeed();
		break;
	}
}
