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

#include "AudioManagerInterface.h"
#include "ScriptSoundEffect.h"

namespace openblack::audio
{

/// The game's sound effects, the way the game plays most of them: each is heard or kept quiet by what its bank says of
/// it and by what the game is doing (a script's cut scene, the camera in the temple, the scripts' sound switch, the
/// player's creature fight controls). The creatures' and buildings' animation sounds, the advisors' speech, the music
/// and the land's atmosphere are played other ways and are not kept quiet by these rules.

/// What the game is doing now that decides whether its sound effects are heard
[[nodiscard]] SoundEffectConditions CurrentSoundEffectConditions();

/// Whether a loaded sound played as one of the game's sound effects is heard in these conditions. A sound that isn't
/// loaded isn't heard.
[[nodiscard]] bool GameSoundEffectHeard(const SoundEffectConditions& conditions, entt::id_type sound);

/// Plays one of the game's sound effects once, if it is heard now (see AudioManagerInterface::PlaySoundEffect)
void PlayGameSoundEffect(entt::id_type sound, std::optional<glm::vec3> worldPosition);

/// Starts one of the game's sound effects, if it is heard now: its emitter, null when nothing plays (see
/// AudioManagerInterface::StartSoundEffect)
entt::entity StartGameSoundEffect(entt::id_type sound, const SoundEffectOptions& options);

} // namespace openblack::audio
