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
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SoundPoint.h"
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

void StopSound(entt::entity& emitter)
{
	if (emitter == entt::null)
	{
		return;
	}
	if (Locator::audio::has_value())
	{
		auto& audio = Locator::audio::value();
		if (audio.EmitterExists(emitter))
		{
			audio.StopEmitter(emitter);
			audio.DestroyEmitter(emitter);
		}
	}
	emitter = entt::null;
}

void StopSound(SoundTag& tag)
{
	StopSound(tag.emitter);
}

bool IsPlaying(entt::entity emitter)
{
	auto& audio = Locator::audio::value();
	return emitter != entt::null && audio.EmitterExists(emitter) && audio.GetStatus(emitter) != audio::AudioStatus::Stopped;
}

bool IsPlaying(const SoundTag& tag)
{
	return IsPlaying(tag.emitter);
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

/// The point that an object's sounds come from (see ecs::SoundPointOf). An object in a hand sounds from the hand.
glm::vec3 SoundPoint(const ecs::Registry& registry, entt::entity object, const Transform& transform)
{
	return ecs::SoundPointOf(registry, object).value_or(transform.position);
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

void SoundTagSystem::StartPointSound(PointSound& point, std::optional<glm::vec3> camera) const
{
	const auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(point.sound) ||
	    (camera.has_value() && !WithinReach(sounds.Handle(point.sound)->maxDistance, point.position, *camera)) ||
	    !Heard(point.sound))
	{
		return;
	}
	auto& audio = Locator::audio::value();
	point.emitter = audio.CreateOwnSoundEmitter(point.sound, point.position, audio::PlayType::Once);
	if (point.emitter != entt::null)
	{
		// The sound stays at its point, and the audio turn stops it when the camera goes too far
		audio.SetEmitterFollows(point.emitter, audio::Follows::Place);
		audio.PlayEmitter(point.emitter);
	}
}

void SoundTagSystem::ProcessPointSounds(const glm::vec3& camera)
{
	const auto& sounds = Locator::resources::value().GetSounds();
	std::erase_if(_points, [this, &sounds, &camera](PointSound& point) {
		++point.turns;
		if (!sounds.Contains(point.sound))
		{
			StopSound(point.emitter);
			return true;
		}
		if (point.delayed)
		{
			// Not heard until the sound has travelled as far as the camera, and then only nearer than its reach
			const auto distanceSquared = audio::DistanceSquared(camera, point.position);
			if (!audio::SoundArrived(audio::SoundTravelled(point.turns, k_MillisecondsPerTurn), distanceSquared))
			{
				return false;
			}
			if (audio::ArrivedSoundStarts(distanceSquared, sounds.Handle(point.sound)->maxDistance))
			{
				StartPointSound(point, std::nullopt);
			}
			point.delayed = false;
			return false;
		}
		if (!IsPlaying(point.emitter))
		{
			StopSound(point.emitter);
			return true;
		}
		return false;
	});
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
			return;
		}
		const auto position = SoundPoint(registry, source, *transform) + tag.offset;
		const auto reach = sounds.Handle(tag.sound)->maxDistance;
		// The sound of an object follows the object: each turn its emitter moves to where the object is now. The audio
		// turn after this stops the sound if the new place is out of its range.
		if (tag.active && IsPlaying(tag))
		{
			audio.SetEmitterPosition(tag.emitter, position);
			return;
		}
		if (!tag.active || !WithinReach(reach, position, camera) || !Heard(tag.sound))
		{
			return;
		}
		StopSound(tag);
		// Each tag is the owner of its own sound, so the sounds of two tags never take the voice of the other
		tag.emitter =
		    audio.CreateEmitter(tag.sound, position, tag.loops ? audio::PlayType::Repeat : audio::PlayType::Once, entity);
		if (tag.emitter != entt::null)
		{
			// This system moves the sound with its object each turn, and the audio turn stops it when the camera goes
			// too far
			audio.SetEmitterFollows(tag.emitter, audio::Follows::Place);
			audio.PlayEmitter(tag.emitter);
		}
	});
	for (const auto entity : orphans)
	{
		LetGo(registry.Get<SoundTag>(entity));
		registry.Destroy(entity);
	}
	ProcessPointSounds(camera);
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

void SoundTagSystem::CreatePointSound(entt::id_type sound, const glm::vec3& position, bool delayed)
{
	auto& point = _points.emplace_back(PointSound {.sound = sound, .position = position, .delayed = delayed});
	// One not delayed sounds at once, where it is made
	if (!delayed && Locator::audio::has_value() && Locator::resources::has_value())
	{
		StartPointSound(point, Locator::audio::value().GetListener().origin);
	}
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

void SoundTagSystem::ForgetSavedSounds()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> gone;
	registry.Each<SoundTag>([&gone](entt::entity entity, SoundTag& tag) {
		if (tag.point || tag.owner != entt::null)
		{
			gone.push_back(entity);
			return;
		}
		tag.emitter = entt::null;
		tag.turns = 0;
	});
	for (const auto entity : gone)
	{
		registry.Destroy(entity);
	}
	_points.clear();
}
