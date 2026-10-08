/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSounds.h"

#include <cstdlib>

#include <algorithm>
#include <memory>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Audio/Game/AudioSystem.h"
#include "Camera/Camera.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"
#include "Particles/PSys.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// A live looping sound is only kept going within this distance of the camera
constexpr float k_CullDistance = 1200.0f;
/// The speed of sound for the thunder delay (units per second)
constexpr float k_SoundSpeed = 347.0f;

/// What this module keeps between calls (Locator::audioState)
struct SpellSoundsState
{
	/// Every live particle sound
	std::vector<std::shared_ptr<ParticleSound>> sounds {};
};

SpellSoundsState& SpellSoundsData()
{
	return openblack::Locator::audioState::value().Get<SpellSoundsState>();
}

bool Trace()
{
	static const bool trace = debug_env::PsysSoundTrace();
	return trace;
}

/// The spells bank (Audio\Sfx\Game\spells.sad, AUDIO_SFX_BANK_TYPE 3)
BankId SpellsBank()
{
	return Bank(SfxBank::Spells);
}

float CameraDistance(glm::vec3 position)
{
	return Locator::camera::has_value() ? glm::distance(Locator::camera::value().GetOrigin(), position) : 1e9f;
}

float LandAltitude(glm::vec3 position)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                                           : position.y;
}

/// The attribute array handed to the bank: {size, alignment, 1, surface, action}
AnimKey Attributes(const psys::SoundAction& action)
{
	return {action.size, action.alignment, 1, action.surface, action.action};
}

Owner OwnerOf(const ParticleSound& sound)
{
	return Owner::Object(sound.owner);
}

/// The 3D sound position: with an atom, its position with the land's height under it when SnapToGround; it answers
/// even without an atom, leaving the point as it was, so the channel keeps the last one
/// (openblack) the original reads the atom's drawn position without asking whether the atom was drawn: that field is
/// only written when it is (Atom::Draw), so a never drawn atom would give it a stale point; `drawn` stands for that.
glm::vec3 ParticleSoundPosition(ParticleSound& sound)
{
	if (sound.atom != nullptr && sound.atom->drawn)
	{
		sound.position = sound.atom->current.position;
		if ((sound.action.flags & psys::SoundAction::k_SnapToGround) != 0)
		{
			sound.position.y = LandAltitude(sound.position);
		}
	}
	return sound.position;
}

/// PlayAnimationEffect with action Play: the owner this ParticleSound, the camera distance the caller measured, the key,
/// the spells bank, track 1, min / max 0. The filters, the row's sample at random, the 800 / max distance gates and the
/// sample's play mode are audio::PlayAnimationEffect's.
void Play(const ParticleSound& sound, float distance)
{
	const auto channel = PlayAnimationEffect(OwnerOf(sound), distance, Attributes(sound.action), AnimAction::Play, SpellsBank(),
	                                         true, 0.0f, 0.0f);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: start {} ({}) size {} surface {} at {:.1f} -> {}",
		                   psys::SoundActionName(sound.action.action), sound.action.action, sound.action.size,
		                   sound.action.surface, distance,
		                   channel != k_NoChannel ? fmt::format("channel {}", channel) : std::string("nothing"));
	}
}

/// The release: PlayAnimationEffect(this, 0, key, 1 + (SoftRelease ? 1 : 0), bank, 1, 0, 0), so 1 = stop and 2 =
/// release the loop of every sample of the row's list playing for this sound
void Release(const ParticleSound& sound, bool soft)
{
	PlayAnimationEffect(OwnerOf(sound), 0.0f, Attributes(sound.action), soft ? AnimAction::Release : AnimAction::Stop,
	                    SpellsBank(), true, 0.0f, 0.0f);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} {}, atom gone", soft ? "release loop" : "stop",
		                   psys::SoundActionName(sound.action.action));
	}
}

/// The sound's end: out of the global list, and its owner number forgotten
void Forget(const ParticleSound& sound)
{
	UnregisterObject(sound.owner);
}
} // namespace

openblack::psys::Atom::~Atom()
{
	for (const auto& sound : sounds)
	{
		sound->atom = nullptr;
	}
}

void spell_sounds::StartSound(const psys::Effect& effect, psys::Atom& atom, const psys::SoundAction& action)
{
	// nothing for NO_SOUND
	if (action.action == -1)
	{
		return;
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} StartSound {} ({}) flags {:#x}", effect.GetFile().name,
		                   psys::SoundActionName(action.action), action.action, action.flags);
	}
	auto sound = std::make_shared<ParticleSound>();
	sound->action = action;
	// the atom's global position, the land's height with SnapToGround
	glm::vec3 p = effect.GlobalPosition(atom);
	if ((action.flags & psys::SoundAction::k_SnapToGround) != 0)
	{
		p.y = LandAltitude(p);
	}
	// USESURFACE: the surface type (GameQueries::surfaceType: ecs::sea_cells, the one reading of the map's cells)
	if ((action.flags & psys::SoundAction::k_UseSurface) != 0)
	{
		sound->action.surface = SurfaceType(p);
	}
	// the owner's alignment (the discrete alignment 0..6 -> 1,1,2,2,2,3,3) needs the
	// player link: the action's slot stays (no spells.sad row reads it)
	sound->atom = &atom;
	sound->position = p;
	sound->owner = NewObjectId();
	// the channels' 3D function asks the ParticleSound for its point
	RegisterObject(sound->owner, [raw = sound.get()]() -> std::optional<glm::vec3> { return ParticleSoundPosition(*raw); });
	atom.sounds.insert(atom.sounds.begin(), sound);
	SpellSoundsData().sounds.push_back(sound);
	// the camera's distance to the point
	const float distance = CameraDistance(p);
	// the Delayed flag waits distance / 347 s
	if ((action.flags & psys::SoundAction::k_Delayed) != 0)
	{
		sound->delay = distance / k_SoundSpeed;
		return;
	}
	Play(*sound, distance);
}

void spell_sounds::StopSound(psys::Atom& atom, ParticleSound& sound)
{
	sound.atom = nullptr;
	std::erase_if(atom.sounds, [&sound](const std::shared_ptr<ParticleSound>& s) { return s.get() == &sound; });
}

void spell_sounds::StopAllSounds(psys::Atom& atom)
{
	for (const auto& sound : atom.sounds)
	{
		sound->atom = nullptr;
	}
	atom.sounds.clear();
}

ParticleSound* spell_sounds::GetSoundOfAction(const psys::Atom& atom, int32_t action)
{
	for (const auto& sound : atom.sounds)
	{
		if (sound->action.action == action)
		{
			return sound.get();
		}
	}
	return nullptr;
}

void spell_sounds::ProcessTurn(float turnSeconds)
{
	auto& state = SpellSoundsData();
	// each ParticleSound of the global list
	for (auto it = state.sounds.begin(); it != state.sounds.end();)
	{
		auto& sound = **it;
		ParticleSoundPosition(sound);
		if (sound.atom == nullptr)
		{
			// the first channel of the spells bank playing for this sound, asked of the mixer directly; none -> the
			// sound is deleted
			const auto channel = PlayingChannel(OwnerOf(sound), SpellsBank());
			if (channel == k_NoChannel)
			{
				if (Trace())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} deleted",
					                   psys::SoundActionName(sound.action.action));
				}
				Forget(sound);
				it = state.sounds.erase(it);
				continue;
			}
			// with a FadeStep, that first channel of the owner's volume becomes max(volume - FadeStep, 0)
			if (sound.action.fadeStep != 0)
			{
				SetVolume(channel, std::max(Volume(channel) - sound.action.fadeStep, 0));
			}
			// the release once
			if (!sound.released)
			{
				Release(sound, (sound.action.flags & psys::SoundAction::k_SoftRelease) != 0);
				sound.released = true;
			}
			++it;
			continue;
		}
		// Looping plays again; Delayed with a delay left counts it down and plays once it goes below 0
		bool play = (sound.action.flags & psys::SoundAction::k_Looping) != 0;
		if (!play && (sound.action.flags & psys::SoundAction::k_Delayed) != 0 && sound.delay > 0.0f)
		{
			sound.delay -= turnSeconds;
			play = sound.delay < 0.0f;
		}
		if (play)
		{
			// the 3D sound position, squared camera distance < 1200 * 1200, then the square root
			const float distance = CameraDistance(sound.position);
			if (distance < k_CullDistance)
			{
				Play(sound, distance);
			}
		}
		++it;
	}
}

void spell_sounds::Clear()
{
	auto& state = SpellSoundsData();
	// (openblack) a new map: the audio's reset stops every channel already; the sounds are forgotten
	for (const auto& sound : state.sounds)
	{
		sound->atom = nullptr;
		StopSoundEffect(0, OwnerOf(*sound), SpellsBank());
		Forget(*sound);
	}
	state.sounds.clear();
}

size_t spell_sounds::Count()
{
	return SpellSoundsData().sounds.size();
}

// SizeFromRadius / SizeFromImpactSpeed: strict "<" at both thresholds against the rule's floats
int32_t spell_sounds::SizeFromRadius(float radius, float small, float medium)
{
	if (radius < small)
	{
		return 3;
	}
	return radius < medium ? 2 : 1;
}

int32_t spell_sounds::SizeFromThrow(float fraction)
{
	// the float fraction against the doubles 0.59999999999999998 and 0.29999999999999999, strict ">": 0.6f and 0.3f
	// are above them
	const auto f = static_cast<double>(fraction);
	if (f > 0.6)
	{
		return 1;
	}
	return f > 0.3 ? 2 : 3;
}

int32_t spell_sounds::SizeFromImpactSpeed(float speed, float medium, float large)
{
	if (speed < medium)
	{
		return 3;
	}
	return speed < large ? 2 : 1;
}
