/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireSound.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <optional>

#include <glm/geometric.hpp>

#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSoundSystemInterface.h"
#include "FireEffect.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::fire;

namespace
{
/// LH_SAMPLE_G_FIRE_01: InGame sample 2 (the .sad gives it loops -1, play mode 2 and its flags)
constexpr int k_FireSample = 2;

using sound::Slot;

/// The game's fire crackle; stops with a message when there is none (before the game or after it has gone)
ecs::systems::FireSoundSystemInterface& Crackle()
{
	if (!Locator::fireSoundSystem::has_value())
	{
		std::fputs("ecs::fire::sound: no fire crackle in the locator (Locator::fireSoundSystem)\n", stderr);
		std::abort();
	}
	return Locator::fireSoundSystem::value();
}

/// GetDistanceInMetres(object, camera) (x, z)
float CameraDistance(const FireEffect& fire)
{
	if (!Locator::camera::has_value())
	{
		return 0.0f;
	}
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(fire.object);
	if (transform == nullptr)
	{
		return 0.0f;
	}
	const auto camera = Locator::camera::value().GetOrigin();
	return glm::length(glm::vec2(transform->position.x - camera.x, transform->position.z - camera.z));
}

/// The object's MapCoords as a point, 1; 0 without an object
std::optional<glm::vec3> FireSoundPoint(const FireEffect& fire)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (fire.object == entt::null || !registry.Valid(fire.object))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(fire.object);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	return transform->position;
}

/// The channels' owner of the fire (the play options' owner is the fire): an audio::Owner::Object number, registered
/// for the fire's 3D sound position until the fire goes
audio::Owner OwnerOf(const FireEffect& fire)
{
	auto& owners = Crackle().GetOwners();
	auto found = owners.find(&fire);
	if (found == owners.end())
	{
		found = owners.emplace(&fire, audio::NewObjectId()).first;
		audio::RegisterObject(found->second, [&fire]() { return FireSoundPoint(fire); });
	}
	return audio::Owner::Object(found->second);
}

void ForgetOwner(const FireEffect& fire)
{
	auto& owners = Crackle().GetOwners();
	if (const auto found = owners.find(&fire); found != owners.end())
	{
		audio::UnregisterObject(found->second);
		owners.erase(found);
	}
}

void RecomputeMax()
{
	float max = 0.0f;
	for (const auto& slot : Crackle().GetSlots())
	{
		if (!(slot.distance < max))
		{
			max = slot.distance;
		}
	}
	Crackle().SetMaxDistance(max);
}

/// A fire whose sound plays (the SoundPlaying flag) stops it (InGame, owner the fire, sample 2); then the flag goes
void Stop(FireEffect& fire)
{
	if ((fire.flags & FireEffect::k_SoundPlaying) != 0)
	{
		audio::StopSoundEffect(k_FireSample, OwnerOf(fire), audio::SfxBank::InGame);
	}
	fire.flags &= static_cast<uint8_t>(~FireEffect::k_SoundPlaying);
}

void Stop(Slot& slot)
{
	if (slot.fire != nullptr)
	{
		Stop(*slot.fire);
	}
}
} // namespace

void sound::RefreshDistances()
{
	for (auto& slot : Crackle().GetSlots())
	{
		if (slot.fire != nullptr)
		{
			slot.distance = CameraDistance(*slot.fire);
		}
	}
	RecomputeMax();
}

void sound::Consider(FireEffect& fire, bool loud)
{
	// Nearer than the farthest slot (or no slot yet), the fire takes the first slot that is empty or not nearer than
	// that one, then the farthest distance is recomputed. As in the original a fire already in a slot can take the
	// other one too (no check there). (approximate: the original also stops the slot's sound when it is this same
	// fire; the port keeps it playing)
	if (loud)
	{
		const float distance = CameraDistance(fire);
		const float max = Crackle().MaxDistance();
		if (!(distance < max) && max != 0.0f)
		{
			return;
		}
		for (auto& slot : Crackle().GetSlots())
		{
			if (slot.fire != nullptr && slot.distance < max)
			{
				continue;
			}
			if (slot.fire != nullptr && slot.fire != &fire)
			{
				Stop(slot);
			}
			slot.fire = &fire;
			slot.distance = distance;
			break;
		}
		RecomputeMax();
		return;
	}
	if ((fire.flags & FireEffect::k_SoundPlaying) == 0)
	{
		return;
	}
	for (auto& slot : Crackle().GetSlots())
	{
		if (slot.fire == &fire)
		{
			Stop(slot);
			slot = Slot {};
			RecomputeMax();
			return;
		}
	}
}

void sound::StartSlots()
{
	// each slot's fire plays, every turn
	for (auto& slot : Crackle().GetSlots())
	{
		if (slot.fire == nullptr)
		{
			continue;
		}
		// FireSoundPoint must answer 1
		const auto position = FireSoundPoint(*slot.fire);
		if (!position)
		{
			continue;
		}
		// The play options (constructor defaults): bank InGame, owner the fire, sample 2, is3D, track, the point; then the
		// play. The .sad's play mode 2 makes the later turns' calls do nothing while the loop plays.
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), k_FireSample};
		options.owner = OwnerOf(*slot.fire);
		options.is3D = true;
		options.track = true;
		options.position = *position;
		// the SoundPlaying flag set before the play
		slot.fire->flags |= FireEffect::k_SoundPlaying;
		audio::PlaySoundEffect(options);
	}
}

void sound::Free(FireEffect& fire)
{
	// ToBeDeleted: the first slot of the fire: its sound stops, the slot emptied, the farthest distance again; no slot,
	// nothing. (openblack) a second slot holding the same fire is emptied too: the original leaves it pointing at the
	// deleted fire.
	auto& slots = Crackle().GetSlots();
	auto found = std::ranges::find_if(slots, [&fire](const Slot& slot) { return slot.fire == &fire; });
	if (found != slots.end())
	{
		Stop(fire);
		for (auto& slot : slots)
		{
			if (slot.fire == &fire)
			{
				slot = Slot {};
			}
		}
		RecomputeMax();
	}
	ForgetOwner(fire);
}

void sound::Clear()
{
	for (auto& slot : Crackle().GetSlots())
	{
		Stop(slot);
		slot = Slot {};
	}
	auto& owners = Crackle().GetOwners();
	for (const auto& [fire, id] : owners)
	{
		audio::UnregisterObject(id);
	}
	owners.clear();
	Crackle().SetMaxDistance(0.0f);
}
