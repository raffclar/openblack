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

#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFileSave.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creaturef(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creaturef(entt::meta_ctx& context)
{
	Reflect<components::CreatureFighting>(context)
	    .Field<&components::CreatureFighting::stage>("stage")
	    .Field<&components::CreatureFighting::opponent>("opponent")
	    .Field<&components::CreatureFighting::arena>("arena")
	    .Field<&components::CreatureFighting::madeArena>("madeArena")
	    .Field<&components::CreatureFighting::arenaEntity>("arenaEntity")
	    .Field<&components::CreatureFighting::planned>("planned")
	    .Field<&components::CreatureFighting::fighter>("fighter")
	    .Field<&components::CreatureFighting::marks>("marks")
	    .Field<&components::CreatureFighting::reaches>("reaches")
	    .Field<&components::CreatureFighting::hitTimesMs>("hitTimesMs")
	    .Field<&components::CreatureFighting::measured>("measured")
	    .Field<&components::CreatureFighting::stageSeconds>("stageSeconds")
	    .Field<&components::CreatureFighting::faceSeconds>("faceSeconds")
	    .Field<&components::CreatureFighting::played>("played")
	    .Field<&components::CreatureFighting::taunted>("taunted")
	    .Field<&components::CreatureFighting::rude>("rude")
	    .Field<&components::CreatureFighting::ended>("ended")
	    .Field<&components::CreatureFighting::held>("held")
	    .Field<&components::CreatureFighting::partnerLeft>("partnerLeft");
	Reflect<components::CreatureFightRecord>(context)
	    .Field<&components::CreatureFightRecord::tendency>("tendency")
	    .Field<&components::CreatureFightRecord::choosingTendency>("choosingTendency")
	    .Field<&components::CreatureFightRecord::fights>("fights")
	    .Field<&components::CreatureFightRecord::wins>("wins")
	    .Field<&components::CreatureFightRecord::control>("control")
	    .Field<&components::CreatureFightRecord::struckTurn>("struckTurn")
	    .Field<&components::CreatureFightRecord::health>("health");
	Reflect<components::CreatureAfterFight>(context).Field<&components::CreatureAfterFight::agenda>("agenda");
	Reflect<components::CreatureKnockedOut>(context)
	    .Field<&components::CreatureKnockedOut::stage>("stage")
	    .Field<&components::CreatureKnockedOut::seconds>("seconds")
	    .Field<&components::CreatureKnockedOut::layStill>("layStill")
	    .Field<&components::CreatureKnockedOut::fainting>("fainting")
	    .Field<&components::CreatureKnockedOut::handsChecked>("handsChecked")
	    .Field<&components::CreatureKnockedOut::rest>("rest")
	    .Field<&components::CreatureKnockedOut::permanent>("permanent")
	    .Field<&components::CreatureKnockedOut::home>("home")
	    .Field<&components::CreatureKnockedOut::fizzTurns>("fizzTurns");
	Reflect<components::CreatureFileSave>(context).Field<&components::CreatureFileSave::turnsSeen>("turnsSeen");
	Reflect<components::CreatureFizz>(context).Field<&components::CreatureFizz::fizz>("fizz");
	Reflect<components::CreatureFriends>(context).Field<&components::CreatureFriends::friends>("friends");
}
