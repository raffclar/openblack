/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerReactions.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/InfluenceCircle.h"
#include "3D/L3DMesh.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/TownDesire.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerMemory.h"
#include "ECS/VillagerNeeds.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ReactionRules.h"
#include "Magic/VillagerReactionRules.h"
#include "MagicLiving.h"
#include "ObjectMeasures.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerAnimate.h"
#include "VillagerCover.h"
#include "VillagerCreatureReactions.h"
#include "VillagerDance.h"
#include "VillagerFire.h"
#include "VillagerForester.h"
#include "VillagerHome.h"
#include "VillagerMourning.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// A building at least this wide, in metres from its middle, is danced round by the large building dance
constexpr float k_LargeBuildingRadius = 10.0f;
/// A watching villager turns this much a turn towards what it watches: an eighth of a half turn
constexpr float k_WatchTurn = std::numbers::pi_v<float> / 8.0f;

/// The share a town takes is padded by this, so that an empty town takes a share at all
constexpr float k_BeliefShareSmall = 0.001f;
/// A symbol of belief is worth the belief by this many
constexpr float k_BeliefSpriteScale = 10000.0f;
/// The guidance's voices of a town's awe, good and evil, the most awed first, by their place in its sound bank
constexpr std::array<uint32_t, 3> k_GoodAwe {187, 188, 189};
constexpr std::array<uint32_t, 3> k_EvilAwe {174, 175, 176};
/// A voice of awe is heard no sooner than this many turns after the last, and up to this many more
constexpr uint32_t k_VoiceLeastGap = 30;
constexpr float k_VoiceGapSpread = 150.0f;
/// The guidance's first words seed their clock up to this many turns back
constexpr uint32_t k_GuidanceLeastGap = 50;
/// A voice is wanted less within this many turns of the guidance speaking
constexpr float k_GuidanceQuietTurns = 50.0f;
/// The belief shares are padded by this
constexpr float k_VoiceShareSmall = 0.0001f;
/// A voice is heard within this many metres of the hand, fully within a third of it, and only when wanted more than this
constexpr float k_VoiceReach = 200.0f;
constexpr float k_VoiceNearShare = 0.333f;
constexpr float k_VoiceWanted = 0.3f;
constexpr uint32_t k_VoicePitch = 100;

ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

/// The land balances of how fast villagers go and how belief speeds them
constexpr size_t k_SpeedBalance = 4;
constexpr size_t k_BeliefSpeedBalance = 7;

/// The state a villager is to end up in: the state it is in if that is an end in itself, else where it is going
VillagerStates FinalStateOf(const LivingAction& action);

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

VillagerStates FinalStateOf(const LivingAction& action)
{
	const auto top = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Top)));
	return StateInfo(top).isFinalState != 0
	           ? top
	           : static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final)));
}

VillagerStates StateOf(const LivingAction& action, LivingAction::Index index)
{
	return Locator::livingActionSystem::value().VillagerGetState(action, index);
}

void SetTop(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

/// What the villager reacts to and where it is now, none once its reaction has gone
std::optional<ReactionSystemInterface::Active> ReactionOf(entt::entity villager)
{
	const auto* state = EntityRegistry().TryGet<const LivingReaction>(villager);
	if (state == nullptr || state->reaction == 0 || !Locator::reactionSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::reactionSystem::value().Find(state->reaction);
}

/// The villager stops reacting and goes back to what it was doing
uint32_t GiveUp(LivingAction& action)
{
	villager_reactions::StopReacting(action);
	return 0;
}

/// Whether the villager holds a reaction: one of the reactions going on, or a fire's
bool HoldsReaction(entt::entity villager)
{
	const auto* state = EntityRegistry().TryGet<const LivingReaction>(villager);
	const auto* fire = EntityRegistry().TryGet<const VillagerFireState>(villager);
	return (state != nullptr && state->reaction != 0) || (fire != nullptr && fire->reaction != 0);
}

/// The villager turns towards a point by at most a step a turn: whether it now faces it
bool TurnTowards(entt::entity villager, const glm::vec3& point, float step = k_WatchTurn)
{
	auto& registry = EntityRegistry();
	auto& transform = registry.Get<Transform>(villager);
	auto& wallHug = registry.Get<WallHug>(villager);
	const auto diff = glm::xz(point) - glm::xz(transform.position);
	if (diff == glm::vec2(0.0f))
	{
		return true;
	}
	const float wanted = std::atan2(diff.y, diff.x);
	float turn = std::remainder(wanted - wallHug.yAngle, 2.0f * std::numbers::pi_v<float>);
	const bool faces = std::abs(turn) < step;
	if (!faces)
	{
		turn = std::copysign(step, turn);
	}
	wallHug.yAngle += turn;
	// Turning the walker turns the way its walk faces as well
	wallHug.gameAngle = static_cast<uint16_t>(gutils::ConvertAngle3DToGame(wallHug.yAngle));
	transform.rotation = glm::eulerAngleY(-wallHug.yAngle - std::numbers::pi_v<float> * 0.5f);
	return faces;
}
/// The awe of a villager's people at a miracle, heard now and then near the hand
void BeliefVoiceOf(entt::entity source, entt::entity town, PlayerNames player, GuidanceAlignment alignment,
                   villager_reactions::BeliefVoice& voice, uint32_t turn)
{
	auto& registry = EntityRegistry();
	if (!Locator::gameRandom::has_value() || !Locator::audio::has_value() || town == entt::null || !registry.Valid(town))
	{
		return;
	}
	auto& random = Locator::gameRandom::value();
	if (!voice.started)
	{
		voice.started = true;
		voice.lastVoice = turn - random.LocalRand(static_cast<int32_t>(k_VoiceLeastGap));
		voice.lastGuidance = turn - random.LocalRand(static_cast<int32_t>(k_GuidanceLeastGap));
	}
	// Only where the player doesn't lead the town's belief already
	const auto* impression = registry.TryGet<const TownImpression>(town);
	if (impression == nullptr)
	{
		return;
	}
	float most = 0.0f;
	for (const float belief : impression->belief.belief)
	{
		most = std::max(most, belief);
	}
	const float believed = impression->belief.belief.at(static_cast<size_t>(player));
	if (!(believed < most))
	{
		return;
	}
	// Now and then: a while after the last, more often a longer while
	const float r = random.LocalFloatRand(1.0f);
	const auto spread = static_cast<int32_t>(k_VoiceGapSpread * (1.0f - r * r * r));
	const uint32_t gap = k_VoiceLeastGap + random.LocalRand(spread);
	if (!(turn - voice.lastVoice > gap))
	{
		return;
	}
	float share = (believed + k_VoiceShareSmall) / (most + k_VoiceShareSmall);
	share -= random.LocalFloatRand(share / 3.0f);
	if (alignment != GuidanceAlignment::Good && alignment != GuidanceAlignment::Evil)
	{
		alignment = random.LocalRand(2) != 0 ? GuidanceAlignment::Evil : GuidanceAlignment::Good;
	}
	const auto& samples = alignment == GuidanceAlignment::Good ? k_GoodAwe : k_EvilAwe;
	std::optional<uint32_t> sample;
	if (share >= 0.7f)
	{
		sample = samples[0];
	}
	else if (share >= 0.4f)
	{
		sample = samples[1];
	}
	else if (share > 0.05f)
	{
		sample = samples[2];
	}
	if (!sample.has_value())
	{
		return;
	}
	// Heard near enough the hand, and not straight after the guidance has spoken
	const auto& at = registry.Get<const Transform>(source).position;
	float distance = k_VoiceReach;
	if (Locator::handSystem::has_value())
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
		if (const auto* handAt = registry.Valid(hand) ? registry.TryGet<const Transform>(hand) : nullptr)
		{
			distance = glm::distance(glm::xz(handAt->position), glm::xz(at));
		}
	}
	const float away = std::min(distance / k_VoiceReach, 1.0f);
	const float since = std::min(static_cast<float>(turn - voice.lastGuidance) / k_GuidanceQuietTurns, 1.0f);
	if (!((1.0f - away * away) * since > k_VoiceWanted))
	{
		return;
	}
	voice.lastVoice = turn;
	audio::StartGameSoundEffect(resources::HashIdentifier(fmt::format("Guidance.sad/{}", *sample)),
	                            {.position = at,
	                             .pitchPercent = k_VoicePitch,
	                             .minDistance = k_VoiceReach * k_VoiceNearShare,
	                             .maxDistance = k_VoiceReach});
}
/// The dance a new building's reaction is danced round, made as the first villager joins: the small or the large
/// building dance by how wide the building is. Whether the villager joined a group of it (else it has no room); none
/// when there is no dance to join.
std::optional<bool> JoinNewBuildingDance(entt::entity villager, uint32_t reactionId)
{
	auto& registry = EntityRegistry();
	if (!Locator::reactionSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	auto& reactions = Locator::reactionSystem::value();
	const auto reaction = reactions.Find(reactionId);
	if (!reaction.has_value())
	{
		return std::nullopt;
	}
	auto dance = reaction->dance;
	if (dance == entt::null || !registry.Valid(dance))
	{
		const auto building = reaction->source.initiator;
		if (!Locator::danceSystem::has_value() || !registry.Valid(building) || !registry.AllOf<Transform>(building))
		{
			return std::nullopt;
		}
		const auto type = object_measures::TwoDRadius(registry, building) < k_LargeBuildingRadius ? DanceInfo::BuildingSmall
		                                                                                          : DanceInfo::BuildingLarge;
		const auto& info = Locator::infoConstants::value().dance.at(static_cast<size_t>(type));
		dance = Locator::danceSystem::value().Create(type, registry.Get<const Transform>(building).position, building,
		                                             info.duration, false);
		if (dance == entt::null)
		{
			return std::nullopt;
		}
		reactions.SetDance(reactionId, dance);
	}
	return ecs::dances::AddDancer(registry, dance, villager, ecs::villager_dance::DanceSex(villager));
}
} // namespace

bool villager_reactions::Available(entt::entity villager, Reaction type)
{
	auto& registry = EntityRegistry();
	const auto* component = registry.TryGet<const Villager>(villager);
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (component == nullptr || action == nullptr || !Locator::livingActionSystem::has_value())
	{
		return false;
	}
	// Judged by the state it is to end up in, which must let it react, and not be dying, dead or past reacting
	const auto state = FinalStateOf(*action);
	if (StateInfo(state).availableForReaction == 0)
	{
		return false;
	}
	switch (state)
	{
	case VillagerStates::InDance:
	case VillagerStates::SetDying:
	case VillagerStates::Dying:
	case VillagerStates::Dead:
	case VillagerStates::Drowning:
	case VillagerStates::Downed:
	case VillagerStates::BeingEaten:
	case VillagerStates::WaitForAnimation:
		return false;
	default:
		break;
	}
	// Nor dying where it is now
	const auto now = StateOf(*action, LivingAction::Index::Top);
	if (now == VillagerStates::SetDying || now == VillagerStates::Dying)
	{
		return false;
	}
	// Too weak to react to anything but food
	// TODO(raffclar): the game also keeps a villager scripts control, one on a structure and two flags whose meaning is
	// unknown from reacting; none of them is modelled
	return type == Reaction::ReactToFood || component->life > InfoOf(*component).lifeWhenCrawlsWounded;
}

villager_reactions::Taken villager_reactions::Start(entt::entity villager, Reaction type, LivingReaction& state,
                                                    bool wasReacting)
{
	auto& registry = EntityRegistry();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr)
	{
		return Taken::Impressed;
	}
	// A new building: it joins the dance round it, or with no room in the dance looks at it
	if (type == Reaction::ReactToNewBuilding)
	{
		const auto joined = JoinNewBuildingDance(villager, state.reaction);
		if (!joined.has_value())
		{
			return Taken::No;
		}
		if (!wasReacting)
		{
			villager_reactions::RememberStateToGoBackTo(*action);
		}
		SetTop(*action, *joined ? VillagerStates::DanceWhileReacting : VillagerStates::LookingAtObjectReaction);
		return *joined ? Taken::NotImpressed : Taken::Impressed;
	}
	// A creature: its town starts to watch it, and by how the town feels about it the villager inspects it or runs; an
	// indifferent town's villager doesn't take it up
	if (type == Reaction::ReactToCreature)
	{
		const auto creature = ReactingTo(villager);
		const auto first = creature.has_value() ? ecs::villager_creature::Setup(villager, *creature) : std::nullopt;
		if (!first.has_value())
		{
			return Taken::No;
		}
		if (!wasReacting)
		{
			villager_reactions::RememberStateToGoBackTo(*action);
		}
		SetTop(*action, *first);
		// TODO(villager-social): the game impresses the villager by the creature's last action, its size and its town's
		// feeling; that measure isn't ported yet, so the creature impresses no one
		return Taken::NotImpressed;
	}
	// A hunter: one on its way to hide counts its time inside afresh and is neither taken up nor impressed; any other
	// is frightened by it
	if (type == Reaction::FleeFromPredator)
	{
		if (FinalStateOf(*action) == VillagerStates::GoAndHideInNearbyBuilding)
		{
			ecs::villager_cover::ResetHidingTurns(villager);
			return Taken::No;
		}
		if (!wasReacting)
		{
			villager_reactions::RememberStateToGoBackTo(*action);
		}
		SetTop(*action, VillagerStates::FleeingFromPredatorReaction);
		return Taken::Impressed;
	}
	if (!wasReacting)
	{
		villager_reactions::RememberStateToGoBackTo(*action);
	}
	switch (type)
	{
	case Reaction::FleeFromObject:
	case Reaction::FleeFromSpell:
	case Reaction::ReactToBurningObjectInHand:
		SetTop(*action, VillagerStates::FleeingFromObjectReaction);
		break;
	case Reaction::LookAtObject:
	case Reaction::LookAtSpell:
	case Reaction::LookAtNiceSpell:
	case Reaction::ReactToImpressiveSpell:
	case Reaction::ReactToScaffold:
		// It looks at it: a thing that came to rest, a miracle, a scaffold put up for a building
		SetTop(*action, VillagerStates::LookingAtObjectReaction);
		break;
	case Reaction::ReactToTeleport:
		// It turns aside into the stone
		if (Locator::teleportSystem::has_value())
		{
			if (const auto reaction = Locator::reactionSystem::value().Find(state.reaction))
			{
				Locator::teleportSystem::value().SetupReact(reaction->source.initiator, villager);
			}
		}
		break;
	case Reaction::ReactToFlyingObject:
		// It points at what flies by, or runs from it coming too near
		if (Locator::reactionSystem::has_value())
		{
			if (const auto reaction = Locator::reactionSystem::value().Find(state.reaction))
			{
				villager_physics::SetupReactToFlyingObject(villager, reaction->source.initiator);
			}
		}
		break;
	case Reaction::ReactToFood:
		// It goes to the food
		SetTop(*action, VillagerStates::GotoFoodReaction);
		break;
	case Reaction::ReactToWood:
		// It goes to the wood, remembering it
		if (Locator::reactionSystem::has_value())
		{
			if (const auto reaction = Locator::reactionSystem::value().Find(state.reaction))
			{
				ecs::villager_forester::SetupReactToWood(villager, reaction->source.initiator);
			}
		}
		SetTop(*action, VillagerStates::GotoWoodReaction);
		break;
	case Reaction::ReactToVillagerInHand:
		// It waits for the one held, a mate to be
		SetTop(*action, VillagerStates::WaitForMate);
		break;
	case Reaction::ReactToBreeder:
		// It goes to the breeder to make love
		SetTop(*action, VillagerStates::ReactToBreeder);
		break;
	case Reaction::ReactToTownCelebration:
		// Its own town celebrating with no town centre, it looks at the town
		// TODO(villager-social): the game also makes a dance about the town (the dance about a person) that no one is
		// put in; whether a town's dances draw dancers of their own is not known yet
		SetTop(*action, VillagerStates::LookingAtObjectReaction);
		break;
	case Reaction::ReactToDeath:
		// It points at the dead, goes to look or, at a dead creature, watches it
		if (const auto dead = ReactingTo(villager); dead.has_value())
		{
			ecs::villager_mourning::Setup(*action, *dead);
		}
		break;
	case Reaction::ReactToFire:
		// The fire's own states take over, from the object burning
		if (Locator::reactionSystem::has_value())
		{
			if (const auto reaction = Locator::reactionSystem::value().Find(state.reaction))
			{
				villager_fire::SetupReactToFire(villager, reaction->source.initiator, state.reaction);
			}
		}
		break;
	default:
		break;
	}
	return Taken::Impressed;
}

bool villager_reactions::ActsOut(Reaction type)
{
	switch (type)
	{
	case Reaction::FleeFromObject:
	case Reaction::FleeFromSpell:
	case Reaction::ReactToBurningObjectInHand:
	case Reaction::LookAtObject:
	case Reaction::LookAtSpell:
	case Reaction::ReactToScaffold:
	case Reaction::LookAtNiceSpell:
	case Reaction::ReactToImpressiveSpell:
	case Reaction::ReactToTeleport:
	case Reaction::ReactToFlyingObject:
	case Reaction::ReactToFood:
	case Reaction::ReactToWood:
	case Reaction::ReactToVillagerInHand:
	case Reaction::ReactToBreeder:
	case Reaction::ReactToDeath:
	case Reaction::ReactToFire:
	case Reaction::ReactToNewBuilding:
	case Reaction::ReactToTownCelebration:
	case Reaction::ReactToCreature:
	case Reaction::FleeFromPredator:
		return true;
	default:
		return false;
	}
}

void villager_reactions::Stop(entt::entity villager, LivingReaction& state, bool resetState)
{
	auto& registry = EntityRegistry();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (!resetState || action == nullptr)
	{
		auto* wallHug = registry.TryGet<WallHug>(villager);
		if (const auto* component = registry.TryGet<const Villager>(villager); component != nullptr && wallHug != nullptr)
		{
			wallHug->speed = GetSpeedStateSpeed(InfoOf(*component).speedGroup.speedDefault);
		}
		return;
	}
	// TODO(raffclar): a reaction villagers don't act out yet left them doing what they were doing, so there is nothing
	// to go back from; it is only let go. Once each is acted out it ends as every other does.
	if (!ActsOut(state.type))
	{
		LetGo(villager);
		return;
	}
	StopReacting(*action);
}

void villager_reactions::RememberStateToGoBackTo(LivingAction& action)
{
	villager_memory::StorePreviousState(action);
}

void villager_reactions::GoBackToRemembered(LivingAction& action, bool firemanDecidesWhenNothingRemembered)
{
	auto& living = Locator::livingActionSystem::value();
	const auto remembered = StateOf(action, LivingAction::Index::Previous);
	auto resume = static_cast<VillagerStates>(StateInfo(remembered).resumeState);
	if (firemanDecidesWhenNothingRemembered)
	{
		resume = villager_fire::StateToGoBackTo(resume);
	}
	if (!living.VillagerChangeTopState(action, resume))
	{
		// The states it is in won't let it go: it is put straight into deciding what to do, with none of their rules
		living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo, true);
	}
	living.VillagerSetState(action, LivingAction::Index::Previous, VillagerStates::InvalidState, true);
}

void villager_reactions::GoBackAfterReacting(LivingAction& action)
{
	const auto villager = EntityRegistry().ToEntity(action);
	const auto* state = EntityRegistry().TryGet<const LivingReaction>(villager);
	const bool fireman = villager_fire::IsFireMan(villager) || (state != nullptr && state->type == Reaction::ReactToFire);
	GoBackToRemembered(action, fireman);
	// Still heading for a reaction's state, with the reaction over, it decides afresh
	if (StateInfo(FinalStateOf(action)).isReactionState != 0)
	{
		SetTop(action, VillagerStates::DecideWhatToDo);
	}
}

void villager_reactions::LetGo(entt::entity villager)
{
	auto& registry = EntityRegistry();
	// Dancing in its reaction's dance, it leaves the dance
	if (const auto* action = registry.TryGet<const LivingAction>(villager);
	    action != nullptr && Locator::livingActionSystem::has_value() &&
	    StateOf(*action, LivingAction::Index::Top) == VillagerStates::DanceWhileReacting &&
	    ecs::dances::DanceOf(registry, villager) != entt::null)
	{
		ecs::dances::RemoveDancer(registry, villager);
	}
	if (auto* fire = registry.TryGet<VillagerFireState>(villager))
	{
		fire->reaction = 0;
		fire->reactionTarget = entt::null;
	}
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().StopReacting(villager);
	}
	else if (auto* state = registry.TryGet<LivingReaction>(villager))
	{
		state->reaction = 0;
		state->type = Reaction::None;
	}
}

uint32_t villager_reactions::StopReacting(LivingAction& action)
{
	const auto villager = EntityRegistry().ToEntity(action);
	GoBackAfterReacting(action);
	if (HoldsReaction(villager))
	{
		LetGo(villager);
	}
	return 0;
}

bool villager_reactions::ExitReaction(LivingAction& action, VillagerStates next)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	// The circle it was walking round, and any trial of going round it the other way, are forgotten
	registry.Remove<WallHugObjectReference, WallHugLookahead>(villager);
	if (StateInfo(next).isReactionState == 0 && HoldsReaction(villager))
	{
		LetGo(villager);
	}
	return false;
}

uint32_t villager_reactions::Fleeing(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto reaction = ReactionOf(villager);
	const auto* info = reaction.has_value() && Locator::infoConstants::has_value()
	                       ? &Locator::infoConstants::value().reaction.at(static_cast<size_t>(reaction->source.type))
	                       : nullptr;
	if (info == nullptr)
	{
		return GiveUp(action);
	}
	const auto here = registry.Get<const Transform>(villager).position;
	const auto object = reaction->source.position;
	const float distance = glm::distance(here, object);
	if (distance > info->maxDistanceToRunAwayFromObject)
	{
		return GiveUp(action);
	}
	const bool moving = glm::length(reaction->velocity) > 0.0f;
	// Far enough from what isn't coming at it, it turns and watches
	if (distance > info->minDistanceToRunAwayFromObject && !magic::ComingTowards(here, object, reaction->velocity))
	{
		SetTop(action, VillagerStates::FleeingAndLookingAtObjectReaction);
		TurnTowards(villager, object);
		return 0;
	}
	glm::vec3 to = magic::FleePointFromStill(here, object);
	if (moving && Locator::gameRandom::has_value())
	{
		auto& random = Locator::gameRandom::value();
		const float x = random.GameFloatRand(magic::k_FleeJitter);
		const float z = random.GameFloatRand(magic::k_FleeJitter);
		to = magic::FleePointFromMoving(here, object, reaction->velocity, x, z);
	}
	// It runs there at its speed for fleeing and watching, then watches
	villager_home::SetupMoveTo(action, glm::xz(to), VillagerStates::FleeingAndLookingAtObjectReaction);
	SetStateSpeed(villager, FinalStateOf(action));
	return 0;
}

std::optional<entt::entity> villager_reactions::ReactingTo(entt::entity villager)
{
	const auto reaction = ReactionOf(villager);
	if (!reaction.has_value() || reaction->source.initiator == entt::null ||
	    !EntityRegistry().Valid(reaction->source.initiator))
	{
		return std::nullopt;
	}
	return reaction->source.initiator;
}

bool villager_reactions::LookAt(entt::entity villager, const glm::vec3& point, LookSpeed speed)
{
	return TurnTowards(villager, point, speed == LookSpeed::Quick ? 2.0f * k_WatchTurn : k_WatchTurn);
}

uint32_t villager_reactions::Watching(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto reaction = ReactionOf(villager);
	if (!reaction.has_value() || !Locator::infoConstants::has_value())
	{
		return GiveUp(action);
	}
	const auto& info = Locator::infoConstants::value().reaction.at(static_cast<size_t>(reaction->source.type));
	const auto here = registry.Get<const Transform>(villager).position;
	if (glm::distance(here, reaction->source.position) > info.maxDistanceToRunAwayFromObject)
	{
		return GiveUp(action);
	}
	TurnTowards(villager, reaction->source.position);
	return 0;
}

uint32_t villager_reactions::DanceWhileReacting(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	if (ecs::dances::DanceOf(registry, villager) == entt::null)
	{
		// Out of the dance, it decides what to do at once
		SetTop(action, VillagerStates::DecideWhatToDo);
		return Locator::livingActionSystem::value().VillagerCallState(action, LivingAction::Index::Top);
	}
	return ecs::villager_dance::PerformDance(action, VillagerStates::DanceWhileReacting);
}

namespace
{
/// How far a villager is from a burning thing, and how fast the thing moves, in metres a second: a thing that doesn't walk
/// about keeps still as far as this goes
std::pair<float, float> BurningThingFrom(entt::entity villager, entt::entity thing)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(thing) || !registry.AllOf<Transform>(thing) || !registry.AllOf<Transform>(villager))
	{
		return {std::numeric_limits<float>::max(), 0.0f};
	}
	const float distance = gutils::GetDistanceInMetres(registry.Get<const Transform>(villager).position,
	                                                   registry.Get<const Transform>(thing).position);
	const auto* walker = registry.TryGet<const WallHug>(thing);
	return {distance, walker != nullptr ? walker->speed : 0.0f};
}

float LeastRunDistance()
{
	return Locator::infoConstants::value()
	    .reaction.at(static_cast<size_t>(Reaction::ReactToBurningObjectInHand))
	    .minDistanceToRunAwayFromObject;
}
} // namespace

uint8_t villager_reactions::BurningInHandPriority(entt::entity villager, entt::entity thing, uint32_t tablePriority)
{
	const auto [distance, speed] = BurningThingFrom(villager, thing);
	return magic::villager_reaction::BurningInHandPriority(distance, LeastRunDistance(), speed, tablePriority);
}

uint32_t villager_reactions::BurningInHandAgainTurns(entt::entity villager, entt::entity thing, uint32_t tableTurns)
{
	const auto [distance, speed] = BurningThingFrom(villager, thing);
	return magic::villager_reaction::BurningInHandAgainTurns(distance, LeastRunDistance(), speed, tableTurns);
}

uint32_t villager_reactions::ReactToVillagerInHandPriority(entt::entity villager, entt::entity held)
{
	auto& registry = EntityRegistry();
	if (!Locator::infoConstants::has_value() || !registry.Valid(held))
	{
		return 0;
	}
	const auto* watcher = registry.TryGet<const Villager>(villager);
	const auto* heldVillager = registry.TryGet<const Villager>(held);
	const auto* heldAction = registry.TryGet<const LivingAction>(held);
	if (watcher == nullptr)
	{
		return 0;
	}
	const auto& kind = InfoOf(*watcher);
	const auto age = villager_age::AgeNow(*watcher);
	const auto& info = Locator::infoConstants::value();
	const auto watch = magic::villager_reaction::VillagerInHandWatch {
	    .heldIsVillager = heldVillager != nullptr,
	    .heldInHand = heldAction != nullptr && FinalStateOf(*heldAction) == VillagerStates::InHand,
	    .watcherSexuallyActive = kind.startHavingSexAge <= age && age < kind.stopHavingSexAge,
	    .otherSex = heldVillager != nullptr && InfoOf(*heldVillager).sex != kind.sex,
	    .samePlayer = world_objects::PlayerOf(held) == world_objects::PlayerOf(villager),
	    .watcherScripted = registry.AllOf<ScriptControlled>(villager),
	};
	return magic::villager_reaction::VillagerInHandPriority(
	    watch, info.reaction.at(static_cast<size_t>(Reaction::ReactToBreeder)).priority);
}

uint32_t villager_reactions::WaitForMate(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto reaction = ReactionOf(villager);
	if (!reaction.has_value() || !Locator::infoConstants::has_value())
	{
		return GiveUp(action);
	}
	const auto& info = Locator::infoConstants::value().reaction.at(static_cast<size_t>(reaction->source.type));
	const auto here = registry.Get<const Transform>(villager).position;
	const auto* held =
	    registry.Valid(reaction->source.initiator) ? registry.TryGet<const Transform>(reaction->source.initiator) : nullptr;
	const auto there = held != nullptr ? held->position : reaction->source.position;
	if (!magic::villager_reaction::KeepsWaitingForMate(gutils::GetDistanceInMetres(here, there), info.maxReactionDistance))
	{
		GiveUp(action);
		return 1;
	}
	// It faces the one held, and stands afresh each time its clip has played through once it does
	if (TurnTowards(villager, there) && villager_animate::IsReadyForNewAnimation(action))
	{
		villager_animate::SetStateAnim(villager);
	}
	return 1;
}

float villager_reactions::TownShare(entt::entity town)
{
	auto& registry = EntityRegistry();
	uint32_t people = 0;
	registry.Each<const Villager>([&](entt::entity, const Villager& villager) {
		if (villager.town == town)
		{
			++people;
		}
	});
	const float unmodified = Locator::infoConstants::value().town.populationForUnmodifiedBelief;
	return (unmodified + k_BeliefShareSmall) / (static_cast<float>(people) + k_BeliefShareSmall);
}

ecs::components::TownImpression& villager_reactions::ImpressionOf(entt::entity town)
{
	auto& registry = EntityRegistry();
	if (auto* impression = registry.TryGet<TownImpression>(town))
	{
		return *impression;
	}
	auto& impression = registry.Assign<TownImpression>(town);
	impression.belief.neutral = Locator::infoConstants::value().town.beliefInNeutralPlayer;
	return impression;
}

void villager_reactions::SetStateSpeed(entt::entity villager, VillagerStates state)
{
	auto& registry = EntityRegistry();
	const auto* component = registry.TryGet<const Villager>(villager);
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (component == nullptr || wallHug == nullptr || !Locator::infoConstants::has_value() ||
	    !villager_speed::StateChangeSetsSpeed(registry.AllOf<ScriptControlled>(villager), registry.AllOf<Dancer>(villager)))
	{
		return;
	}
	const auto& info = InfoOf(*component);
	const auto& townInfo = Locator::infoConstants::value().town;
	const auto& globals = registry.Context().mapScriptGlobals;
	villager_speed::Inputs inputs {
	    .speeds = {static_cast<int32_t>(info.speedGroup.speedDefault), static_cast<int32_t>(info.speedGroup.speedFleeing),
	               static_cast<int32_t>(info.speedGroup.speed2), static_cast<int32_t>(info.speedGroup.speed3),
	               static_cast<int32_t>(info.speedGroup.speed4), static_cast<int32_t>(info.speedGroup.speed5)},
	    .speedIndex = StateInfo(state).speedIndex,
	    .life = magic_living::LifeOf(villager).value_or(1.0f),
	    .lifeWhenWalksWounded = info.lifeWhenWalksWounded,
	    .lifeWhenCrawlsWounded = info.lifeWhenCrawlsWounded,
	    .landSpeedBalance = globals.landBalance.at(k_SpeedBalance),
	    .landBeliefSpeedScale = globals.landBalance.at(k_BeliefSpeedBalance),
	    .beliefSpeedScaleMultiPlayer = townInfo.beliefSpeedScaleMultiPlayer,
	    .beliefSpeedScaleStory = townInfo.beliefSpeedScaleStory,
	    .landNumber = static_cast<uint32_t>(globals.landNumber),
	    .baseForTownNeedsSpeedMod = info.baseForTownNeedsSpeedMod,
	    .divisorForTownNeedsSpeedMod = info.divisorForTownNeedsSpeedMod,
	    .maxWood = static_cast<float>(info.maxWoodCarried),
	    .maxFood = static_cast<float>(info.maxFoodCarried),
	    .speedModWhenFullLoadOfWood = info.speedModWhenFullLoadOfWood,
	    .speedModWhenFullLoadOfFood = info.speedModWhenFullLoadOfFood,
	    .foodSpeedUp = component->foodSpeedUp != 0,
	    .foodPowerupIncrease = info.foodPowerupIncrease,
	};
	const auto* town = registry.Valid(component->town) ? registry.TryGet<const Town>(component->town) : nullptr;
	if (town != nullptr)
	{
		if (const auto* wants = registry.TryGet<const TownDesire>(component->town))
		{
			inputs.townNeeds = town_desire::TownNeedsSum(*wants);
		}
		// A player's villager goes by the player's Indian tribal power, 1 with no wonder raising it
		if (town->owner != PlayerNames::NEUTRAL)
		{
			inputs.indianPower = 1.0f;
			const auto& belief = ImpressionOf(component->town).belief.belief;
			inputs.belief = villager_speed::Inputs::Belief {.player = belief.at(static_cast<size_t>(town->owner)),
			                                                .neutral = belief.at(static_cast<size_t>(PlayerNames::NEUTRAL))};
		}
	}
	const auto speed = villager_speed::StateSpeed(inputs, [](float max) {
		return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(max) : 0.0f;
	});
	// Then its own way of going
	SetSpeed(villager, speed);
}

void villager_reactions::SetSpeed(entt::entity villager, int32_t speed)
{
	auto& registry = EntityRegistry();
	const auto* component = registry.TryGet<const Villager>(villager);
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (component == nullptr || wallHug == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = InfoOf(*component);
	// Made its own way: when it was made, its age, and as an adult its belly, its life and its sex
	const auto factor = villager_speed::PersonalFactor({
	    .creationIndex = component->creationIndex,
	    .age = villager_age::AgeNow(*component),
	    .grownUpAge = info.grownUpAge,
	    .oldAge = info.oldAge,
	    .desireForFood = villager_needs::DesireForFood(component->food),
	    .life = magic_living::LifeOf(villager).value_or(1.0f),
	    .female = info.sex == SexType::Female,
	});
	wallHug->speed = GetSpeedStateSpeed(static_cast<SpeedState>(villager_speed::FinalSpeed(speed, factor)));
}

void villager_reactions::ShowTownBelief(const glm::vec3& centre, PlayerNames player, float belief)
{
	const auto amount = static_cast<int32_t>(belief * k_BeliefSpriteScale);
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	const auto colour = influence::k_PlayerColours.at(static_cast<size_t>(player) & (influence::k_PlayerColours.size() - 1));
	Locator::particleSystem::value().AddBeliefSprite({.position = centre, .amount = amount, .colour = colour});
}

void villager_reactions::ShowBelief(entt::entity villager, PlayerNames player, float belief, GuidanceAlignment alignment,
                                    BeliefVoice& voice, uint32_t turn)
{
	ShowObjectBelief(villager, EntityRegistry().Get<const Villager>(villager).town, player, belief, alignment, voice, turn);
}

void villager_reactions::ShowObjectBelief(entt::entity object, entt::entity town, PlayerNames player, float belief,
                                          GuidanceAlignment alignment, BeliefVoice& voice, uint32_t turn)
{
	auto& registry = EntityRegistry();
	const auto& transform = registry.Get<const Transform>(object);
	// A symbol worth at least one ten thousandth rises from the top of the thing, in the player's colour
	const auto amount = static_cast<int32_t>(belief * k_BeliefSpriteScale);
	if (amount >= 1 && Locator::particleSystem::has_value())
	{
		float height = 0.0f;
		if (const auto* mesh = registry.TryGet<const Mesh>(object);
		    mesh != nullptr && Locator::resources::value().GetMeshes().Contains(mesh->id))
		{
			height = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox().Size().y * transform.scale.y;
		}
		const auto colour =
		    influence::k_PlayerColours.at(static_cast<size_t>(player) & (influence::k_PlayerColours.size() - 1));
		Locator::particleSystem::value().AddBeliefSprite(
		    {.position = transform.position + glm::vec3(0.0f, height, 0.0f), .amount = amount, .colour = colour});
	}
	BeliefVoiceOf(object, town, player, alignment, voice, turn);
}
