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

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/LeashRules.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturel(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creaturel(entt::meta_ctx& context)
{
	Reflect<openblack::creature_layers::BodyAction>(context, ValueOnly {})
	    .Field<&openblack::creature_layers::BodyAction::kind>("kind")
	    .Field<&openblack::creature_layers::BodyAction::phase>("phase")
	    .Field<&openblack::creature_layers::BodyAction::animations>("animations")
	    .Field<&openblack::creature_layers::BodyAction::timeMs>("timeMs")
	    .Field<&openblack::creature_layers::BodyAction::mirrored>("mirrored")
	    .Field<&openblack::creature_layers::BodyAction::endWanted>("endWanted")
	    .Field<&openblack::creature_layers::BodyAction::holdLoop>("holdLoop")
	    .Field<&openblack::creature_layers::BodyAction::timedByPlayer>("timedByPlayer")
	    .Field<&openblack::creature_layers::BodyAction::hurried>("hurried")
	    .Field<&openblack::creature_layers::BodyAction::keepsPace>("keepsPace")
	    .Field<&openblack::creature_layers::BodyAction::realTime>("realTime")
	    .Field<&openblack::creature_layers::BodyAction::danceMoveMs>("danceMoveMs")
	    .Field<&openblack::creature_layers::BodyAction::blendedWith>("blendedWith")
	    .Field<&openblack::creature_layers::BodyAction::blendWeight>("blendWeight")
	    .Field<&openblack::creature_layers::BodyAction::chooseDanceMove>("chooseDanceMove")
	    .Field<&openblack::creature_layers::BodyAction::firstDanceMove>("firstDanceMove");
	Reflect<openblack::creature_layers::FaceLayer>(context, ValueOnly {})
	    .Field<&openblack::creature_layers::FaceLayer::current>("current")
	    .Field<&openblack::creature_layers::FaceLayer::timeMs>("timeMs")
	    .Field<&openblack::creature_layers::FaceLayer::wanted>("wanted")
	    .Field<&openblack::creature_layers::FaceLayer::remainingMs>("remainingMs")
	    .Field<&openblack::creature_layers::FaceLayer::cue>("cue");
	Reflect<openblack::creature_layers::GestureLayer>(context, ValueOnly {})
	    .Field<&openblack::creature_layers::GestureLayer::animation>("animation")
	    .Field<&openblack::creature_layers::GestureLayer::timeMs>("timeMs");
	Reflect<openblack::creature_layers::LookAxis>(context, ValueOnly {})
	    .Field<&openblack::creature_layers::LookAxis::angle>("angle")
	    .Field<&openblack::creature_layers::LookAxis::velocity>("velocity");
	Reflect<openblack::creature_learning::Context>(context, ValueOnly {})
	    .Field<&openblack::creature_learning::Context::action>("action")
	    .Field<&openblack::creature_learning::Context::desire>("desire")
	    .Field<&openblack::creature_learning::Context::object>("object")
	    .Field<&openblack::creature_learning::Context::belief>("belief")
	    .Field<&openblack::creature_learning::Context::activity>("activity")
	    .Field<&openblack::creature_learning::Context::activityBelief>("activityBelief")
	    .Field<&openblack::creature_learning::Context::running>("running")
	    .Field<&openblack::creature_learning::Context::secondsSince>("secondsSince")
	    .Field<&openblack::creature_learning::Context::learnable>("learnable")
	    .Field<&openblack::creature_learning::Context::windowSeconds>("windowSeconds")
	    .Field<&openblack::creature_learning::Context::credited>("credited")
	    .Field<&openblack::creature_learning::Context::source>("source")
	    .Field<&openblack::creature_learning::Context::ordered>("ordered")
	    .Field<&openblack::creature_learning::Context::done>("done")
	    .Field<&openblack::creature_learning::Context::seen>("seen");
	Reflect<openblack::creature_learning::CreatureAttitude>(context, ValueOnly {})
	    .Field<&openblack::creature_learning::CreatureAttitude::creature>("creature")
	    .Field<&openblack::creature_learning::CreatureAttitude::howNice>("howNice")
	    .Field<&openblack::creature_learning::CreatureAttitude::howImpressive>("howImpressive")
	    .Field<&openblack::creature_learning::CreatureAttitude::attention>("attention");
	Reflect<openblack::creature_leash::Lesson>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::Lesson::desire>("desire")
	    .Field<&openblack::creature_leash::Lesson::change>("change");
	Reflect<openblack::creature_leash::MindHooks>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::MindHooks::attitudeTurn>("attitudeTurn")
	    .Field<&openblack::creature_leash::MindHooks::learningInHand>("learningInHand")
	    .Field<&openblack::creature_leash::MindHooks::miracleSightingWeight>("miracleSightingWeight")
	    .Field<&openblack::creature_leash::MindHooks::shown>("shown")
	    .Field<&openblack::creature_leash::MindHooks::actOn>("actOn")
	    .Field<&openblack::creature_leash::MindHooks::attitudes>("attitudes");
	Reflect<openblack::creature_leash::MindHooks::Attitude>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::MindHooks::Attitude::creature>("creature")
	    .Field<&openblack::creature_leash::MindHooks::Attitude::change>("change");
	Reflect<openblack::creature_leash::MindHooks::Shown>(context, ValueOnly {})
	    .Field<&openblack::creature_leash::MindHooks::Shown::object>("object")
	    .Field<&openblack::creature_leash::MindHooks::Shown::type>("type")
	    .Field<&openblack::creature_leash::MindHooks::Shown::lessons>("lessons");
	Reflect<openblack::creature_locomotion::Pair>(context, ValueOnly {})
	    .Field<&openblack::creature_locomotion::Pair::from>("from")
	    .Field<&openblack::creature_locomotion::Pair::to>("to")
	    .Field<&openblack::creature_locomotion::Pair::weight>("weight");
	Reflect<openblack::creature_locomotion::Ring>(context, ValueOnly {})
	    .Field<&openblack::creature_locomotion::Ring::min>("min")
	    .Field<&openblack::creature_locomotion::Ring::max>("max");
	Reflect<openblack::creature_locomotion::Speeds>(context, ValueOnly {})
	    .Field<&openblack::creature_locomotion::Speeds::walk>("walk")
	    .Field<&openblack::creature_locomotion::Speeds::run>("run");
	Reflect<openblack::creature_look::NoticedMover>(context, ValueOnly {})
	    .Field<&openblack::creature_look::NoticedMover::at>("at")
	    .Field<&openblack::creature_look::NoticedMover::height>("height")
	    .Field<&openblack::creature_look::NoticedMover::turn>("turn");
	Reflect<openblack::creature_look::Target>(context, ValueOnly {})
	    .Field<&openblack::creature_look::Target::id>("id")
	    .Field<&openblack::creature_look::Target::sight>("sight")
	    .Field<&openblack::creature_look::Target::place>("place")
	    .Field<&openblack::creature_look::Target::interest>("interest")
	    .Field<&openblack::creature_look::Target::point>("point")
	    .Field<&openblack::creature_look::Target::watchedTurns>("watchedTurns");
	Reflect<openblack::creature_look::WhileGoing>(context, ValueOnly {})
	    .Field<&openblack::creature_look::WhileGoing::look>("look")
	    .Field<&openblack::creature_look::WhileGoing::turns>("turns")
	    .Field<&openblack::creature_look::WhileGoing::sweepAngle>("sweepAngle")
	    .Field<&openblack::creature_look::WhileGoing::sweepAngleStep>("sweepAngleStep")
	    .Field<&openblack::creature_look::WhileGoing::sweepDistance>("sweepDistance")
	    .Field<&openblack::creature_look::WhileGoing::sweepDistanceStep>("sweepDistanceStep");
}
