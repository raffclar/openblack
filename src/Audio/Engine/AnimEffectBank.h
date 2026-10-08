/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Audio/Engine/AnimEffects.h"

namespace openblack::audio
{

/// The miracles' name of the anim effect tables of a .sad bank: the audio core's AnimEffectTable (AnimEffects.h),
/// unchanged (the same members, Load, FindList, SoundId and FindSample). The core reads the tables of
/// every bank once as it is registered (anim_effects::Tables(bank)); SpellSounds plays through
/// audio::PlayAnimationEffect on those tables (this name stays for the miracles' tests, test_spell_sounds).
using AnimEffectBank = AnimEffectTable;

} // namespace openblack::audio
