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

#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureFizz.h"
#include "Creature/FightMarks.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturef(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creaturef(entt::meta_ctx& context)
{
	Reflect<openblack::creature_feedback::HardSlaps>(context, ValueOnly {})
	    .Field<&openblack::creature_feedback::HardSlaps::band>("band")
	    .Field<&openblack::creature_feedback::HardSlaps::count>("count");
	Reflect<openblack::creature_feedback::StrokeClocks>(context, ValueOnly {})
	    .Field<&openblack::creature_feedback::StrokeClocks::sinceTouch>("sinceTouch")
	    .Field<&openblack::creature_feedback::StrokeClocks::ready>("ready")
	    .Field<&openblack::creature_feedback::StrokeClocks::contact>("contact")
	    .Field<&openblack::creature_feedback::StrokeClocks::sinceHold>("sinceHold");
	Reflect<openblack::creature_fight::Arena>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::Arena::centre>("centre")
	    .Field<&openblack::creature_fight::Arena::radius>("radius");
	Reflect<openblack::creature_fight::Fighter>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::Fighter::state>("state")
	    .Field<&openblack::creature_fight::Fighter::animation>("animation")
	    .Field<&openblack::creature_fight::Fighter::timeMs>("timeMs")
	    .Field<&openblack::creature_fight::Fighter::stateMs>("stateMs")
	    .Field<&openblack::creature_fight::Fighter::speed>("speed")
	    .Field<&openblack::creature_fight::Fighter::mirrored>("mirrored")
	    .Field<&openblack::creature_fight::Fighter::turning>("turning")
	    .Field<&openblack::creature_fight::Fighter::lastAction>("lastAction")
	    .Field<&openblack::creature_fight::Fighter::special>("special")
	    .Field<&openblack::creature_fight::Fighter::spell>("spell")
	    .Field<&openblack::creature_fight::Fighter::stopping>("stopping")
	    .Field<&openblack::creature_fight::Fighter::hurried>("hurried")
	    .Field<&openblack::creature_fight::Fighter::queue>("queue")
	    .Field<&openblack::creature_fight::Fighter::health>("health")
	    .Field<&openblack::creature_fight::Fighter::stamina>("stamina")
	    .Field<&openblack::creature_fight::Fighter::control>("control")
	    .Field<&openblack::creature_fight::Fighter::computerWaitMs>("computerWaitMs")
	    .Field<&openblack::creature_fight::Fighter::tendency>("tendency");
	Reflect<openblack::creature_fight::Move>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::Move::kind>("kind")
	    .Field<&openblack::creature_fight::Move::value>("value");
	Reflect<openblack::creature_fight::QueuedMove>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::QueuedMove::move>("move")
	    .Field<&openblack::creature_fight::QueuedMove::chargeMs>("chargeMs");
	Reflect<openblack::creature_fight::Reach>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::Reach::animation>("animation")
	    .Field<&openblack::creature_fight::Reach::point>("point")
	    .Field<&openblack::creature_fight::Reach::height>("height")
	    .Field<&openblack::creature_fight::Reach::swing>("swing");
	Reflect<openblack::creature_fight::marks::Mark>(context, ValueOnly {})
	    .Field<&openblack::creature_fight::marks::Mark::anchor>("anchor")
	    .Field<&openblack::creature_fight::marks::Mark::subMesh>("subMesh")
	    .Field<&openblack::creature_fight::marks::Mark::firstIndex>("firstIndex")
	    .Field<&openblack::creature_fight::marks::Mark::s>("s")
	    .Field<&openblack::creature_fight::marks::Mark::t>("t")
	    .Field<&openblack::creature_fight::marks::Mark::local>("local")
	    .Field<&openblack::creature_fight::marks::Mark::chargeMs>("chargeMs")
	    .Field<&openblack::creature_fight::marks::Mark::frame>("frame");
	Reflect<openblack::creature_fizz::Fizz>(context, ValueOnly {})
	    .Field<&openblack::creature_fizz::Fizz::now>("now")
	    .Field<&openblack::creature_fizz::Fizz::target>("target")
	    .Field<&openblack::creature_fizz::Fizz::perSecond>("perSecond")
	    .Field<&openblack::creature_fizz::Fizz::goesForGood>("goesForGood");
}
