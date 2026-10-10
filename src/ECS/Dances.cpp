/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Dances.h"

#include <algorithm>
#include <utility>

#include <DanceFile.h>

#include "ECS/Components/Dance.h"
#include "ECS/DanceRules.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Anything that isn't a villager dances as a dancer of no kind in particular
constexpr uint32_t k_LivingDanceType = 0;
} // namespace

entt::entity dances::Create(Registry& registry, const DanceSetup& setup, std::shared_ptr<const dance::DanceFile> file)
{
	const auto entity = registry.Create();
	auto& dance = registry.Assign<Dance>(entity);
	dance.type = setup.type;
	dance.place = setup.place;
	dance.owner = setup.owner;
	dance.duration = setup.duration;
	dance.autostart = setup.autostart;
	dance.madeByScript = setup.madeByScript;
	// A dance always has one group, and as many as its file names
	dance.groups.all.resize(1);
	if (file != nullptr)
	{
		dance.groups.all.resize(std::max<std::size_t>(1, file->groupCount));
		for (std::size_t i = 0; i < file->groupNames.size() && i < dance.groups.all.size(); ++i)
		{
			dance.groups.all[i].name = file->groupNames[i];
		}
		dance.clock = file->beat;
		dance.loopLength = file->loops;
		dance_rules::ApplyKeyFramesUpTo(dance.groups, *file, dance.clock);
	}
	dance.file = std::move(file);
	dance_rules::SetSpeed(dance, dance_rules::k_MadeSpeed);
	return entity;
}

bool dances::IsDance(const Registry& registry, entt::entity thing)
{
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<Dance>(thing);
}

entt::entity dances::DanceOf(const Registry& registry, entt::entity living)
{
	const auto* dancer = registry.Valid(living) ? registry.TryGet<const Dancer>(living) : nullptr;
	return dancer != nullptr && IsDance(registry, dancer->dance) ? dancer->dance : entt::null;
}

uint32_t dances::Size(const Registry& registry, entt::entity dance)
{
	return IsDance(registry, dance) ? registry.Get<const Dance>(dance).dancers : 0;
}

bool dances::AddDancer(Registry& registry, entt::entity dance, entt::entity living, uint32_t sex)
{
	if (!IsDance(registry, dance))
	{
		return false;
	}
	auto& data = registry.Get<Dance>(dance);
	const auto group = dance_rules::AddDancer(data, living, k_LivingDanceType, sex);
	if (!group.has_value())
	{
		return false;
	}
	registry.AssignOrReplace<Dancer>(living, Dancer {.dance = dance, .group = *group});
	return true;
}

void dances::RemoveDancer(Registry& registry, entt::entity living)
{
	const auto* dancer = registry.Valid(living) ? registry.TryGet<const Dancer>(living) : nullptr;
	if (dancer == nullptr)
	{
		return;
	}
	if (IsDance(registry, dancer->dance))
	{
		dance_rules::RemoveDancer(registry.Get<Dance>(dancer->dance), dancer->group, living);
	}
	registry.Remove<Dancer>(living);
}

entt::entity dances::FirstDancer(const Registry& registry, entt::entity dance, entt::entity exclude)
{
	return IsDance(registry, dance) ? dance_rules::FirstDancer(registry.Get<const Dance>(dance).groups, exclude) : entt::null;
}

std::vector<entt::entity> dances::Dancers(const Registry& registry, entt::entity dance)
{
	std::vector<entt::entity> dancers;
	if (IsDance(registry, dance))
	{
		for (const auto& group : registry.Get<const Dance>(dance).groups.all)
		{
			dancers.insert(dancers.end(), group.dancers.begin(), group.dancers.end());
		}
	}
	return dancers;
}

void dances::Destroy(Registry& registry, entt::entity dance, const std::function<void(entt::entity)>& finished)
{
	if (!IsDance(registry, dance))
	{
		return;
	}
	for (const auto dancer : Dancers(registry, dance))
	{
		RemoveDancer(registry, dancer);
		if (finished && registry.Valid(dancer))
		{
			finished(dancer);
		}
	}
	// TODO(opening): what it was danced about is told the dance has gone
	registry.Destroy(dance);
}

void dances::ProcessTurn(Registry& registry, entt::entity entity, const TurnContext& context)
{
	auto& dance = registry.Get<Dance>(entity);
	if (dance.owner != entt::null && !context.available(dance.owner))
	{
		Destroy(registry, entity, context.finished);
		return;
	}
	// TODO(opening): a dance that follows what it is danced about moves with it; the scripts' dances don't
	dance_rules::ProcessTurn(dance, context.turn);
}
