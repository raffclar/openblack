/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerAfterDeath.h"

#include <algorithm>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "Animals/AnimalAnimation.h"
#include "Animals/AnimalRules.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/HelpSpeech.h"
#include "CHLApi.h"
#include "Common/GameRandom.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/RisingSoul.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerClips.h"
#include "ECS/VillagerDeathRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_after_death = openblack::ecs::villager_after_death;

namespace
{
/// The size of the puff a body leaves, which is the dust puff's own white
constexpr float k_BodySmokeSize = 1.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t Turn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
}

const Town* TownOf(entt::entity town)
{
	auto& registry = Entities();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
}

/// The player whose villager it is: its town's
std::optional<PlayerNames> OwnerOf(entt::entity villager)
{
	const auto* person = Entities().TryGet<const Villager>(villager);
	const auto* town = person != nullptr ? TownOf(person->town) : nullptr;
	return town != nullptr ? std::optional(town->owner) : std::nullopt;
}

const GVillagerInfo* InfoOf(const Villager& person)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& infos = Locator::infoConstants::value().villager;
	const auto kind = static_cast<size_t>(GVillagerInfo::Find(person.tribe, person.number));
	return kind < infos.size() ? &infos.at(kind) : nullptr;
}

/// The player's entity, by name
std::optional<entt::entity> PlayerEntity(PlayerNames name)
{
	std::optional<entt::entity> found;
	Entities().Each<const Player>([&](entt::entity entity, const Player& player) {
		if (player.name == name)
		{
			found = entity;
		}
	});
	return found;
}

/// Whether the guidance of a kind may be heard by the player here now; if so, it is marked heard this turn
bool MayHearGuidance(PlayerNames here, GuidanceSound sound, const villager_death::GuidanceKind& kind)
{
	const auto player = PlayerEntity(here);
	if (!player.has_value() || !Locator::gameRandom::has_value())
	{
		return false;
	}
	auto& registry = Entities();
	auto* heard = registry.TryGet<GuidanceHeard>(*player);
	if (heard == nullptr)
	{
		heard = &registry.Assign<GuidanceHeard>(*player);
	}
	// The help level the player chose, none while the scripts keep the help system off
	const bool helpOn = !Locator::chlapi::has_value() || Locator::chlapi::value().IsHelpSystemOn();
	const auto helpLevel = helpOn ? chlapi::CHLApi::k_PlayerHelpLevel : 0u;
	const auto& context = registry.Context();
	const bool firstLand = context.mapScriptGlobals.landNumber == 1 && !context.skirmish;
	auto& random = Locator::gameRandom::value();
	const auto spread = villager_death::GuidanceGapSpread(kind.leastGap, random.LocalFloatRand(1.0f));
	const auto gap = kind.leastGap + random.LocalRand(static_cast<int32_t>(spread));
	const auto turn = Turn();
	auto& last = heard->lastTurn.at(static_cast<size_t>(sound));
	if (!villager_death::GuidanceMayPlay(kind, helpLevel, firstLand, gap, turn - last))
	{
		return false;
	}
	last = turn;
	return true;
}

/// The guidance's line of a death in the village, one of its five at random
void PlayDeathLine(PlayerNames here)
{
	if (!Locator::helpSpeechSystem::has_value() || !Locator::audio::has_value() || !Locator::resources::has_value() ||
	    !MayHearGuidance(here, GuidanceSound::DeathInVillage, villager_death::k_DeathLineKind))
	{
		return;
	}
	const auto sample = villager_death::k_DeathLineFirstSample +
	                    Locator::gameRandom::value().LocalRand(static_cast<int32_t>(villager_death::k_DeathLineCount));
	const auto sound = Locator::helpSpeechSystem::value().GetTable().FindSound(audio::SpeechBank::Guidance, sample);
	SPDLOG_LOGGER_DEBUG(spdlog::get("ai"), "Death in the village: guidance line {} ({})", sample,
	                    sound.has_value() ? "found" : "missing");
	if (sound.has_value() && Locator::resources::value().GetSounds().Contains(*sound))
	{
		Locator::audio::value().StartSoundEffect(
		    *sound, {.pitchPercent = villager_death::k_DeathLinePitch, .volume = villager_death::k_DeathLineVolume});
	}
}
} // namespace

void villager_after_death::Hear(entt::entity villager, DeathReason reason, std::optional<PlayerNames> killer)
{
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	const auto here = Locator::playerSystem::value().GetLocalPlayer();
	const auto owner = OwnerOf(villager).value_or(PlayerNames::NEUTRAL);
	switch (villager_death::WhoHears(reason, killer.value_or(PlayerNames::NEUTRAL) == here, owner == here))
	{
	case villager_death::DeathVoice::OwnerLine:
		PlayDeathLine(here);
		break;
	case villager_death::DeathVoice::KillerAdvisors:
		// TODO(advisor-spirits): the advisors speak of the death the player here caused, when the villager is on screen.
		// Their commentary of the game as it goes isn't made yet; it is kept quiet on the first land anyway.
		break;
	case villager_death::DeathVoice::Nobody:
		break;
	}
	// TODO(advisor-spirits): the advisors also warn of worshippers dying of chanting, of losing people to another
	// player, and of a town left with few people; none of them speaks on the first land
}

void villager_after_death::MoveOwnersAlignment(entt::entity villager, DeathReason reason)
{
	const auto owner = OwnerOf(villager);
	const auto* person = Entities().TryGet<const Villager>(villager);
	if (!owner.has_value() || person == nullptr || !Locator::alignmentSystem::has_value() ||
	    !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& changes = Locator::infoConstants::value().player.dealthReason;
	const auto index = static_cast<size_t>(reason);
	if (index >= changes.size())
	{
		return;
	}
	const bool child = person->lifeStage == Villager::LifeStage::Child;
	Locator::alignmentSystem::value().AddPendingAlignment(*owner, villager_death::AlignmentChange(changes.at(index), child));
}

void villager_after_death::CountInTown(entt::entity town, std::optional<PlayerNames> killer, DeathReason reason)
{
	auto& registry = Entities();
	if (TownOf(town) == nullptr)
	{
		return;
	}
	auto* deaths = registry.TryGet<TownDeaths>(town);
	if (deaths == nullptr)
	{
		deaths = &registry.Assign<TownDeaths>(town);
	}
	villager_death::CountDeath(deaths->counts, Turn(), static_cast<size_t>(killer.value_or(PlayerNames::NEUTRAL)), reason);
}

uint32_t villager_after_death::DeathsOf(entt::entity town, DeathReason reason)
{
	auto& registry = Entities();
	const auto* deaths = town != entt::null && registry.Valid(town) ? registry.TryGet<const TownDeaths>(town) : nullptr;
	const auto index = static_cast<size_t>(reason);
	return deaths != nullptr && index < deaths->counts.byReason.size() ? deaths->counts.byReason.at(index) : 0u;
}

void villager_after_death::LeaveWorship(entt::entity villager, entt::entity town)
{
	auto& registry = Entities();
	const auto* data = TownOf(town);
	if (data == nullptr || data->worshipSite == entt::null || !registry.Valid(data->worshipSite))
	{
		return;
	}
	if (auto* site = registry.TryGet<WorshipSite>(data->worshipSite))
	{
		std::erase(site->worshippers, villager);
	}
}

void villager_after_death::TellThoseAbout(entt::entity villager)
{
	auto& registry = Entities();
	if (!Locator::reactionSystem::has_value())
	{
		return;
	}
	// The death is nobody's: it impresses no one
	Locator::reactionSystem::value().Create({.initiator = villager,
	                                         .type = Reaction::ReactToDeath,
	                                         .player = PlayerNames::NEUTRAL,
	                                         .position = registry.Get<const Transform>(villager).position,
	                                         .playerless = true});
}

void villager_after_death::SmokePuff(entt::entity villager)
{
	auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(villager);
	if (!Locator::explosionSystem::has_value() || !Locator::resources::has_value() || transform == nullptr)
	{
		return;
	}
	// Half its height out along the way it faces: its model's height, times its size
	const auto* mesh = registry.TryGet<const Mesh>(villager);
	const auto& meshes = Locator::resources::value().GetMeshes();
	const float height = mesh != nullptr && meshes.Contains(mesh->id)
	                         ? meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform->scale.x
	                         : 0.0f;
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	const float heading = wallHug != nullptr ? wallHug->yAngle : 0.0f;
	Locator::explosionSystem::value().AddSmoke(animals::DeadBodySmokePlace(transform->position, heading, height),
	                                           k_BodySmokeSize, dust_puff::Kind::Smoke);
}

void villager_after_death::SoulRises(entt::entity villager)
{
	auto& registry = Entities();
	const auto* person = registry.TryGet<const Villager>(villager);
	const auto* transform = registry.TryGet<const Transform>(villager);
	if (person == nullptr || transform == nullptr || ecs::villager_clips::IsOnWater(transform->position) ||
	    !Locator::gameRandom::has_value())
	{
		return;
	}
	const auto* info = InfoOf(*person);
	if (info == nullptr)
	{
		return;
	}
	// A child's soul is a child, and always goes to heaven; the others go up or down as chance has it, whatever they did
	const bool child = villager_age::AgeNow(*person) < info->grownUpAge;
	const float draw = Locator::gameRandom::value().CrtRandom(0.0f, 100.0f);
	const auto soul = registry.Create();
	registry.Assign<Transform>(soul, *transform);
	registry.Assign<RisingSoul>(soul, RisingSoul {
	                                      .mesh = resources::HashIdentifier(child ? info->childMeshHigh : info->highDetail),
	                                      .clip = villager_death::SoulClip(draw, child),
	                                      .timeMs = 0,
	                                      .alpha = villager_death::k_SoulAlpha,
	                                      .bones = {},
	                                  });
	SPDLOG_LOGGER_DEBUG(spdlog::get("ai"), "Villager #{}: its soul rises ({})", entt::to_integral(villager),
	                    child || draw < 50.0f ? "heaven" : "hell");
}

void villager_after_death::MoveSouls(uint32_t elapsedMs)
{
	auto& registry = Entities();
	if (!Locator::resources::has_value())
	{
		return;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto& meshes = Locator::resources::value().GetMeshes();
	std::vector<entt::entity> gone;
	registry.Each<RisingSoul>([&](entt::entity soul, RisingSoul& rising) {
		const auto clipId = resources::HashIdentifier(static_cast<uint32_t>(rising.clip));
		if (!animations.Contains(clipId) || !meshes.Contains(rising.mesh))
		{
			gone.push_back(soul);
			return;
		}
		const auto clip = animations.Handle(clipId);
		rising.timeMs += elapsedMs;
		if (villager_death::SoulGone(rising.timeMs, clip->GetPlayTime()))
		{
			gone.push_back(soul);
			return;
		}
		rising.alpha = villager_death::SoulAlphaAt(rising.timeMs, clip->GetPlayTime());
		const auto model = meshes.Handle(rising.mesh);
		const auto& frames = clip->GetFrames();
		const auto& parents = model->GetBoneParents();
		const auto span = animals::SpanAt(
		    {.playTime = clip->GetPlayTime(), .frameCount = frames.size(), .looping = clip->IsLooping()}, rising.timeMs);
		if (!model->IsBoned() || frames.empty() || frames[span.from].bones.size() != parents.size() ||
		    frames[span.to].bones.size() != parents.size())
		{
			rising.bones.clear();
			return;
		}
		rising.bones.resize(parents.size());
		animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, rising.bones);
	});
	for (const auto soul : gone)
	{
		registry.Destroy(soul);
	}
	if (!gone.empty())
	{
		registry.SetDirty();
	}
}
