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

#include "3D/SkeletalAnimation.h"
#include "3D/SkyDome.h"
#include "3D/StormClouds.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/StatisticsRules.h"
#include "ScriptHeaders/ScriptChallengeSnapshots.h"
#include "ScriptHeaders/ScriptTimers.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_s(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_s(entt::meta_ctx& context)
{
	Reflect<openblack::script::challenge_snapshots::Value>(context, ValueOnly {})
	    .Field<&openblack::script::challenge_snapshots::Value::value>("value")
	    .Field<&openblack::script::challenge_snapshots::Value::type>("type");
	Reflect<openblack::script::timers::Timer>(context, ValueOnly {})
	    .Field<&openblack::script::timers::Timer::setTurn>("setTurn")
	    .Field<&openblack::script::timers::Timer::turns>("turns");
	Reflect<openblack::skeletal_animation::Animation>(context, ValueOnly {})
	    .Field<&openblack::skeletal_animation::Animation::duration>("duration")
	    .Field<&openblack::skeletal_animation::Animation::looping>("looping")
	    .Field<&openblack::skeletal_animation::Animation::rotatedJoints>("rotatedJoints")
	    .Field<&openblack::skeletal_animation::Animation::translatedJoints>("translatedJoints")
	    .Field<&openblack::skeletal_animation::Animation::frames>("frames")
	    .Field<&openblack::skeletal_animation::Animation::displacement>("displacement")
	    .Field<&openblack::skeletal_animation::Animation::strideLength>("strideLength");
	Reflect<openblack::skeletal_animation::Animation::Frame>(context, ValueOnly {})
	    .Field<&openblack::skeletal_animation::Animation::Frame::eulerAngles>("eulerAngles")
	    .Field<&openblack::skeletal_animation::Animation::Frame::translations>("translations");
	Reflect<openblack::skeletal_animation::Skeleton>(context, ValueOnly {})
	    .Field<&openblack::skeletal_animation::Skeleton::parents>("parents")
	    .Field<&openblack::skeletal_animation::Skeleton::restRotations>("restRotations")
	    .Field<&openblack::skeletal_animation::Skeleton::inverseRestRotations>("inverseRestRotations");
	Reflect<openblack::sky_dome::FrameRows>(context, ValueOnly {})
	    .Field<&openblack::sky_dome::FrameRows::rows>("rows")
	    .Field<&openblack::sky_dome::FrameRows::count>("count");
	Reflect<openblack::sky_dome::Rows>(context, ValueOnly {})
	    .Field<&openblack::sky_dome::Rows::skyType>("skyType")
	    .Field<&openblack::sky_dome::Rows::first>("first")
	    .Field<&openblack::sky_dome::Rows::count>("count");
	Reflect<openblack::statistics::BuildingsBuilt>(context, ValueOnly {})
	    .Field<&openblack::statistics::BuildingsBuilt::total>("total")
	    .Field<&openblack::statistics::BuildingsBuilt::abodes>("abodes")
	    .Field<&openblack::statistics::BuildingsBuilt::civic>("civic")
	    .Field<&openblack::statistics::BuildingsBuilt::wonders>("wonders");
	Reflect<openblack::statistics::GlobalFigures>(context, ValueOnly {})
	    .Field<&openblack::statistics::GlobalFigures::linesOfCode>("linesOfCode")
	    .Field<&openblack::statistics::GlobalFigures::maxFrameRate>("maxFrameRate")
	    .Field<&openblack::statistics::GlobalFigures::minFrameRate>("minFrameRate")
	    .Field<&openblack::statistics::GlobalFigures::townsAtStart>("townsAtStart");
	Reflect<openblack::statistics::History>(context, ValueOnly {})
	    .Field<&openblack::statistics::History::points>("points")
	    .Field<&openblack::statistics::History::index>("index")
	    .Field<&openblack::statistics::History::samplesPerPoint>("samplesPerPoint")
	    .Field<&openblack::statistics::History::samplesInPoint>("samplesInPoint");
	Reflect<openblack::statistics::Satisfaction>(context, ValueOnly {})
	    .Field<&openblack::statistics::Satisfaction::total>("total")
	    .Field<&openblack::statistics::Satisfaction::samples>("samples");
	Reflect<openblack::storm_clouds::Drift>(context, ValueOnly {})
	    .Field<&openblack::storm_clouds::Drift::place>("place")
	    .Field<&openblack::storm_clouds::Drift::target>("target")
	    .Field<&openblack::storm_clouds::Drift::step>("step")
	    .Field<&openblack::storm_clouds::Drift::stepsLeft>("stepsLeft");
}
