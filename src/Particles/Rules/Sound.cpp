/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The appearance rules that start and stop an atom's sounds (run per atom). The sounds themselves:
// Audio/Services/SpellSounds.h.

#include <cstdlib>

#include <spdlog/spdlog.h>

#include "Audio/Services/SpellSounds.h"
#include "Debug/DebugEnv.h"
#include "Particles/PSys.h"
#include "Particles/PSysRegistry.h"
#include "Particles/SoundAction.h"

using namespace openblack::psys;
namespace spell_sounds = openblack::audio::spell_sounds;

namespace
{
/// StartStopSoundOnCondition: SoundCondition true (or none) -> the sound once (unless the
/// atom already has one of that action); false -> that sound stopped, fading by FadeStep
class StartStopSoundOnCondition final: public Modifier
{
public:
	explicit StartStopSoundOnCondition(const Object& object)
	    : sound(ReadSoundAction(object, "Sound"))
	    , soundCondition(object.String("SoundCondition"))
	    , fadeStep(object.Int("FadeStep", 0))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		if (!soundCondition.empty() && !effect.ConditionForAtom(soundCondition, atom))
		{
			if (auto* playing = spell_sounds::GetSoundOfAction(atom, sound.action); playing != nullptr)
			{
				if (fadeStep != 0)
				{
					playing->action.fadeStep = fadeStep;
				}
				spell_sounds::StopSound(atom, *playing);
			}
			return true;
		}
		if (spell_sounds::GetSoundOfAction(atom, sound.action) == nullptr)
		{
			spell_sounds::StartSound(effect, atom, sound);
		}
		return true;
	}
	SoundAction sound;
	std::string soundCondition;
	int fadeStep;
};

/// AddSoundToAtom: once per atom, when its age reaches Delay and SoundCondition holds (or there is none);
/// StopOtherSoundsFirst stops the atom's sounds first. DoCameraShake (a camera shake of CameraShakeRadius at the atom,
/// strength 1.0, for CameraShakeDuration x 1000 ms) is not ported: openblack has no camera shake.
class AddSoundToAtom final: public Modifier
{
public:
	explicit AddSoundToAtom(const Object& object)
	    : sound(ReadSoundAction(object, "Sound"))
	    , soundCondition(object.String("SoundCondition"))
	    , stopOthers(object.Bool("StopOtherSoundsFirst", false))
	    , delay(object.Float("Delay", 0.0f))
	    , cameraShake(object.Bool("DoCameraShake", false))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& done = atom.data[this].x; // the rule's per-atom data
		if (done != 0.0f || effect.AtomAge(atom) < delay)
		{
			return true;
		}
		if (!soundCondition.empty() && !effect.ConditionForAtom(soundCondition, atom))
		{
			return true;
		}
		if (stopOthers)
		{
			spell_sounds::StopAllSounds(atom);
		}
		spell_sounds::StartSound(effect, atom, sound);
		done = 1.0f;
		if (cameraShake && openblack::debug_env::PsysSoundTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "PSys sound: {} asks for a camera shake (not ported)",
			                   effect.GetFile().name);
		}
		return true;
	}
	SoundAction sound;
	std::string soundCondition;
	bool stopOthers;
	float delay;
	bool cameraShake;
};

/// RemoveSoundFromAtom: once per atom, when SoundCondition holds (or there is none): the
/// atom's sound of that action stopped, fading by FadeStep
class RemoveSoundFromAtom final: public Modifier
{
public:
	explicit RemoveSoundFromAtom(const Object& object)
	    : sound(ReadSoundAction(object, "Sound"))
	    , soundCondition(object.String("SoundCondition"))
	    , fadeStep(object.Int("FadeStep", 0))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		auto& done = atom.data[this].x;
		if (done != 0.0f || (!soundCondition.empty() && !effect.ConditionForAtom(soundCondition, atom)))
		{
			return true;
		}
		if (auto* playing = spell_sounds::GetSoundOfAction(atom, sound.action); playing != nullptr)
		{
			if (fadeStep != 0)
			{
				playing->action.fadeStep = fadeStep;
			}
			spell_sounds::StopSound(atom, *playing);
		}
		done = 1.0f;
		return true;
	}
	SoundAction sound;
	std::string soundCondition;
	int fadeStep;
};
} // namespace

void openblack::psys::RegisterSoundRules()
{
	RegisterModifier("StartStopSoundOnCondition", MakeModifierOf<StartStopSoundOnCondition>);
	RegisterModifier("AddSoundToAtom", MakeModifierOf<AddSoundToAtom>);
	RegisterModifier("RemoveSoundFromAtom", MakeModifierOf<RemoveSoundFromAtom>);
}
