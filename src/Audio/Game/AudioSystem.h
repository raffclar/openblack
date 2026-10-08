/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/BankTables.h"
#include "Audio/Game/Banks.h"

// The game's audio system: layer 2 of the audio engine (docs/bw1-notes/audio.md). The public face is Audio.h; this
// header is for src/Audio (the services under it and the old sample_play names).

namespace openblack::audio
{
struct GameQueries;

// The banks (BankId, RegisterBank, Bank, FindBank, SampleId...): Banks.h

/// The owner is a thing that is not available. No owner, the atmos mixer and any other kind of owner are available.
[[nodiscard]] bool OwnerUnavailable(const Owner& owner);

/// The queries audio::Init got (the services of layer 3 read the game through them)
[[nodiscard]] const GameQueries& Queries();

/// The sound position of a Thing (GameQueries::thingPosition) or Object owner (RegisterObject): nullopt for the
/// other kinds or a gone one
[[nodiscard]] std::optional<glm::vec3> OwnerSoundPosition(const Owner& owner);

/// The guard on the sound points handed to the sample player: each coordinate whose absolute value is more than 5000
/// becomes 0 (equal, below and NaN are kept). Done on the channel's point (after copying it for the default) and on
/// the point returned; the distance returned is measured from the unguarded point
[[nodiscard]] glm::vec3 GuardSoundPoint(glm::vec3 point);

/// The game's 3D position function for the sample player, as the anim effects ask it: no owner = the camera; the
/// atmos owner gives none; a thing gives none when not available, else its sound position; any other owner its own
/// (a sound tag's: tags::TagSoundPoint). nullopt = nothing plays. The point is guarded (GuardSoundPoint).
[[nodiscard]] std::optional<glm::vec3> OwnerSoundPoint(const Owner& owner);
/// The listener's point (the camera's), nullopt without a camera
[[nodiscard]] std::optional<glm::vec3> ListenerPoint();
/// GameQueries::landAltitude, 0 when unset
[[nodiscard]] float IslandAltitude(float x, float z);
/// GameQueries::surfaceType (ecs::sea_cells::GetSurfaceType), 6 (off the map) when unset. The game calls
/// ecs::sea_cells::GetSurfaceType itself; this is for src/Audio.
[[nodiscard]] int32_t SurfaceType(glm::vec3 point);

/// PlaySoundEffect on a sound id: nothing without a game (openblack always has one once the audio is initialised); a
/// 3D sample (with a sample number) not started when the camera's squared distance to pos + offset is more than the
/// squared max distance (the .sad's, or the options' when that is 0; inside the citadel from the citadel's camera);
/// then the filters by the sample's user parameter (its low 16 bits): 1 not while a script holds the wide screen, only
/// 2 inside the citadel, only the banks Villagers / HelpSprites after SET_GAME_SOUND false, not 4 in the interface
/// states 0x10 / 0x16 / 0x17; and a 3D tracked sample of an unavailable owner. Then the sample plays.
/// (The original returns nothing; openblack returns the channel.)
Channel PlaySoundEffectOptions(const sample_play::Options& options);

/// SET_GAME_SOUND: false -> every sound effect stopped and the game sound off, true -> on
void SetGameSound(bool enabled);
/// HelpSystem's wide screen as a script sets it (with its owning task)
void SetScriptWideScreen(bool on);
/// The script's wide screen, as SetScriptWideScreen left it
[[nodiscard]] bool IsScriptWideScreen();
/// GameQueries::insideCitadel
[[nodiscard]] bool IsInsideCitadel();
/// GameQueries::videoPlaying
[[nodiscard]] bool IsVideoPlaying();

} // namespace openblack::audio
