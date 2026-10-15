/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AltarOffering.h"

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Audio/GameSoundEffects.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/AltarSacrifice.h"
#include "ECS/Archetypes/FloatingNumberArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace altar_offering = openblack::ecs::altar_offering;
namespace altar_sacrifice = openblack::ecs::altar_sacrifice;
namespace world_objects = openblack::ecs::world_objects;

namespace
{

/// The bank the sacrifice sound comes from, and the bank the villagers' cries come from
constexpr std::string_view k_GameBank = "InGame.sad";
constexpr std::string_view k_CryBank = "editor.sad";

void PlaySample(std::string_view bank, uint32_t sample, glm::vec3 position)
{
	if (Locator::audio::has_value())
	{
		audio::PlayGameSoundEffect(entt::hashed_string(fmt::format("{}/{}", bank, sample).c_str()).value(), position);
	}
}

uint32_t LocalRandom(uint32_t count)
{
	if (count == 0 || !Locator::gameRandom::has_value())
	{
		return 0;
	}
	return Locator::gameRandom::value().LocalRand(static_cast<int32_t>(count)) % count;
}

std::optional<altar_sacrifice::Offering> OfferingOf(const ecs::Registry& registry, entt::entity held)
{
	if (registry.AllOf<Villager>(held))
	{
		return altar_sacrifice::Offering::Villager;
	}
	if (registry.AllOf<Animal>(held))
	{
		return altar_sacrifice::Offering::Animal;
	}
	if (registry.AllOf<Tree>(held))
	{
		return altar_sacrifice::Offering::Tree;
	}
	if (registry.AllOf<DeadTree>(held))
	{
		return altar_sacrifice::Offering::DeadTree;
	}
	return std::nullopt;
}

/// The sacrifice value of a thing's kind in the game's tables: an animal's by its species, anything else's by its own row
float TableValueOf(const ecs::Registry& registry, entt::entity held)
{
	if (const auto* animal = registry.TryGet<const Animal>(held); animal != nullptr && Locator::infoConstants::has_value())
	{
		const auto& rows = Locator::infoConstants::value().animal;
		const auto row = static_cast<size_t>(animal->type);
		return row < rows.size() ? rows[row].sacrificeValue : 0.0f;
	}
	const auto* info = world_objects::InfoOf(held);
	return info != nullptr ? info->sacrificeValue : 0.0f;
}

/// An animal sacrificed counts for its sacrificer, and moves the sacrificer's alignment as an animal's death does
void CountAnimal(ecs::Registry& registry, PlayerNames player)
{
	player_statistics::CountSacrifice(registry, player);
	if (!Locator::alignmentSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& changes = Locator::infoConstants::value().player.dealthReason;
	const auto change = changes.at(static_cast<size_t>(DeathReason::Sacrifice));
	Locator::alignmentSystem::value().AddPendingAlignment(player, altar_sacrifice::AnimalAlignmentChange(change));
}

/// A light rises from the top of the altar, in the colours of the site's player
void LightAltar(entt::entity altar, PlayerNames sitePlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* at = registry.TryGet<const Transform>(altar);
	if (at == nullptr || !Locator::particleSystem::has_value())
	{
		return;
	}
	const auto top = at->position + glm::vec3(0.0f, world_objects::SizeOf(altar).height, 0.0f);
	auto& particles = Locator::particleSystem::value();
	const auto effect = particles.StartSpotVisual(SpotVisualType::ObjectAppear, top, std::nullopt, entt::null, 1.0f);
	particles.SetPlayer(effect, static_cast<int>(sitePlayer));
}

} // namespace

bool altar_offering::IsAltar(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<WorshipAltar>(object);
}

bool altar_offering::Takes(entt::entity altar, entt::entity held)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return IsAltar(altar) && registry.Valid(held) && OfferingOf(registry, held).has_value();
}

altar_offering::Answer altar_offering::Offer(entt::entity altar, entt::entity held, const Giver& giver)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!IsAltar(altar) || !registry.Valid(held))
	{
		return Answer::NotAnAltar;
	}
	const auto kind = OfferingOf(registry, held);
	const auto site = registry.Get<const WorshipAltar>(altar).site;
	auto* chants = registry.Valid(site) ? registry.TryGet<WorshipChants>(site) : nullptr;
	if (!kind.has_value() || chants == nullptr)
	{
		return Answer::NotAnAltar;
	}

	const auto* person = registry.TryGet<const Villager>(held);
	const float life = world_objects::LifeOf(held);
	const auto sacrifice = altar_sacrifice::Judge({
	    .kind = *kind,
	    .life = life,
	    .tableValue = TableValueOf(registry, held),
	    .child = person != nullptr && person->lifeStage == Villager::LifeStage::Child,
	    .indestructible = registry.AllOf<Indestructible>(held),
	    .siteBuilt = Locator::worshipSiteSystem::has_value() && Locator::worshipSiteSystem::value().IsBuilt(site),
	});
	if (!sacrifice.has_value())
	{
		return Answer::Refused;
	}

	// Its prayer power is stored at the site, however full, and counted among what was sacrificed there
	chants->battery += sacrifice->prayer;
	chants->sacrificed += sacrifice->prayer;
	if (const auto logger = spdlog::get("game"))
	{
		logger->debug("Sacrifice of kind {} at the altar: {:.1f} prayer power, the site now stores {:.1f}",
		              static_cast<int>(*kind), sacrifice->prayer, chants->battery);
	}

	const auto* transform = registry.TryGet<const Transform>(held);
	const auto position = transform != nullptr ? transform->position : giver.hand;
	// The player's creature may see the deed, and learn to sacrifice
	if (sacrifice->deed && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().PlayerDid(creature_deeds::k_Sacrifice, position, held, giver.player);
	}
	if (sacrifice->animalCounts)
	{
		CountAnimal(registry, giver.player);
	}
	if (sacrifice->villagerDies)
	{
		villager_fire::DieByEffect(held, villager_fire::DeathCause {
		                                     .reason = DeathReason::Sacrifice,
		                                     .killer = giver.player,
		                                     .weight = life,
		                                 });
	}
	if (sacrifice->light)
	{
		LightAltar(altar, registry.Get<const WorshipSite>(site).player);
	}

	// The sacrifice is heard where the hand is; a man or a woman cries out
	PlaySample(k_GameBank, altar_sacrifice::k_SacrificeSample, giver.hand);
	if (person != nullptr)
	{
		if (const auto cry =
		        altar_sacrifice::CryOf(person->lifeStage == Villager::LifeStage::Child, person->sex == Villager::Sex::FEMALE))
		{
			PlaySample(k_CryBank, cry->first + LocalRandom(cry->count), giver.hand);
		}
	}
	if (sacrifice->number && giver.local && giver.pointed.has_value())
	{
		archetypes::FloatingNumberArchetype::Create(*giver.pointed, sacrifice->prayer, altar_sacrifice::k_NumberColour);
	}

	// It goes from the hand and the world, a tree as a ghost that flickers out
	registry.Remove<InHand>(held);
	if (sacrifice->ghost)
	{
		world_objects::LeaveGhost(held);
	}
	world_objects::Remove(held);
	return Answer::Sacrificed;
}
