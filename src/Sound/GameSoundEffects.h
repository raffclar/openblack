/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Sound/ScriptSoundEffect.h"
#include "Sound/SoundRequestInterface.h"

namespace openblack::audio
{

/// The game's sound effects, as the game plays most of them. The bank of each sound and the state of the game decide if
/// the player hears it. The state is a cut scene of a script, the camera in the temple, the sound switch of the scripts
/// and the fight controls of the player's creature. These rules do not apply to the animation sounds, the speech of the
/// advisors, the music and the atmosphere of the land.
///
/// The world asks for these sounds through the sound requests (Locator::sounds). With no sound requests, no sound plays.

/// The state of the game that decides if the player hears its sound effects
[[nodiscard]] SoundEffectConditions CurrentSoundEffectConditions();

/// Whether the player hears a loaded sound in these conditions, when the game plays it as a sound effect. The player
/// does not hear a sound that is not loaded.
[[nodiscard]] bool GameSoundEffectHeard(const SoundEffectConditions& conditions, entt::id_type sound);

/// Plays one of the game's sound effects one time, if the player hears it now (see SoundRequestInterface::PlayGameEffect)
void PlayGameSoundEffect(entt::id_type sound, std::optional<glm::vec3> worldPosition);

/// Starts one of the game's sound effects, if the player hears it now. It gives the emitter, or null when no sound plays
/// (see SoundRequestInterface::StartGameEffect).
entt::entity StartGameSoundEffect(entt::id_type sound, const SoundEffectOptions& options);

} // namespace openblack::audio
