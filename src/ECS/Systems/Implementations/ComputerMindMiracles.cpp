/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include "ComputerMindCommon.h"
#include "ComputerMindRuleTable.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;
using computer_hand::Argument;
using computer_hand::StepKind;
using computer_mind::MindDomain;
using computer_mind::NodeKind;

namespace
{

/// The worship site's icon that the god wants to charge most is the node's object. How much it wants it.
float ChargeUpSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	std::vector<entt::entity> icons;
	std::vector<computer_mind_rules::ChargeCandidate> candidates;
	for (const auto site : SitesOf(context.player))
	{
		if (site == entt::null)
		{
			continue;
		}
		if (BuiltWithDancers(site))
		{
			for (const auto icon : IconsOf(site))
			{
				const auto& component = registry.Get<const WorshipIcon>(icon);
				icons.push_back(icon);
				candidates.push_back(
				    {.charging = component.charging,
				     .store = component.store,
				     .requests = context.state.mind.castRequested.at(static_cast<size_t>(PlainType(component.seed)))});
			}
		}
		else if (!Locator::worshipSiteSystem::value().IsBuilt(site) && Repaired(site))
		{
			// The god finishes a site that is whole at once
			Locator::worshipSiteSystem::value().BuildBy(site, 1.0f);
		}
	}
	const auto choice = computer_mind_rules::ChooseCharge(candidates);
	if (!choice.has_value())
	{
		return 0.0f;
	}
	mind.nodes[node].object = icons[choice->index];
	return choice->value;
}

/// The god's worshippers chant for the miracle of the node's icon
bool PerformCharge(RuleContext& context, computer_mind::Mind& mind, uint16_t node)
{
	auto& registry = Entities();
	entt::entity icon = mind.nodes[node].object;
	for (auto at = mind.nodes[node].parent; icon == entt::null && at.has_value(); at = mind.nodes[*at].parent)
	{
		icon = mind.nodes[*at].object;
	}
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<WorshipIcon>(icon))
	{
		return false;
	}
	const auto type = PlainType(registry.Get<const WorshipIcon>(icon).seed);
	ReachIcon(context, icon, type);
	computer_hand::Add(context.state.hand, StepKind::MakeWorshippersChantForSpellNoWait,
	                   {Argument {.number = static_cast<int32_t>(type)}, Argument {.object = icon}});
	return true;
}

} // namespace

void computer_world::RegisterMiraclesRules(RuleTable& table)
{
	AddRule(table, MindDomain::Miracles, NodeKind::ActionChargeUpSpell, {.intensity = ChargeUpSpell, .perform = PerformCharge});
}
