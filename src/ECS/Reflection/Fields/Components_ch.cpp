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

#include "ECS/Components/ChallengePicture.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Components/ChangedMaterials.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_ch(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_ch(entt::meta_ctx& context)
{
	Reflect<components::ChallengePicture>(context)
	    .Field<&components::ChallengePicture::full>("full")
	    .Field<&components::ChallengePicture::small>("small")
	    .Field<&components::ChallengePicture::waiting>("waiting")
	    .Field<&components::ChallengePicture::asked>("asked")
	    .Field<&components::ChallengePicture::taken>("taken");
	Reflect<components::ChallengeRecord>(context)
	    .Field<&components::ChallengeRecord::challenge>("challenge")
	    .Field<&components::ChallengeRecord::order>("order")
	    .Field<&components::ChallengeRecord::reminderScript>("reminderScript")
	    .Field<&components::ChallengeRecord::arguments>("arguments")
	    .Field<&components::ChallengeRecord::title>("title")
	    .Field<&components::ChallengeRecord::success>("success")
	    .Field<&components::ChallengeRecord::alignment>("alignment")
	    .Field<&components::ChallengeRecord::position>("position")
	    .Field<&components::ChallengeRecord::focus>("focus")
	    .Field<&components::ChallengeRecord::quest>("quest")
	    .Field<&components::ChallengeRecord::startedTurn>("startedTurn")
	    .Field<&components::ChallengeRecord::doneTurn>("doneTurn");
	Reflect<components::ChangedMaterials>(context).Field<&components::ChangedMaterials::properties>("properties");
	Reflect<components::ChimneySmoke>(context)
	    .Field<&components::ChimneySmoke::chimney>("chimney")
	    .Field<&components::ChimneySmoke::rgb>("rgb")
	    .Field<&components::ChimneySmoke::state>("state")
	    .Field<&components::ChimneySmoke::puffs>("puffs");
}
