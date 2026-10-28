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

#include <cmath>

#include <algorithm>
#include <string>
#include <vector>

#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureFootstepPuffs.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePointing.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/SnowDust.h"
#include "ECS/SoundGround.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::audio::AnimEffectPlay;
using openblack::creature::CreatureRig;

namespace
{
/// Whether a creature is the local player's own creature, which is always heard in its own voice
bool IsLocalPlayersCreature(entt::entity entity)
{
	if (!Locator::playerSystem::has_value())
	{
		return false;
	}
	const auto& players = Locator::playerSystem::value();
	return players.GetPrimaryCreature((Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
	                                                                    : PlayerNames::PLAYER_ONE)) == entity;
}

const CreatureRig* RigOf(const Creature& creature)
{
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

/// Plays an event's sound from the creature and notes it among the creature's last sounds
void Sound(entt::entity entity, const Creature& creature, const Transform& transform, const CreatureRig& rig,
           CreatureAudio& heard, const creature_audio::SoundEvent& event, const std::string& silence)
{
	const auto bank = event.kind == creature_audio::EventKind::Generic
	                      ? std::string(creature_audio::k_GenericBank)
	                      : creature_audio::VoiceBank(rig.soundBankName, creature.species);
	const auto keys = creature_audio::Keys(ShownSize(creature), creature.alignment, rig.soundObject,
	                                       creature_audio::SurfaceKey(ecs::sound_ground::At(transform.position)), event.action);
	CreatureAudio::Heard note {
	    .atMs = heard.clockMs,
	    .kind = event.kind,
	    .keys = keys,
	    .bank = bank,
	    .sample = std::nullopt,
	    .played = false,
	    .note = silence,
	};
	if (silence.empty())
	{
		if (event.mode != 0)
		{
			// Stopping a sound or letting its loop run out, which no shipped animation does
			note.note = "stop and release are not supported";
		}
		else if (Locator::audio::has_value())
		{
			// The sample has its say in when it is heard, as for every sound effect
			auto& audio = Locator::audio::value();
			const auto result =
			    audio.PlayAnimEffect(bank, keys.ToArray(), entity, transform.position, audio::CurrentSoundEffectConditions());
			// The sound stays with the creature while it plays
			audio.SetEmitterFollows(result.emitter, audio::Follows::Owner);
			note.sample = result.sample;
			note.played = result.outcome == AnimEffectPlay::Outcome::Played;
			switch (result.outcome)
			{
			case AnimEffectPlay::Outcome::Played:
				break;
			case AnimEffectPlay::Outcome::NoEffect:
				note.note = "no effect for the keys";
				break;
			case AnimEffectPlay::Outcome::NotLoaded:
				note.note = "sample not loaded";
				break;
			case AnimEffectPlay::Outcome::NotHeard:
				note.note = "its sample may not be heard now";
				break;
			case AnimEffectPlay::Outcome::TooFar:
				note.note = fmt::format("too far ({:.0f})",
				                        glm::distance(Locator::audio::value().GetListener().origin, transform.position));
				break;
			case AnimEffectPlay::Outcome::AlreadyPlaying:
				note.note = "already playing";
				break;
			case AnimEffectPlay::Outcome::NoVoice:
				note.note = "no free voice";
				break;
			}
		}
	}
	[[maybe_unused]] const auto array = keys.ToArray();
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Creature {} {} ({}) from {} keys [{} {} {} {} {}]: {}{}",
	                    static_cast<uint32_t>(entity), creature_audio::Name(event.action), static_cast<int32_t>(event.action),
	                    bank, array[0], array[1], array[2], array[3], array[4],
	                    note.sample.has_value() ? fmt::format("sample {}", *note.sample) : std::string("no sample"),
	                    note.played ? std::string(" played") : fmt::format(", silent: {}", note.note));
	heard.recent.push_back(std::move(note));
	while (heard.recent.size() > CreatureAudio::k_RecentCount)
	{
		heard.recent.pop_front();
	}
}

/// Plays an event's sound if the creature's rules ask for it. A sound not asked for is noted as silent, unless the
/// creatures are muted
void SoundIfAsked(entt::entity entity, const Creature& creature, const Transform& transform, const CreatureRig& rig,
                  CreatureAudio& heard, const creature_audio::SoundEvent& event, const creature_audio::Gate& gate)
{
	if (creature_audio::IsHeard(event.kind, gate))
	{
		Sound(entity, creature, transform, rig, heard, event, {});
	}
	else if (!gate.muted && event.kind == creature_audio::EventKind::Voice)
	{
		Sound(entity, creature, transform, rig, heard, event, "another player's creature's voice");
	}
}

/// The layers of the body as they are posed this frame
creature_audio::Layers LayersOf(const CreatureAnimation& animation)
{
	creature_audio::Layers layers;
	// The animations played by weight stand in for the body's own action
	const auto& slots = DrawnSlots(animation);
	if (slots.empty() && creature_layers::IsPlaying(animation.body))
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
	std::vector<size_t> animations;
	for (const auto& slot : slots)
	{
		layers.slots.push_back({slot.animation, slot.timeMs});
		weights.push_back(slot.weight);
		animations.push_back(slot.animation);
	}
	// The game moves the four animations of a point on as the first of them, so only its events sound, whatever the
	// weights
	const auto pointing =
	    creature_audio::BlendSoundingSlot(animations, creature_pointing::k_Animations, creature_pointing::k_Animations.front());
	layers.soundingSlot = pointing.has_value() ? pointing : creature_audio::SoundingSlot(weights);
	return layers;
}

/// The dust that a footstep throws up at the creature's place on the ground. Where snow lies, it shows the colour of
/// the land's light
void FootstepPuffs(const Creature& creature, const Transform& transform, int32_t count)
{
	if (count <= 0 || !Locator::liquidParticleSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	auto& random = Locator::gameRandom::value();
	const float size = ShownSize(creature);
	const auto colour = ecs::snow_dust::Tint(creature_footstep_puffs::k_Colour, ecs::snow_dust::SnowAt(transform.position));
	for (int32_t i = 0; i < count; ++i)
	{
		const auto velocity = creature_footstep_puffs::VelocityOf(size, [&random](int32_t n) { return random.LocalRand(n); });
		Locator::liquidParticleSystem::value().Add(transform.position, velocity, creature_footstep_puffs::SizeOf(size), colour,
		                                           liquid_particles::Kind::Dust);
	}
}
} // namespace

void CreatureAudioSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _muted);
	reflection::Save(out, _otherVoices);
}

bool CreatureAudioSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _muted);
	reflection::Load(in, _otherVoices);
	return !in.Failed();
}

creature_audio::Gate CreatureAudioSystem::GateOf(entt::entity creature) const
{
	return {
	    .muted = _muted,
	    .localPlayersCreature = IsLocalPlayersCreature(creature),
	    .otherVoicesEnabled = _otherVoices,
	};
}

void CreatureAudioSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Every creature remembers where its animations were
	std::vector<entt::entity> unheard;
	registry.Each<const Creature, const CreatureAnimation>(
	    [&unheard](entt::entity entity, const Creature&, const CreatureAnimation&) { unheard.push_back(entity); },
	    entt::exclude<CreatureAudio>);
	for (const auto entity : unheard)
	{
		registry.Assign<CreatureAudio>(entity);
	}

	registry.Each<const Creature, const CreatureAnimation, const Transform, CreatureAudio>(
	    [&](entt::entity entity, const Creature& creature, const CreatureAnimation& animation, const Transform& turnTransform,
	        CreatureAudio& heard) {
		    // The sounds come from the body where the frame draws it
		    const auto transform = DrawnTransform(turnTransform, &animation);
		    heard.clockMs += gameTime.count();
		    const auto* rig = RigOf(creature);
		    const auto layers = LayersOf(animation);
		    if (rig == nullptr)
		    {
			    return;
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

		    const auto gate = GateOf(entity);
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
			    SoundIfAsked(entity, creature, transform, *rig, heard, fired.event, gate);
			    // After the sound, a footstep throws up dust, heard or not
			    FootstepPuffs(creature, transform, creature_footstep_puffs::CountOf(fired.event.action));
			    // Then an event can set a creed glow on the body, heard or not
			    if (Locator::creatureAnimationSystem::has_value())
			    {
				    Locator::creatureAnimationSystem::value().PlayCreedEvent(entity, static_cast<int32_t>(fired.event.action));
			    }
		    }
	    });

	// The creatures' sounds follow them about. The sounds' entities are the audio's, apart from the world's.
	auto* emitters = Locator::audio::has_value() ? Locator::audio::value().Emitters() : nullptr;
	if (emitters == nullptr)
	{
		return;
	}
	emitters->Each<AudioEmitter>([&registry](AudioEmitter& emitter) {
		if (emitter.owner == entt::null || !registry.Valid(emitter.owner))
		{
			return;
		}
		if (const auto* transform = registry.TryGet<const Transform>(emitter.owner);
		    transform != nullptr && registry.AnyOf<Creature>(emitter.owner))
		{
			emitter.position = DrawnTransform(*transform, registry.TryGet<const CreatureAnimation>(emitter.owner)).position;
		}
	});
}

void CreatureAudioSystem::Play(entt::entity creature, const creature_audio::SoundEvent& event)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return;
	}
	const auto& component = registry.Get<const Creature>(creature);
	const auto* rig = RigOf(component);
	if (rig == nullptr || event.kind == creature_audio::EventKind::HairGroup)
	{
		return;
	}
	auto* heard = registry.TryGet<CreatureAudio>(creature);
	if (heard == nullptr)
	{
		heard = &registry.Assign<CreatureAudio>(creature);
	}
	Sound(creature, component, registry.Get<const Transform>(creature), *rig, *heard, event, {});
}

void CreatureAudioSystem::PlayAction(entt::entity creature, const creature_audio::SoundEvent& event)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return;
	}
	const auto& component = registry.Get<const Creature>(creature);
	const auto* rig = RigOf(component);
	if (rig == nullptr)
	{
		return;
	}
	auto* heard = registry.TryGet<CreatureAudio>(creature);
	if (heard == nullptr)
	{
		heard = &registry.Assign<CreatureAudio>(creature);
	}
	SoundIfAsked(creature, component, registry.Get<const Transform>(creature), *rig, *heard, event, GateOf(creature));
}
