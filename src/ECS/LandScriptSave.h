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

#include <functional>
#include <optional>
#include <string>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

/// The class's land-script writer (docs/bw1-notes/land-script-save.md): the line(s) that re-create an object, through
/// lhscriptx::WriteCommand. The vortex writes them relative to its entry (at != nullptr: the position is written as the
/// map coordinates minus at); a land save would pass nullptr. Each class's owner registers its writer; the physics
/// classes' are here.
namespace openblack::ecs::land_script_save
{
/// A global save count (a uint16): one pass of saving; CheckAndSetSaved marks an object saved in this pass and says
/// whether it was not yet
void BeginSavePass();
[[nodiscard]] bool CheckAndSetSaved(entt::entity object);

/// A class's SaveObject: the text written (possibly empty: nothing to write), or nullopt when the object was already
/// saved in this pass. The original's 0 / 1 result is not kept: its only caller, the vortex's write, ignores it
using SaveObjectFn = std::function<std::optional<std::string>(entt::entity object, const map_coords::MapCoords* at)>;
/// Which writer an object uses: the first registered whose test matches
void Register(std::function<bool(entt::entity)> matches, SaveObjectFn fn);
/// The object's SaveObject; nullopt for no writer or already saved
[[nodiscard]] std::optional<std::string> SaveObject(entt::entity object, const map_coords::MapCoords* at);

/// The physics classes' writers (registered by RegisterPhysicsWriters):
/// a mobile static (Rock and Fragment use it; a Fragment is written with the Rock info every fragment gets):
/// CREATE_MOBILE_STATIC (42, "ANFFFFF")
[[nodiscard]] std::optional<std::string> SaveMobileStatic(entt::entity object, const map_coords::MapCoords* at);
/// A mobile object (Ball uses it): CREATE_MOBILEOBJECT (40, "ANNN")
[[nodiscard]] std::optional<std::string> SaveMobileObject(entt::entity object, const map_coords::MapCoords* at);
/// A pot: CREATE_POT (38, "ANNN")
[[nodiscard]] std::optional<std::string> SavePot(entt::entity object, const map_coords::MapCoords* at);
/// A dead tree: CREATE_DEAD_TREE (43, "ALNFFFF")
[[nodiscard]] std::optional<std::string> SaveDeadTree(entt::entity object, const map_coords::MapCoords* at);
/// A one-shot spell seed: CREATE_ONE_SHOT_SPELL_PU (84, "AL"; not 83)
[[nodiscard]] std::optional<std::string> SaveOneOffSpellSeed(entt::entity object, const map_coords::MapCoords* at);
void RegisterPhysicsWriters();
} // namespace openblack::ecs::land_script_save
