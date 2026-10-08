/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureAudioSystem.h"

#include <span>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Audio/Audio.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/Game/Banks.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRig.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;

namespace
{
const CreatureRig* RigOf(const Creature& creature)
{
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

/// Plays an event's sound from the creature and notes it among the creature's last sounds
void Sound(entt::entity entity, const Creature& creature, const Transform& transform, const CreatureRig& rig,
           CreatureAudio& heard, const creature_audio::SoundEvent& event, const std::string& silence)
{
	const auto surface = static_cast<audio::SoundSurface>(ecs::sea_cells::GetSurfaceType(transform.position));
	const auto request = creature_audio::Request(event, creature.size, creature.alignment, rig.soundObject, surface,
	                                             rig.soundBankName, creature.species);
	const bool hasAudio = Locator::audio::has_value();
	audio::BankId bank = audio::k_NoBank;
	if (request.generic)
	{
		bank = audio::Bank(audio::SfxBank::Creature);
	}
	else if (hasAudio && !request.voiceStem.empty())
	{
		bank = Locator::audio::value().CreatureBank(request.voiceStem);
	}
	CreatureAudio::Heard note {
	    .atMs = heard.clockMs,
	    .kind = event.kind,
	    .keys = request.keys,
	    .bank = bank,
	    .channel = audio::k_NoChannel,
	    .played = false,
	    .note = silence,
	};
	if (silence.empty())
	{
		const auto listener = Locator::audioState::has_value() ? audio::ListenerPoint() : std::nullopt;
		if (!hasAudio)
		{
			note.note = "no audio";
		}
		else if (!request.generic && bank == audio::k_NoBank)
		{
			note.note = "no voice bank";
		}
		else if (!listener.has_value())
		{
			note.note = "no listener";
		}
		else
		{
			// Played from the creature, which the sound follows
			note.channel = Locator::audio::value().PlayAnimationEffect(
			    audio::Owner::Thing(entity), glm::distance(*listener, transform.position), request.keys.ToArray(),
			    request.action, bank, true, 0.0f, 0.0f);
			note.played = note.channel != audio::k_NoChannel;
			if (!note.played)
			{
				note.note = "filtered out or not started";
			}
		}
	}
	const auto keys = request.keys.ToArray();
	if (auto logger = spdlog::get("audio"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "Creature {} {} ({}) from bank {} keys [{} {} {} {} {}]: {}", static_cast<uint32_t>(entity),
		                    creature_audio::Name(event.action), static_cast<int32_t>(event.action), bank, keys[0], keys[1],
		                    keys[2], keys[3], keys[4],
		                    note.played ? fmt::format("channel {}", note.channel) : fmt::format("silent: {}", note.note));
	}
	heard.recent.push_back(std::move(note));
	while (heard.recent.size() > CreatureAudio::k_RecentCount)
	{
		heard.recent.pop_front();
	}
}

/// The layers of the body as they are posed this frame
creature_audio::Layers LayersOf(const CreatureAnimation& animation)
{
	creature_audio::Layers layers;
	// The animations played by weight stand in for the body's own action
	if (animation.slots.empty() && creature_layers::IsPlaying(animation.body))
	{
		layers.body = creature_audio::Played {creature_layers::CurrentAnimation(animation.body), animation.body.timeMs};
	}
	if (animation.gesture.animation.has_value())
	{
		layers.gesture = creature_audio::Played {*animation.gesture.animation, animation.gesture.timeMs};
	}
	// A face being run back to its start to change makes no sound
	if (animation.face.current.has_value() && animation.face.wanted == animation.face.current)
	{
		layers.face = creature_audio::Played {*animation.face.current, animation.face.timeMs};
	}
	std::vector<float> weights;
	for (const auto& slot : animation.slots)
	{
		layers.slots.push_back({slot.animation, slot.timeMs});
		weights.push_back(slot.weight);
	}
	layers.soundingSlot = creature_audio::SoundingSlot(weights);
	return layers;
}

/// The creature's sounds, made the first time it is heard
CreatureAudio& HeardOf(ecs::Registry& registry, entt::entity creature)
{
	return std::as_const(registry).AllOf<CreatureAudio>(creature) ? registry.Get<CreatureAudio>(creature)
	                                                              : registry.AssignState<CreatureAudio>(creature);
}
} // namespace

bool CreatureAudioSystem::AreOtherVoicesEnabled() const
{
	return !Locator::audioState::has_value() ||
	       creature_audio::OtherVoicesEnabled(audio::GetScriptAudioState().creatureSound.load());
}

void CreatureAudioSystem::SetOtherVoicesEnabled(bool enabled)
{
	if (Locator::audioState::has_value())
	{
		audio::GetScriptAudioState().creatureSound.store(enabled ? 1 : 0);
	}
}

void CreatureAudioSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);

	// Every creature remembers where its animations were
	std::vector<entt::entity> creatures;
	lookup.Each<const Creature, const CreatureAnimation, const Transform>(
	    [&creatures](entt::entity entity, const Creature&, const CreatureAnimation&, const Transform&) {
		    creatures.push_back(entity);
	    });

	const creature_audio::Gate gate {
	    .muted = _muted,
	    .localPlayersCreature = false,
	    .otherVoicesEnabled = AreOtherVoicesEnabled(),
	};
	const auto local = creature::LocalPlayer();

	for (const auto entity : creatures)
	{
		auto& heard = HeardOf(registry, entity);
		const auto& creature = lookup.Get<const Creature>(entity);
		const auto& animation = lookup.Get<const CreatureAnimation>(entity);
		const auto& transform = lookup.Get<const Transform>(entity);
		heard.clockMs += gameTime.count();
		const auto* rig = RigOf(creature);
		const auto layers = LayersOf(animation);
		if (rig == nullptr)
		{
			continue;
		}
		const creature_audio::InfoOf infoOf = [rig](size_t index) -> std::optional<creature_audio::AnimationInfo> {
			const auto* played = rig->GetAnimation(CreatureRig::Mesh::Base, index);
			if (played == nullptr)
			{
				return std::nullopt;
			}
			return creature_audio::AnimationInfo {
			    .events = index < rig->soundEvents.size() ? std::span(rig->soundEvents[index])
			                                              : std::span<const creature_audio::SoundEvent> {},
			    .durationMs = played->duration,
			    .looping = played->looping,
			};
		};

		// Every layer's events since the last frame, queued in order of when they fall within the frame
		const auto queue = creature_audio::FrameEvents(heard.last, layers, gameTime.count(), infoOf);

		auto creatureGate = gate;
		creatureGate.localPlayersCreature = creature.owner == local;
		for (const auto& fired : queue)
		{
			// Hair groups shown and hidden by animations are not drawn differently yet
			if (fired.event.kind == creature_audio::EventKind::HairGroup)
			{
				continue;
			}
			// A footstep leaves its print even when it isn't heard
			if (creature_audio::IsFootstep(fired.event.action) && Locator::footprintSystem::has_value())
			{
				Locator::footprintSystem::value().Step(entity);
			}
			if (creature_audio::IsHeard(fired.event.kind, creatureGate))
			{
				Sound(entity, creature, transform, *rig, heard, fired.event, {});
			}
			else if (!creatureGate.muted)
			{
				Sound(entity, creature, transform, *rig, heard, fired.event, "another player's creature's voice");
			}
		}
	}
}

void CreatureAudioSystem::Play(entt::entity creature, const creature_audio::SoundEvent& event)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature, Transform>(creature))
	{
		return;
	}
	const auto& component = lookup.Get<const Creature>(creature);
	const auto* rig = RigOf(component);
	if (rig == nullptr || event.kind == creature_audio::EventKind::HairGroup)
	{
		return;
	}
	auto& heard = HeardOf(registry, creature);
	Sound(creature, component, lookup.Get<const Transform>(creature), *rig, heard, event, {});
}
