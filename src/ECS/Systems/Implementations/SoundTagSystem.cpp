/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SoundTagSystem.h"

#include <cstdint>

#include <optional>
#include <vector>

#include <glm/geometric.hpp>

#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/HearingRange.h"
#include "Camera/Camera.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The milliseconds of a game turn
constexpr auto k_MillisecondsPerTurn = static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count());

void StopSound(SoundTag& tag)
{
	if (tag.emitter == entt::null)
	{
		return;
	}
	if (Locator::audio::has_value())
	{
		auto& audio = Locator::audio::value();
		if (audio.EmitterExists(tag.emitter))
		{
			audio.StopEmitter(tag.emitter);
			audio.DestroyEmitter(tag.emitter);
		}
	}
	tag.emitter = entt::null;
}

bool IsPlaying(const SoundTag& tag)
{
	auto& audio = Locator::audio::value();
	return tag.emitter != entt::null && audio.EmitterExists(tag.emitter) &&
	       audio.GetStatus(tag.emitter) != audio::AudioStatus::Stopped;
}

/// Removes a tag from its object. A looping sound that plays finishes its loop, and a sound that plays one time
/// finishes. The audio removes the emitter when the sound stops.
void LetGo(SoundTag& tag)
{
	if (tag.emitter != entt::null && Locator::audio::has_value() && IsPlaying(tag) && tag.loops)
	{
		Locator::audio::value().ReleaseEmitterLoop(tag.emitter);
	}
	tag.emitter = entt::null;
}

/// Whether a point is within a sound's reach of the camera, a reach of none meaning anywhere
bool WithinReach(float reach, const glm::vec3& position, const glm::vec3& camera)
{
	const auto offset = position - camera;
	return reach <= 0.0f || glm::dot(offset, offset) <= reach * reach;
}

} // namespace

SoundTagSystem::SoundTagSystem()
    : SoundTagSystem(&audio::CurrentSoundEffectConditions)
{
}

SoundTagSystem::SoundTagSystem(ConditionsSource conditions)
    : _conditions(conditions)
{
}

bool SoundTagSystem::Heard(entt::id_type sound) const
{
	return audio::GameSoundEffectHeard(_conditions(), sound);
}

void SoundTagSystem::StartPointSound(entt::entity entity, SoundTag& tag, const glm::vec3& position,
                                     std::optional<glm::vec3> camera) const
{
	const auto& sounds = Locator::resources::value().GetSounds();
	if (!tag.active || !sounds.Contains(tag.sound) ||
	    (camera.has_value() && !WithinReach(sounds.Handle(tag.sound)->maxDistance, position, *camera)) || !Heard(tag.sound))
	{
		return;
	}
	auto& audio = Locator::audio::value();
	tag.emitter = audio.CreateEmitter(tag.sound, position, audio::PlayType::Once, entity);
	if (tag.emitter != entt::null)
	{
		audio.PlayEmitter(tag.emitter);
	}
}

void SoundTagSystem::ProcessTurn(const glm::vec3& camera)
{
	if (!Locator::audio::has_value())
	{
		return;
	}
	auto& audio = Locator::audio::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& sounds = Locator::resources::value().GetSounds();
	std::vector<entt::entity> finished;
	std::vector<entt::entity> orphans;
	registry.Each<SoundTag>([&](entt::entity entity, SoundTag& tag) {
		++tag.turns;
		// An attached tag sounds from its object. When the object is gone or has no position, the tag goes.
		const auto source = tag.owner != entt::null ? tag.owner : entity;
		const auto* transform = registry.Valid(source) ? registry.TryGet<const Transform>(source) : nullptr;
		if (tag.owner != entt::null && transform == nullptr)
		{
			orphans.push_back(entity);
			return;
		}
		if (transform == nullptr)
		{
			return;
		}
		if (!sounds.Contains(tag.sound))
		{
			if (tag.point)
			{
				finished.push_back(entity);
			}
			return;
		}
		const auto position = transform->position + tag.offset;
		const auto reach = sounds.Handle(tag.sound)->maxDistance;
		if (tag.point)
		{
			if (tag.delayed)
			{
				// Not heard until the sound has travelled as far as the camera, and then only nearer than its reach
				const auto distanceSquared = audio::DistanceSquared(camera, position);
				if (!audio::SoundArrived(audio::SoundTravelled(tag.turns, k_MillisecondsPerTurn), distanceSquared))
				{
					return;
				}
				if (audio::ArrivedSoundStarts(distanceSquared, reach))
				{
					StartPointSound(entity, tag, position, std::nullopt);
				}
				tag.delayed = false;
				return;
			}
			if (!IsPlaying(tag))
			{
				finished.push_back(entity);
			}
			return;
		}
		if (!tag.active || IsPlaying(tag) || !WithinReach(reach, position, camera) || !Heard(tag.sound))
		{
			return;
		}
		StopSound(tag);
		// Each tag is the owner of its own sound, so the sounds of two tags never take the voice of the other
		tag.emitter =
		    audio.CreateEmitter(tag.sound, position, tag.loops ? audio::PlayType::Repeat : audio::PlayType::Once, entity);
		if (tag.emitter != entt::null)
		{
			audio.PlayEmitter(tag.emitter);
		}
	});
	for (const auto entity : finished)
	{
		StopSound(registry.Get<SoundTag>(entity));
		registry.Destroy(entity);
	}
	for (const auto entity : orphans)
	{
		LetGo(registry.Get<SoundTag>(entity));
		registry.Destroy(entity);
	}
}

void SoundTagSystem::SetActive(entt::entity entity, bool active)
{
	auto* tag = Locator::entitiesRegistry::value().TryGet<SoundTag>(entity);
	if (tag == nullptr)
	{
		return;
	}
	tag->active = active;
	if (!active)
	{
		StopSound(*tag);
	}
}

entt::entity SoundTagSystem::CreatePointSound(entt::id_type sound, const glm::vec3& position, bool delayed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& tag = registry.Assign<SoundTag>(entity, SoundTag {
	                                                  .sound = sound,
	                                                  .offset = glm::vec3(0.0f),
	                                                  .active = true,
	                                                  .emitter = entt::null,
	                                                  .point = true,
	                                                  .delayed = delayed,
	                                                  .turns = 0,
	                                              });
	// One not delayed sounds at once, where it is made
	if (!delayed && Locator::audio::has_value() && Locator::resources::has_value())
	{
		StartPointSound(entity, tag, position, Locator::audio::value().GetListener().origin);
	}
	return entity;
}

entt::entity SoundTagSystem::AttachObjectSound(entt::entity entity, entt::id_type sound, bool loops)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto tag = registry.Create();
	registry.Assign<SoundTag>(tag, SoundTag {
	                                   .sound = sound,
	                                   .offset = glm::vec3(0.0f),
	                                   .active = true,
	                                   .emitter = entt::null,
	                                   .point = false,
	                                   .delayed = false,
	                                   .turns = 0,
	                                   .owner = entity,
	                                   .loops = loops,
	                               });
	return tag;
}

void SoundTagSystem::DetachObjectSound(entt::entity entity, entt::id_type sound)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> attached;
	registry.Each<SoundTag>([&attached, entity, sound](entt::entity tagEntity, const SoundTag& tag) {
		if (tag.owner == entity && tag.sound == sound)
		{
			attached.push_back(tagEntity);
		}
	});
	for (const auto tagEntity : attached)
	{
		LetGo(registry.Get<SoundTag>(tagEntity));
		registry.Destroy(tagEntity);
	}
	// The object's own tag of the same sound goes too
	if (auto* own = registry.TryGet<SoundTag>(entity);
	    own != nullptr && !own->point && own->owner == entt::null && own->sound == sound)
	{
		LetGo(*own);
		registry.Remove<SoundTag>(entity);
	}
}
