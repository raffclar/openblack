/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How the local player's creature reacts when a custom music track starts

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Creature/CreatureFollowUp.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureMusicReaction.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void CreatureMindSystem::ReactToMusic(entt::entity creature, float mood)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (mind == nullptr || body == nullptr)
	{
		return;
	}
	const auto random = [this](uint32_t range) { return Random(range); };
	// It is made to look or point at the camera, or to be sad, and the reaction's steps take the place of the plan's
	const auto plan = creature_music::PlanFor(mood, Random(3));
	creature_music::Camera camera;
	if (Locator::camera::has_value())
	{
		camera = {.position = Locator::camera::value().GetOrigin(), .known = true};
	}
	const auto reaction = creature_music::ReactionTo(mood);
	std::vector<creature_mind::Step> agenda;
	std::optional<creature_follow_up::FollowUp> followUp;
	std::optional<uint32_t> help;
	switch (reaction)
	{
	case creature_music::Reaction::New:
		agenda = creature_music::NewTrack(random, body->alignment, camera);
		followUp = creature_follow_up::FollowUp::ScratchAfterNewMusic;
		break;
	case creature_music::Reaction::Liked:
		agenda = creature_music::LikedTrack(random, mood, camera);
		followUp = creature_music::ShowsHappyAfterLikedTrack(Random(100)) ? creature_follow_up::FollowUp::ShowHappy
		                                                                  : creature_follow_up::FollowUp::DanceAfterLikedMusic;
		help = creature_music::k_NiceMusicHelp;
		break;
	case creature_music::Reaction::Disliked:
		agenda = creature_music::DislikedTrack(random, body->alignment, camera);
		followUp = creature_follow_up::FollowUp::ShowSlapFeeling;
		help = creature_music::k_NastyMusicHelp;
		break;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} reacts to music with mood {}: {} steps", entt::to_integral(creature),
	                    mood, agenda.size());
	static_cast<void>(ForcePlan(
	    creature,
	    {.desire = plan.desire, .action = plan.action, .object = creature, .agenda = std::move(agenda), .ordered = true}));
	// When the steps are done, it does what waits: it scratches, shows it is happy, dances or shows how it feels
	mind->followUp = followUp;
	mind->followUpValue = creature_music::FollowUpValue(mood);
	if (help.has_value() && Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(creature,
		                                           {.kind = creature_help::Kind::MiscellaneousStacked, .parameter = *help});
	}
	const auto dominant =
	    creature_music::DominantAfter(reaction, reaction == creature_music::Reaction::Disliked ? Random(100) : 0u);
	MakeDesireDominant(creature, dominant.desire, dominant.seconds, true);
}
