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

#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_co(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_co(entt::meta_ctx& context)
{
	Reflect<openblack::computer_hand::Argument>(context, ValueOnly {})
	    .Field<&openblack::computer_hand::Argument::object>("object")
	    .Field<&openblack::computer_hand::Argument::number>("number")
	    .Field<&openblack::computer_hand::Argument::value>("value")
	    .Field<&openblack::computer_hand::Argument::place>("place");
	Reflect<openblack::computer_hand::HandAction>(context, ValueOnly {})
	    .Field<&openblack::computer_hand::HandAction::hand>("hand")
	    .Field<&openblack::computer_hand::HandAction::coords>("coords")
	    .Field<&openblack::computer_hand::HandAction::target>("target")
	    .Field<&openblack::computer_hand::HandAction::kept>("kept")
	    .Field<&openblack::computer_hand::HandAction::speed>("speed")
	    .Field<&openblack::computer_hand::HandAction::steps>("steps")
	    .Field<&openblack::computer_hand::HandAction::part>("part")
	    .Field<&openblack::computer_hand::HandAction::moveFirst>("moveFirst")
	    .Field<&openblack::computer_hand::HandAction::insertedMove>("insertedMove")
	    .Field<&openblack::computer_hand::HandAction::locked>("locked")
	    .Field<&openblack::computer_hand::HandAction::counter>("counter")
	    .Field<&openblack::computer_hand::HandAction::progress>("progress")
	    .Field<&openblack::computer_hand::HandAction::castTarget>("castTarget")
	    .Field<&openblack::computer_hand::HandAction::camera>("camera");
	Reflect<openblack::computer_hand::Step>(context, ValueOnly {})
	    .Field<&openblack::computer_hand::Step::kind>("kind")
	    .Field<&openblack::computer_hand::Step::arguments>("arguments");
	Reflect<openblack::computer_mind::Mind>(context, ValueOnly {})
	    .Field<&openblack::computer_mind::Mind::nodes>("nodes")
	    .Field<&openblack::computer_mind::Mind::doing>("doing")
	    .Field<&openblack::computer_mind::Mind::lastEmptyTurn>("lastEmptyTurn")
	    .Field<&openblack::computer_mind::Mind::wasBusy>("wasBusy")
	    .Field<&openblack::computer_mind::Mind::paused>("paused")
	    .Field<&openblack::computer_mind::Mind::scriptHold>("scriptHold")
	    .Field<&openblack::computer_mind::Mind::workedOut>("workedOut")
	    .Field<&openblack::computer_mind::Mind::castRequested>("castRequested")
	    .Field<&openblack::computer_mind::Mind::queue>("queue")
	    .Field<&openblack::computer_mind::Mind::scriptHand>("scriptHand");
	Reflect<openblack::computer_mind::Node>(context, ValueOnly {})
	    .Field<&openblack::computer_mind::Node::kind>("kind")
	    .Field<&openblack::computer_mind::Node::parent>("parent")
	    .Field<&openblack::computer_mind::Node::children>("children")
	    .Field<&openblack::computer_mind::Node::childCount>("childCount")
	    .Field<&openblack::computer_mind::Node::suppression>("suppression")
	    .Field<&openblack::computer_mind::Node::weight>("weight")
	    .Field<&openblack::computer_mind::Node::lastWorkedOut>("lastWorkedOut")
	    .Field<&openblack::computer_mind::Node::lastPerformed>("lastPerformed")
	    .Field<&openblack::computer_mind::Node::value>("value")
	    .Field<&openblack::computer_mind::Node::bestLeaf>("bestLeaf")
	    .Field<&openblack::computer_mind::Node::object>("object")
	    .Field<&openblack::computer_mind::Node::amount>("amount")
	    .Field<&openblack::computer_mind::Node::argument>("argument")
	    .Field<&openblack::computer_mind::Node::place>("place");
	Reflect<openblack::computer_mind::QueuedAction>(context, ValueOnly {})
	    .Field<&openblack::computer_mind::QueuedAction::name>("name")
	    .Field<&openblack::computer_mind::QueuedAction::first>("first")
	    .Field<&openblack::computer_mind::QueuedAction::second>("second")
	    .Field<&openblack::computer_mind::QueuedAction::turn>("turn");
	Reflect<openblack::computer_mind::SeenThing>(context, ValueOnly {})
	    .Field<&openblack::computer_mind::SeenThing::thing>("thing")
	    .Field<&openblack::computer_mind::SeenThing::turn>("turn");
}
