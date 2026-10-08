/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
struct MobileWalkPath;
}

namespace openblack::ecs
{

/// The CHL WALK_PATH of a MobileObject (docs/bw1-notes/camera-tracks.md): the object follows the focus way of the
/// camera track "Track<path>" of Data\camera.edt, timed by its position way, 100 ms of the track per game turn.

/// (path, from, to, forward), from the script's WALK_PATH for a thing that is not Living: into the walk path list once,
/// and a new DataPath: the track, to, forward, step 100, current = from * duration.
/// False when the track cannot be read (the original then crashes on the null ScriptedCamera).
bool StartMobileWalkPath(entt::entity entity, int32_t path, bool forward, float from, float to);

/// The point of a DataPath this turn, the same for a mobile object and a Living moving along a path: the sample
/// current forward, duration - current backwards, truncated toward zero, clamped to 0..duration; the position way is run to it
/// (its point dropped), then the focus way's Bezier with the segment and t it left. The sample in `sampleOut` (the
/// trace's)
[[nodiscard]] glm::vec3 SampleWalkPath(components::MobileWalkPath& walk, int32_t* sampleOut = nullptr);

/// Every game turn after the sharks' process: each object of the list moves along its path.
/// OPENBLACK_WALK_PATH_TRACE=1 logs every turn's sample, segment, t and point.
void ProcessMobileWalkPaths();

} // namespace openblack::ecs
