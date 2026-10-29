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

#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_p(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_p(entt::meta_ctx& context)
{
	Reflect<components::InPhysics> {context};
	Reflect<components::Immovable> {context};
	Reflect<components::BuildProgress>(context).Field<&components::BuildProgress::built>("built");
	Reflect<components::RepairSite>(context).Field<&components::RepairSite::startLife>("startLife");
	Reflect<components::PhysicsDrawPose>(context)
	    .Field<&components::PhysicsDrawPose::axes>("axes")
	    .Field<&components::PhysicsDrawPose::origin>("origin")
	    .Field<&components::PhysicsDrawPose::underSea>("underSea");
	Reflect<components::Player>(context)
	    .Field<&components::Player::name>("name")
	    .Field<&components::Player::handTookFrom>("handTookFrom")
	    .Field<&components::Player::damageFrom>("damageFrom")
	    .Field<&components::Player::lastCast>("lastCast")
	    .Field<&components::Player::lastSeedGained>("lastSeedGained")
	    .Field<&components::Player::windResistance>("windResistance")
	    .Field<&components::Player::miracles>("miracles");
	Reflect<components::PlayerCreatures>(context).Field<&components::PlayerCreatures::acquired>("acquired");
	Reflect<components::PlayerAllies>(context).Field<&components::PlayerAllies::strength>("strength");
	Reflect<components::ComputerAttitudes>(context).Field<&components::ComputerAttitudes::kept>("kept");
	Reflect<components::PlayerInfluencePower>(context)
	    .Field<&components::PlayerInfluencePower::power>("power")
	    .Field<&components::PlayerInfluencePower::average>("average");
	Reflect<components::PlayerStatistics>(context)
	    .Field<&components::PlayerStatistics::births>("births")
	    .Field<&components::PlayerStatistics::deaths>("deaths")
	    .Field<&components::PlayerStatistics::peopleKilled>("peopleKilled")
	    .Field<&components::PlayerStatistics::sacrifices>("sacrifices")
	    .Field<&components::PlayerStatistics::peopleConverted>("peopleConverted")
	    .Field<&components::PlayerStatistics::built>("built")
	    .Field<&components::PlayerStatistics::buildingsDestroyed>("buildingsDestroyed")
	    .Field<&components::PlayerStatistics::disciplesMade>("disciplesMade")
	    .Field<&components::PlayerStatistics::foodUsed>("foodUsed")
	    .Field<&components::PlayerStatistics::woodUsed>("woodUsed")
	    .Field<&components::PlayerStatistics::godsDefeated>("godsDefeated")
	    .Field<&components::PlayerStatistics::castsOfType>("castsOfType")
	    .Field<&components::PlayerStatistics::chantsUsed>("chantsUsed")
	    .Field<&components::PlayerStatistics::satisfaction>("satisfaction")
	    .Field<&components::PlayerStatistics::population>("population")
	    .Field<&components::PlayerStatistics::men>("men")
	    .Field<&components::PlayerStatistics::women>("women")
	    .Field<&components::PlayerStatistics::belief>("belief")
	    .Field<&components::PlayerStatistics::startAlignment>("startAlignment")
	    .Field<&components::PlayerStatistics::startCreatureAlignment>("startCreatureAlignment")
	    .Field<&components::PlayerStatistics::startCreatureSize>("startCreatureSize")
	    .Field<&components::PlayerStatistics::influenceHistory>("influenceHistory")
	    .Field<&components::PlayerStatistics::populationHistory>("populationHistory");
	Reflect<components::Poisoned>(context).Field<&components::Poisoned::dummy>("dummy");
	Reflect<components::CreatureMiracleOpinion>(context)
	    .Field<&components::CreatureMiracleOpinion::lastTurn>("lastTurn")
	    .Field<&components::CreatureMiracleOpinion::changed>("changed");
	Reflect<components::Pot>(context)
	    .Field<&components::Pot::amount>("amount")
	    .Field<&components::Pot::maxAmount>("maxAmount")
	    .Field<&components::Pot::type>("type")
	    .Field<&components::Pot::poisoned>("poisoned")
	    .Field<&components::Pot::town>("town");
	Reflect<components::PrayerPower>(context)
	    .Field<&components::PrayerPower::chants>("chants")
	    .Field<&components::PrayerPower::infinite>("infinite");
	Reflect<components::HanoiBlock>(context)
	    .Field<&components::HanoiBlock::puzzle>("puzzle")
	    .Field<&components::HanoiBlock::home>("home")
	    .Field<&components::HanoiBlock::placedAt>("placedAt")
	    .Field<&components::HanoiBlock::above>("above")
	    .Field<&components::HanoiBlock::below>("below")
	    .Field<&components::HanoiBlock::fixed>("fixed");
	Reflect<components::PuzzleGame>(context)
	    .Field<&components::PuzzleGame::type>("type")
	    .Field<&components::PuzzleGame::place>("place")
	    .Field<&components::PuzzleGame::angle>("angle")
	    .Field<&components::PuzzleGame::scale>("scale")
	    .Field<&components::PuzzleGame::cosineStep>("cosineStep")
	    .Field<&components::PuzzleGame::sineStep>("sineStep")
	    .Field<&components::PuzzleGame::played>("played")
	    .Field<&components::PuzzleGame::started>("started")
	    .Field<&components::PuzzleGame::calmTurns>("calmTurns")
	    .Field<&components::PuzzleGame::order>("order")
	    .Field<&components::PuzzleGame::bases>("bases")
	    .Field<&components::PuzzleGame::rings>("rings")
	    .Field<&components::PuzzleGame::kept>("kept")
	    .Field<&components::PuzzleGame::treeCounts>("treeCounts")
	    .Field<&components::PuzzleGame::trees>("trees")
	    .Field<&components::PuzzleGame::treesPlacedAt>("treesPlacedAt")
	    .Field<&components::PuzzleGame::mists>("mists")
	    .Field<&components::PuzzleGame::bats>("bats")
	    .Field<&components::PuzzleGame::lanterns>("lanterns");
}
