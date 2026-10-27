/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include <array>
#include <span>
#include <string_view>

#include "3D/CameraTrack.h"
#include "Creature/CaveCreature.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_ca(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_ca(entt::meta_ctx& context)
{
	Reflect<openblack::camera_track::Walk>(context, ValueOnly {})
	    .Field<&openblack::camera_track::Walk::runner>("runner")
	    .Field<&openblack::camera_track::Walk::to>("to")
	    .Field<&openblack::camera_track::Walk::forward>("forward")
	    .Field<&openblack::camera_track::Walk::current>("current")
	    .Field<&openblack::camera_track::Walk::step>("step");
	Reflect<openblack::cave_creature::State>(context, ValueOnly {})
	    .Field<&openblack::cave_creature::State::rest>("rest")
	    .Field<&openblack::cave_creature::State::heading>("heading")
	    .Field<&openblack::cave_creature::State::wantedHeading>("wantedHeading")
	    .Field<&openblack::cave_creature::State::turn>("turn")
	    .Field<&openblack::cave_creature::State::zoomed>("zoomed")
	    .Field<&openblack::cave_creature::State::heldMs>("heldMs")
	    .Field<&openblack::cave_creature::State::idleMs>("idleMs")
	    .Field<&openblack::cave_creature::State::fidgets>("fidgets");
	Reflect<openblack::cave_creature::State::Turn>(context, ValueOnly {})
	    .Field<&openblack::cave_creature::State::Turn::from>("from")
	    .Field<&openblack::cave_creature::State::Turn::to>("to")
	    .Field<&openblack::cave_creature::State::Turn::timeMs>("timeMs")
	    .Field<&openblack::cave_creature::State::Turn::durationMs>("durationMs")
	    .Field<&openblack::cave_creature::State::Turn::spin>("spin")
	    .Field<&openblack::cave_creature::State::Turn::blendedWith>("blendedWith")
	    .Field<&openblack::cave_creature::State::Turn::blendWeight>("blendWeight");
}
