/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ArtefactSystem.h"

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/ObjectRules.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerDance.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace rules = openblack::ecs::artefact_rules;

namespace
{
/// The land balances a script may set: how impressive things are, and how fast artefacts gain worth
constexpr size_t k_ImpressivenessBalance = 1;
constexpr size_t k_ArtefactSpeedBalance = 6;
/// An owner's symbol floats this far above its artefact's model (in its model's half heights), or this far above the
/// top of a thing without a model
constexpr float k_SymbolHalfHeights = 1.2f;
constexpr float k_SymbolOverTop = 2.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t Turn()
{
	return Locator::time::value().GetTurn();
}

float LandBalance(size_t index)
{
	return Entities().Context().mapScriptGlobals.landBalance.at(index);
}

/// The row of the dances' table of a dance, none for one out of it
const GDanceInfo* DanceInfoOf(const Dance& dance)
{
	const auto& table = Locator::infoConstants::value().dance;
	const auto row = static_cast<size_t>(dance.type);
	return dance.type != DanceInfo::None && row < table.size() ? &table.at(row) : nullptr;
}

Dance* DanceOf(const TownArtefact& artefact)
{
	return ecs::dances::IsDance(Entities(), artefact.dance) ? &Entities().Get<Dance>(artefact.dance) : nullptr;
}

glm::vec3 PositionOf(entt::entity thing)
{
	return Entities().Get<const Transform>(thing).position;
}

/// How impressive an object's kind is, by the land's balance of impressiveness
float ImpressivenessOf(entt::entity object)
{
	const auto* info = ecs::world_objects::InfoOf(object);
	return info != nullptr ? LandBalance(k_ImpressivenessBalance) * info->impressiveValue : 0.0f;
}

/// A town's adults and children, as last counted
uint32_t PopulationOf(entt::entity town)
{
	const auto* stats = Entities().TryGet<const TownStats>(town);
	return stats != nullptr ? stats->adults + stats->children : 0;
}

bool IsTown(entt::entity town)
{
	return town != entt::null && Entities().Valid(town) && Entities().AllOf<Town>(town);
}
} // namespace

void ArtefactSystem::AddToTown(entt::entity object, entt::entity town, PlayerNames player)
{
	auto& registry = Entities();
	auto* artefact = registry.TryGet<TownArtefact>(object);
	if (artefact == nullptr)
	{
		// A new artefact is worth what its kind impresses villagers
		const auto* info = world_objects::InfoOf(object);
		artefact = &registry.Assign<TownArtefact>(
		    object,
		    TownArtefact {.town = town, .player = player, .value = info != nullptr ? info->villagerImpressiveValue : 0.0f});
		CreateEffectIfNecessary(object, *artefact);
	}
	else
	{
		artefact->town = town;
	}
	if (!artefact->inTown)
	{
		artefact->inTown = true;
		artefact->joined = ++_joined;
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().PlayerDid(physics::objects::k_DeedMakeArtefact, PositionOf(object), object,
		                                               player);
	}
}

void ArtefactSystem::AddToSite(entt::entity object, entt::entity site, PlayerNames player)
{
	auto& registry = Entities();
	auto* artefact = registry.TryGet<TownArtefact>(object);
	if (artefact == nullptr)
	{
		const auto* info = world_objects::InfoOf(object);
		artefact = &registry.Assign<TownArtefact>(
		    object,
		    TownArtefact {.site = site, .player = player, .value = info != nullptr ? info->villagerImpressiveValue : 0.0f});
		CreateEffectIfNecessary(object, *artefact);
	}
	artefact->site = site;
	artefact->inSite = true;
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().PlayerDid(physics::objects::k_DeedMakeArtefact, PositionOf(object), object,
		                                               player);
	}
}

void ArtefactSystem::Taken(entt::entity object, PlayerNames player)
{
	auto* artefact = Entities().TryGet<TownArtefact>(object);
	if (artefact == nullptr)
	{
		return;
	}
	Remove(*artefact);
	artefact->player = player;
}

void ArtefactSystem::Remove(TownArtefact& artefact)
{
	// Out of its town and its site: a dance under way sends its dancers back to their lives
	const auto leave = [this, &artefact](bool& member) {
		if (!member)
		{
			return;
		}
		member = false;
		if (const auto* dance = DanceOf(artefact);
		    dance != nullptr && dance->state == Dance::State::Dancing && artefact.dancing)
		{
			SetDancersState(artefact.dance, VillagerStates::DecideWhatToDo);
			artefact.dancing = false;
		}
	};
	leave(artefact.inTown);
	leave(artefact.inSite);
}

std::vector<entt::entity> ArtefactSystem::ArtefactsOf(entt::entity town) const
{
	std::vector<std::pair<uint64_t, entt::entity>> found;
	Entities().Each<const TownArtefact>([&found, town](entt::entity object, const TownArtefact& artefact) {
		if (artefact.inTown && artefact.town == town)
		{
			found.emplace_back(artefact.joined, object);
		}
	});
	std::ranges::sort(found, std::greater {}, &std::pair<uint64_t, entt::entity>::first);
	std::vector<entt::entity> objects;
	objects.reserve(found.size());
	std::ranges::transform(found, std::back_inserter(objects), &std::pair<uint64_t, entt::entity>::second);
	return objects;
}

void ArtefactSystem::ProcessTurn()
{
	auto& registry = Entities();
	if (!Locator::infoConstants::has_value() || !Locator::time::has_value())
	{
		return;
	}
	std::vector<entt::entity> towns;
	registry.Each<const Town>([&towns](entt::entity town, const Town&) { towns.push_back(town); });
	for (const auto town : towns)
	{
		for (const auto object : ArtefactsOf(town))
		{
			if (registry.Valid(object) && registry.AllOf<TownArtefact>(object))
			{
				Process(object, registry.Get<TownArtefact>(object));
			}
		}
	}
}

void ArtefactSystem::Process(entt::entity object, TownArtefact& artefact)
{
	// A glow that has gone out is forgotten
	if (artefact.effect != 0 &&
	    (!Locator::particleSystem::has_value() || !Locator::particleSystem::value().IsRunning(artefact.effect)))
	{
		artefact.effect = 0;
	}
	if (auto* dance = DanceOf(artefact))
	{
		if (dance->state == Dance::State::Dancing)
		{
			if (dance->dancers == 0)
			{
				// A dance left with no dancers ends
				dance->state = Dance::State::Stopped;
				artefact.dancing = false;
			}
			else
			{
				GiveBeliefIfNecessary(object, artefact);
				if (!artefact.dancing)
				{
					// The dance has started: every dancer dances
					artefact.dancing = true;
					artefact.danceStarted = Turn();
					SetDancersState(artefact.dance, VillagerStates::ArtifactDance);
				}
				else
				{
					StopDanceIfNecessary(object, artefact, false);
				}
			}
		}
		else if (artefact.dancing)
		{
			// The dance stopped by itself: it ends now
			StopDanceIfNecessary(object, artefact, true);
		}
	}
	else
	{
		artefact.dance = entt::null;
	}
	if (artefact.town != entt::null && !IsTown(artefact.town))
	{
		artefact.town = entt::null;
	}
}

void ArtefactSystem::StopDanceIfNecessary(entt::entity object, TownArtefact& artefact, bool force)
{
	auto* dance = DanceOf(artefact);
	if (dance == nullptr)
	{
		return;
	}
	const bool over = rules::DanceTimeLeft(dance->duration, Turn(), artefact.danceStarted) == 0;
	if (!over && !force)
	{
		return;
	}
	const bool townRelaxes = Locator::townDesireSystem::has_value() && artefact.town != entt::null &&
	                         Locator::townDesireSystem::value().GetMostWanted(artefact.town) == TownDesireInfo::ForRelaxation;
	const bool backToLife = !townRelaxes || force;
	// A dance in its owner's own town raises its worth
	if (IsTown(artefact.town) && artefact.player == Entities().Get<const Town>(artefact.town).owner)
	{
		const auto& town = Entities().Get<const Town>(artefact.town);
		std::optional<float> belief;
		if (town.owner != PlayerNames::NEUTRAL)
		{
			const auto* impression = Entities().TryGet<const TownImpression>(artefact.town);
			belief = impression != nullptr ? impression->belief.belief.at(static_cast<size_t>(town.owner)) : 0.0f;
		}
		const auto* info = world_objects::InfoOf(object);
		const auto* danceInfo = DanceInfoOf(*dance);
		const float gain = rules::DanceGain({
		    .townModifier = rules::TownModifier(belief),
		    .artefactMultiplier = info != nullptr ? info->artifactMultiplier : 0.0f,
		    .dancers = dance->dancers,
		    .maximumDancers = danceInfo != nullptr ? danceInfo->maximumDancers : 0,
		    .danced = Turn() - artefact.danceStarted,
		    .duration = dance->duration,
		    .artefactSpeed = LandBalance(k_ArtefactSpeedBalance),
		});
		SetValue(object, artefact, rules::AddToWorth(artefact.value, gain));
		dance = DanceOf(artefact);
		if (dance == nullptr)
		{
			return;
		}
	}
	// While the town most wants to relax, its dancers wait for the next dance; otherwise they go back to their lives
	SetDancersState(artefact.dance, backToLife ? VillagerStates::DecideWhatToDo : VillagerStates::WaitForArtifactDance);
	if (auto* stopped = DanceOf(artefact))
	{
		stopped->state = Dance::State::Stopped;
	}
	artefact.dancing = false;
}

void ArtefactSystem::GiveBeliefIfNecessary(entt::entity object, TownArtefact& artefact)
{
	auto& registry = Entities();
	if (!IsTown(artefact.town) || !artefact.player.has_value() ||
	    *artefact.player == registry.Get<const Town>(artefact.town).owner)
	{
		return;
	}
	const auto& townInfo = Locator::infoConstants::value().town;
	const uint32_t gap = townInfo.numGameTurnsBetweenGettingBeliefForArtifacts;
	auto& gifts = registry.AllOf<TownArtefactGifts>(artefact.town) ? registry.Get<TownArtefactGifts>(artefact.town)
	                                                               : registry.Assign<TownArtefactGifts>(artefact.town);
	auto& last = gifts.lastGift.at(static_cast<size_t>(*artefact.player));
	const uint32_t random = Locator::gameRandom::value().GameRand(rules::GiftRandomLimit(gap));
	if (!rules::BeliefGiftDue(Turn() - last, random, gap))
	{
		return;
	}
	last = Turn();
	const auto* dance = DanceOf(artefact);
	const float belief = rules::DancingBelief({
	    .worth = artefact.value,
	    .impressiveness = ImpressivenessOf(object),
	    .dancers = dance != nullptr ? dance->dancers : 0,
	    .population = PopulationOf(artefact.town),
	    .beliefMultiplier = townInfo.multiplierForBeliefFromVillagersDancingAroundArtifact,
	});
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().GiveTownBelief(artefact.town, *artefact.player, belief, object,
		                                                GuidanceAlignment::Good);
	}
}

bool ArtefactSystem::OfferRelaxation(entt::entity town, entt::entity villager)
{
	if (!IsTown(town) || !Locator::infoConstants::has_value())
	{
		return false;
	}
	// TODO(football): the town's football pitch is offered first, and the god's own place of the town's player before
	// the artefacts; openblack has neither yet
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto object : ArtefactsOf(town))
	{
		const float desire = JoiningDesire(object, Entities().Get<const TownArtefact>(object), villager);
		if (desire > best)
		{
			best = desire;
			chosen = object;
		}
	}
	if (best == 0.0f || chosen == entt::null)
	{
		return false;
	}
	return JoinDance(chosen, Entities().Get<TownArtefact>(chosen), villager);
}

float ArtefactSystem::JoiningDesire(entt::entity object, const TownArtefact& artefact, entt::entity villager) const
{
	const float distance = glm::distance(glm::xz(PositionOf(object)), glm::xz(PositionOf(villager)));
	rules::JoiningInputs in {.dancing = artefact.dancing, .distance = distance};
	const auto type = rules::DanceForRadius(world_objects::SizeOf(object).radius);
	const auto& table = Locator::infoConstants::value().dance;
	in.maximumDancers = table.at(static_cast<size_t>(type)).maximumDancers;
	if (const auto* dance = DanceOf(artefact))
	{
		in.dancers = dance->dancers;
		in.running = dance->state != Dance::State::Stopped;
		in.duration = dance->duration;
		in.sinceStart = Turn() - dance->startTurn;
		if (const auto* info = DanceInfoOf(*dance))
		{
			in.maximumDancers = info->maximumDancers;
		}
	}
	return rules::JoiningDesire(in, gutils::GetDistanceModifier(distance, rules::k_JoiningReach));
}

bool ArtefactSystem::JoinDance(entt::entity object, TownArtefact& artefact, entt::entity villager)
{
	auto& registry = Entities();
	const auto position = PositionOf(object);
	if (DanceOf(artefact) == nullptr && Locator::danceSystem::has_value())
	{
		// Its dance is chosen by how wide it is, made where it stands, lasting the dance's length
		const auto type = rules::DanceForRadius(world_objects::SizeOf(object).radius);
		const auto& info = Locator::infoConstants::value().dance.at(static_cast<size_t>(type));
		artefact.dance = Locator::danceSystem::value().Create(type, position, object, info.duration, false);
	}
	auto* dance = DanceOf(artefact);
	if (dance == nullptr)
	{
		return false;
	}
	// An artefact moved since its dance was set out takes its dance with it
	const auto place = map_coords::FromMetres(glm::xz(position));
	if (dance->place.x != place.x || dance->place.z != place.z || dance->place.altitude != place.altitude)
	{
		dance->place = place;
		// TODO(footpaths): footpaths are laid to it from the storage pit of the nearest town; openblack lays none yet
	}
	if (!dances::AddDancer(registry, artefact.dance, villager, villager_dance::DanceSex(villager)))
	{
		return false;
	}
	if (auto* action = registry.TryGet<LivingAction>(villager); action != nullptr && Locator::livingActionSystem::has_value())
	{
		Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top,
		                                                      VillagerStates::WaitForArtifactDance, false);
	}
	return true;
}

void ArtefactSystem::SetDancersState(entt::entity dance, VillagerStates state) const
{
	auto& registry = Entities();
	if (!Locator::livingActionSystem::has_value())
	{
		return;
	}
	for (const auto dancer : dances::Dancers(registry, dance))
	{
		if (!registry.Valid(dancer) || !registry.AllOf<Villager>(dancer))
		{
			continue;
		}
		if (auto* action = registry.TryGet<LivingAction>(dancer))
		{
			Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, state, false);
		}
	}
}

void ArtefactSystem::SetValue(entt::entity object, TownArtefact& artefact, float value)
{
	artefact.value = value;
	CreateEffectIfNecessary(object, artefact);
	if (artefact.effect == 0 || !Locator::particleSystem::has_value())
	{
		return;
	}
	auto& particles = Locator::particleSystem::value();
	if (!particles.IsRunning(artefact.effect))
	{
		artefact.effect = 0;
		return;
	}
	// The glow is as strong as it is worth
	particles.SetPower(artefact.effect, artefact.value);
}

void ArtefactSystem::CreateEffectIfNecessary(entt::entity object, TownArtefact& artefact)
{
	if (object == entt::null || artefact.effect != 0 || !rules::Glows(artefact.value))
	{
		return;
	}
	if (Locator::particleSystem::has_value())
	{
		auto& particles = Locator::particleSystem::value();
		artefact.effect = particles.StartSpotVisual(SpotVisualType::MagicFxOnObject, PositionOf(object), std::nullopt, object);
		if (artefact.effect != 0)
		{
			// It acts on its object, in its owner's colours
			const auto& spot =
			    Locator::infoConstants::value().spotVisual.at(static_cast<size_t>(SpotVisualType::MagicFxOnObject));
			if (spot.targetOwnerObject != 1)
			{
				particles.AddTarget(artefact.effect, object);
			}
			if (artefact.player.has_value())
			{
				particles.SetPlayer(artefact.effect, static_cast<int>(*artefact.player));
			}
		}
	}
	// Its owner's symbol is made with it, shown once it is worth more
	artefact.symbol = true;
}

void ArtefactSystem::PowerUpSite(entt::entity site, uint32_t dancers)
{
	auto& registry = Entities();
	if (!Locator::time::has_value() || Turn() % rules::k_SitePowerUpTurns != 0 || !registry.Valid(site) ||
	    !registry.AllOf<WorshipSite>(site) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& component = registry.Get<const WorshipSite>(site);
	const auto& table = Locator::infoConstants::value().worshipSite;
	const auto row = static_cast<size_t>(component.tribe);
	if (row >= table.size())
	{
		return;
	}
	// By how many of its player's people dance at it
	uint32_t population = 0;
	registry.Each<const Town>([&population, &component](entt::entity town, const Town& data) {
		if (data.owner == component.player)
		{
			population += PopulationOf(town);
		}
	});
	const float gain =
	    rules::SitePowerUp(LandBalance(k_ArtefactSpeedBalance), table.at(row).artifactPowerupMultiplier, dancers, population);
	if (gain == 0.0f)
	{
		return;
	}
	std::vector<entt::entity> artefacts;
	registry.Each<const TownArtefact>([&artefacts, site](entt::entity object, const TownArtefact& artefact) {
		if (artefact.inSite && artefact.site == site)
		{
			artefacts.push_back(object);
		}
	});
	for (const auto object : artefacts)
	{
		auto& artefact = registry.Get<TownArtefact>(object);
		SetValue(object, artefact, rules::AddToWorth(artefact.value, gain));
	}
}

void ArtefactSystem::UpdateFrame(float gameMilliseconds)
{
	if (!Locator::particleSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Entities();
	std::vector<particles::draw::PlayerSymbol> symbols;
	registry.Each<TownArtefact, const Transform>([&symbols, &registry, gameMilliseconds](
	                                                 entt::entity object, TownArtefact& artefact, const Transform& transform) {
		// Only a town's artefacts show their symbols, and only once worth enough
		if (!artefact.inTown || !artefact.symbol || !rules::ShowsSymbol(artefact.value) || !artefact.player.has_value())
		{
			return;
		}
		// Over the middle of its model, by more than its half height; over the top of a thing without one
		glm::vec3 at = transform.position + glm::vec3(0.0f, world_objects::SizeOf(object).height + k_SymbolOverTop, 0.0f);
		if (const auto* mesh = registry.TryGet<const Mesh>(object);
		    mesh != nullptr && Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(mesh->id))
		{
			const auto box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
			at = glm::vec3(world_objects::PlacementOf(object) * glm::vec4(box.Center(), 1.0f));
			at.y += box.Size().y * 0.5f * transform.scale.y * k_SymbolHalfHeights;
		}
		artefact.symbolMilliseconds += gameMilliseconds;
		symbols.push_back({.position = at,
		                   .scale = rules::k_SymbolScale,
		                   .milliseconds = artefact.symbolMilliseconds,
		                   .player = static_cast<int>(*artefact.player)});
	});
	Locator::particleSystem::value().SetPlayerSymbols(std::move(symbols));
}

uint32_t ArtefactSystem::CountFor(PlayerNames player) const
{
	uint32_t count = 0;
	if (!Locator::entitiesRegistry::has_value())
	{
		return count;
	}
	Entities().Each<const TownArtefact>([&count, player](const TownArtefact& artefact) {
		if (artefact.inTown && artefact.player == player)
		{
			++count;
		}
	});
	return count;
}

float ArtefactSystem::ImpressiveModifier(entt::entity object) const
{
	const auto* artefact = Locator::entitiesRegistry::has_value() && Entities().Valid(object)
	                           ? Entities().TryGet<const TownArtefact>(object)
	                           : nullptr;
	return artefact != nullptr ? rules::ImpressiveModifier(artefact->value) : 1.0f;
}
