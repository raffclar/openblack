/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::designed_scenery
{

/// The designed waterfall, every frame from the landscape's update: scenery fixed by land number
/// (SET_LAND_NUMBER), not game objects (bare 3D objects, no shadow, no map cell). When the land
/// number changes, what the last land had is deleted (objects, the ScriptMarker and its SoundTag).
/// - Land 3: the waterfall Data\MISC\waterfall3.l3d at (3059.23, 0, 3145.33) (y fixed at 0), angle 4.7, scale 1; its
///   texture V scrolls by -0.5 per second of game time (SetUVOffset, kept in -1..0); a water ring every 0.7 s at the
///   foot (3018.8, 0.2, 3130.15); the looping G_WaterFlow (InGame 12) SoundTag at (3059.23, 0, 3145.33).
/// - Land 4: the stranded ark Data\MISC\arche.l3d at (3538, altitude, 2129), angle 8.9728, scale 1.1, with the same
///   SoundTag at (3538, 0, 2129); the dinosaur skeleton Data\MISC\dinosaur.l3d at (2690, altitude, 2590), angle 1.57,
///   scale 1, with its landscape footprint.
/// `gameMilliseconds` = the game time increment (0 while paused).
void Update(float gameMilliseconds);

/// A new map: the registry reset has destroyed the objects and sound_tags::Clear the tag, so the handles are dropped
/// (the same land builds them again on the next frame). The land number, the V scroll and the ring timer are statics
/// of the original and stay.
void OnLoadMap();

} // namespace openblack::ecs::designed_scenery
