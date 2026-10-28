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

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturem(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creaturem(entt::meta_ctx& context)
{
	Reflect<openblack::creature_marks::Mark>(context, ValueOnly {})
	    .Field<&openblack::creature_marks::Mark::u>("u")
	    .Field<&openblack::creature_marks::Mark::v>("v")
	    .Field<&openblack::creature_marks::Mark::skin>("skin")
	    .Field<&openblack::creature_marks::Mark::age>("age")
	    .Field<&openblack::creature_marks::Mark::type>("type")
	    .Field<&openblack::creature_marks::Mark::column>("column");
	Reflect<openblack::creature_marks::Marks>(context, ValueOnly {})
	    .Field<&openblack::creature_marks::Marks::wounds>("wounds")
	    .Field<&openblack::creature_marks::Marks::blood>("blood")
	    .Field<&openblack::creature_marks::Marks::counts>("counts");
	Reflect<openblack::creature_meals::Tallies>(context, ValueOnly {})
	    .Field<&openblack::creature_meals::Tallies::peopleKilled>("peopleKilled")
	    .Field<&openblack::creature_meals::Tallies::animalsKilled>("animalsKilled")
	    .Field<&openblack::creature_meals::Tallies::creaturesKilled>("creaturesKilled")
	    .Field<&openblack::creature_meals::Tallies::keptDown>("keptDown");
	Reflect<openblack::creature_mind::CastOrder>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::CastOrder::magicType>("magicType")
	    .Field<&openblack::creature_mind::CastOrder::object>("object")
	    .Field<&openblack::creature_mind::CastOrder::place>("place");
	Reflect<openblack::creature_mind::DanceOrder>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::DanceOrder::kind>("kind")
	    .Field<&openblack::creature_mind::DanceOrder::point>("point")
	    .Field<&openblack::creature_mind::DanceOrder::type>("type")
	    .Field<&openblack::creature_mind::DanceOrder::villager>("villager")
	    .Field<&openblack::creature_mind::DanceOrder::count>("count");
	Reflect<openblack::creature_mind::Gaze>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::Gaze::object>("object")
	    .Field<&openblack::creature_mind::Gaze::bottom>("bottom")
	    .Field<&openblack::creature_mind::Gaze::itself>("itself")
	    .Field<&openblack::creature_mind::Gaze::hand>("hand")
	    .Field<&openblack::creature_mind::Gaze::player>("player")
	    .Field<&openblack::creature_mind::Gaze::point>("point")
	    .Field<&openblack::creature_mind::Gaze::pointOnGround>("pointOnGround")
	    .Field<&openblack::creature_mind::Gaze::camera>("camera")
	    .Field<&openblack::creature_mind::Gaze::whileGoing>("whileGoing")
	    .Field<&openblack::creature_mind::Gaze::runningAway>("runningAway")
	    .Field<&openblack::creature_mind::Gaze::feet>("feet");
	Reflect<openblack::creature_mind::IdleMind>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::IdleMind::activity>("activity")
	    .Field<&openblack::creature_mind::IdleMind::agenda>("agenda")
	    .Field<&openblack::creature_mind::IdleMind::step>("step")
	    .Field<&openblack::creature_mind::IdleMind::stepStarted>("stepStarted")
	    .Field<&openblack::creature_mind::IdleMind::stepSeconds>("stepSeconds")
	    .Field<&openblack::creature_mind::IdleMind::sitEnding>("sitEnding")
	    .Field<&openblack::creature_mind::IdleMind::faintLay>("faintLay")
	    .Field<&openblack::creature_mind::IdleMind::faceSeconds>("faceSeconds")
	    .Field<&openblack::creature_mind::IdleMind::eyesToClear>("eyesToClear")
	    .Field<&openblack::creature_mind::IdleMind::faceVariety>("faceVariety")
	    .Field<&openblack::creature_mind::IdleMind::shown>("shown")
	    .Field<&openblack::creature_mind::IdleMind::wakeWanted>("wakeWanted")
	    .Field<&openblack::creature_mind::IdleMind::serial>("serial")
	    .Field<&openblack::creature_mind::IdleMind::handsChecked>("handsChecked")
	    .Field<&openblack::creature_mind::IdleMind::fightAgreed>("fightAgreed")
	    .Field<&openblack::creature_mind::IdleMind::gaveUp>("gaveUp")
	    .Field<&openblack::creature_mind::IdleMind::stoppedByScript>("stoppedByScript")
	    .Field<&openblack::creature_mind::IdleMind::stoppedSeen>("stoppedSeen")
	    .Field<&openblack::creature_mind::IdleMind::usedUpHeld>("usedUpHeld")
	    .Field<&openblack::creature_mind::IdleMind::castDone>("castDone")
	    .Field<&openblack::creature_mind::IdleMind::castTurns>("castTurns")
	    .Field<&openblack::creature_mind::IdleMind::inspectTurns>("inspectTurns")
	    .Field<&openblack::creature_mind::IdleMind::watchTurns>("watchTurns")
	    .Field<&openblack::creature_mind::IdleMind::followHolding>("followHolding")
	    .Field<&openblack::creature_mind::IdleMind::followHoldTurns>("followHoldTurns")
	    .Field<&openblack::creature_mind::IdleMind::followMoved>("followMoved")
	    .Field<&openblack::creature_mind::IdleMind::followHandTurns>("followHandTurns")
	    .Field<&openblack::creature_mind::IdleMind::restTurns>("restTurns")
	    .Field<&openblack::creature_mind::IdleMind::danceTurns>("danceTurns")
	    .Field<&openblack::creature_mind::IdleMind::danceEnding>("danceEnding")
	    .Field<&openblack::creature_mind::IdleMind::danceSet>("danceSet")
	    .Field<&openblack::creature_mind::IdleMind::followCameraTurns>("followCameraTurns")
	    .Field<&openblack::creature_mind::IdleMind::movedOn>("movedOn");
	Reflect<openblack::creature_mind::IdleMind::StepRef>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::IdleMind::StepRef::serial>("serial")
	    .Field<&openblack::creature_mind::IdleMind::StepRef::step>("step");
	Reflect<openblack::creature_mind::Movement>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::Movement::kind>("kind")
	    .Field<&openblack::creature_mind::Movement::point>("point")
	    .Field<&openblack::creature_mind::Movement::object>("object")
	    .Field<&openblack::creature_mind::Movement::run>("run")
	    .Field<&openblack::creature_mind::Movement::extraSpeed>("extraSpeed")
	    .Field<&openblack::creature_mind::Movement::minDistance>("minDistance")
	    .Field<&openblack::creature_mind::Movement::maxDistance>("maxDistance")
	    .Field<&openblack::creature_mind::Movement::giveUpIfUnreachable>("giveUpIfUnreachable");
	Reflect<openblack::creature_mind::ObjectOrder>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::ObjectOrder::kind>("kind")
	    .Field<&openblack::creature_mind::ObjectOrder::handful>("handful")
	    .Field<&openblack::creature_mind::ObjectOrder::object>("object")
	    .Field<&openblack::creature_mind::ObjectOrder::point>("point")
	    .Field<&openblack::creature_mind::ObjectOrder::animation>("animation")
	    .Field<&openblack::creature_mind::ObjectOrder::pointHeight>("pointHeight")
	    .Field<&openblack::creature_mind::ObjectOrder::seconds>("seconds")
	    .Field<&openblack::creature_mind::ObjectOrder::mirrored>("mirrored")
	    .Field<&openblack::creature_mind::ObjectOrder::atObject>("atObject")
	    .Field<&openblack::creature_mind::ObjectOrder::swallows>("swallows");
	Reflect<openblack::creature_mind::Step>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::Step::kind>("kind")
	    .Field<&openblack::creature_mind::Step::seconds>("seconds")
	    .Field<&openblack::creature_mind::Step::extraSeconds>("extraSeconds")
	    .Field<&openblack::creature_mind::Step::turns>("turns")
	    .Field<&openblack::creature_mind::Step::animation>("animation")
	    .Field<&openblack::creature_mind::Step::sleepyEyes>("sleepyEyes")
	    .Field<&openblack::creature_mind::Step::movement>("movement")
	    .Field<&openblack::creature_mind::Step::sequence>("sequence")
	    .Field<&openblack::creature_mind::Step::untilRested>("untilRested")
	    .Field<&openblack::creature_mind::Step::untilBetter>("untilBetter")
	    .Field<&openblack::creature_mind::Step::holdLoop>("holdLoop")
	    .Field<&openblack::creature_mind::Step::retriedClip>("retriedClip")
	    .Field<&openblack::creature_mind::Step::closedEyes>("closedEyes")
	    .Field<&openblack::creature_mind::Step::effect>("effect")
	    .Field<&openblack::creature_mind::Step::object>("object")
	    .Field<&openblack::creature_mind::Step::order>("order")
	    .Field<&openblack::creature_mind::Step::cast>("cast")
	    .Field<&openblack::creature_mind::Step::face>("face")
	    .Field<&openblack::creature_mind::Step::gaze>("gaze")
	    .Field<&openblack::creature_mind::Step::heldEyes>("heldEyes")
	    .Field<&openblack::creature_mind::Step::flutterEyelids>("flutterEyelids")
	    .Field<&openblack::creature_mind::Step::dance>("dance")
	    .Field<&openblack::creature_mind::Step::faint>("faint")
	    .Field<&openblack::creature_mind::Step::main>("main")
	    .Field<&openblack::creature_mind::Step::request>("request")
	    .Field<&openblack::creature_mind::Step::ordered>("ordered")
	    .Field<&openblack::creature_mind::Step::desire>("desire")
	    .Field<&openblack::creature_mind::Step::saved>("saved");
	Reflect<openblack::creature_mind::Step::Saved>(context, ValueOnly {})
	    .Field<&openblack::creature_mind::Step::Saved::kind>("kind")
	    .Field<&openblack::creature_mind::Step::Saved::integer>("integer")
	    .Field<&openblack::creature_mind::Step::Saved::point>("point")
	    .Field<&openblack::creature_mind::Step::Saved::real>("real");
	Reflect<openblack::creature_mind_model::Learnt>(context, ValueOnly {})
	    .Field<&openblack::creature_mind_model::Learnt::opinions>("opinions")
	    .Field<&openblack::creature_mind_model::Learnt::turnsSinceDone>("turnsSinceDone")
	    .Field<&openblack::creature_mind_model::Learnt::episodes>("episodes")
	    .Field<&openblack::creature_mind_model::Learnt::trees>("trees")
	    .Field<&openblack::creature_mind_model::Learnt::treeHeaders>("treeHeaders")
	    .Field<&openblack::creature_mind_model::Learnt::contexts>("contexts")
	    .Field<&openblack::creature_mind_model::Learnt::creatures>("creatures")
	    .Field<&openblack::creature_mind_model::Learnt::knowledge>("knowledge")
	    .Field<&openblack::creature_mind_model::Learnt::mimicry>("mimicry")
	    .Field<&openblack::creature_mind_model::Learnt::thoughts>("thoughts")
	    .Field<&openblack::creature_mind_model::Learnt::initialThresholds>("initialThresholds")
	    .Field<&openblack::creature_mind_model::Learnt::name>("name")
	    .Field<&openblack::creature_mind_model::Learnt::alignment>("alignment")
	    .Field<&openblack::creature_mind_model::Learnt::developmentTimer>("developmentTimer")
	    .Field<&openblack::creature_mind_model::Learnt::leashWords>("leashWords")
	    .Field<&openblack::creature_mind_model::Learnt::file>("file");
	Reflect<openblack::creature_mind_model::TreeHeader>(context, ValueOnly {})
	    .Field<&openblack::creature_mind_model::TreeHeader::type>("type")
	    .Field<&openblack::creature_mind_model::TreeHeader::desire>("desire");
	Reflect<openblack::creature_morph::Morph>(context, ValueOnly {})
	    .Field<&openblack::creature_morph::Morph::evilGood>("evilGood")
	    .Field<&openblack::creature_morph::Morph::thinFat>("thinFat")
	    .Field<&openblack::creature_morph::Morph::weakStrong>("weakStrong");
}
