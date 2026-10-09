/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureCaveSystem.h"

#include <algorithm>

#include "3D/CreatureBody.h"
#include "3D/CreatureCaveTrophies.h"
#include "3D/TempleInteriorInterface.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureWatching.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

namespace openblack::ecs::systems
{

using namespace components;

namespace
{
/// Whether the game has a text for a creature having learnt a miracle, by the miracle's place in its table; the creature
/// room's scroll and plinths leave out those it has none for
bool HasLearntText(size_t miracle)
{
	if (!Locator::infoConstants::has_value())
	{
		return true;
	}
	const auto& info = Locator::infoConstants::value();
	return miracle < info.magicEffect.size() &&
	       static_cast<int>(magic::GetMagicEffectInfo(info, static_cast<MagicType>(miracle)).creatureLearntEnum) != 0;
}
} // namespace

bool CreatureCaveSystem::InTemple() const
{
	return Locator::temple::has_value();
}

void CreatureCaveSystem::Update()
{
	if (InTemple())
	{
		// The temple's creature room is the cave: F5 takes the player there, and Escape back out
		const auto& temple = Locator::temple::value();
		_screen.open = temple.Active() && temple.GetCurrentRoom() == TempleRoom::CreatureCave;
		return;
	}
	// Without a temple to go into, F5 shows the cave on its own and hides it again
	if (Locator::gameActionSystem::has_value())
	{
		using input::BindableActionMap;
		const auto& actions = Locator::gameActionSystem::value();
		if (actions.Get(BindableActionMap::ZOOM_TO_CREATURE_ROOM) &&
		    actions.GetChanged(BindableActionMap::ZOOM_TO_CREATURE_ROOM))
		{
			if (_screen.open)
			{
				Close();
			}
			else
			{
				Open();
			}
		}
	}
}

void CreatureCaveSystem::Open()
{
	if (InTemple())
	{
		auto& temple = Locator::temple::value();
		if (temple.Active())
		{
			temple.GoToRoom(TempleRoom::CreatureCave);
		}
		else
		{
			temple.Activate(TempleRoom::CreatureCave);
		}
		return;
	}
	// Going into the cave lets go of the creature the camera followed
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	_screen.open = true;
}

void CreatureCaveSystem::Close()
{
	if (InTemple())
	{
		if (Locator::temple::value().Active())
		{
			Locator::temple::value().RequestLeave();
		}
		return;
	}
	_screen.open = false;
}

bool CreatureCaveSystem::Escape()
{
	// In the temple, its own Escape leads the way out
	if (InTemple() || !_screen.open)
	{
		return false;
	}
	Close();
	return true;
}

std::optional<entt::entity> CreatureCaveSystem::GetCreature() const
{
	// The cave is the player's own creature's, whichever creature the camera follows
	return Locator::creatureModeSystem::has_value() ? Locator::creatureModeSystem::value().PlayersCreature() : std::nullopt;
}

std::optional<creature_cave::Snapshot> CreatureCaveSystem::Snapshot() const
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(*creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	creature_cave::Snapshot snapshot;
	snapshot.name = creature_fight_hud::SpeciesName(body->species);
	snapshot.strength = body->strength;
	snapshot.fatness = body->fatness;
	snapshot.alignment = body->alignment;
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(*creature))
	{
		snapshot.needs = needs->needs;
	}
	if (const auto* record = registry.TryGet<const CreatureFightRecord>(*creature))
	{
		snapshot.fightBalance = record->tendency;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(*creature);
	if (mind == nullptr)
	{
		return snapshot;
	}
	snapshot.attitudeToPlayer = mind->attitudeToPlayer;
	// Whether it thinks it knows what its god wants most
	// TODO(raffclar): the game forgets what it found each time it writes the scroll's text; this is looked at every frame,
	// so it only looks
	auto perceived = mind->perceivedDesires;
	snapshot.knowsGodsDesire = creature_perceived_desires::TakeDominant(perceived, [mind](size_t desire) {
		                           return mind->desires.has_value() && desire < mind->desires->desires.size() &&
		                                  mind->desires->desires.at(desire).activated;
	                           }).has_value();
	snapshot.secondsAlone = mind->secondsAlone;
	const auto* tables = Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	if (mind->learnt.has_value())
	{
		const auto& learnt = *mind->learnt;
		if (!learnt.name.empty())
		{
			snapshot.name = learnt.name;
		}
		snapshot.creaturesKnown = learnt.creatures.size();
		if (mind->desires.has_value())
		{
			for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
			{
				const auto& state = mind->desires->desires.at(d);
				if (state.activated)
				{
					const auto desire = static_cast<creature_desires::Desire>(d);
					snapshot.likes.push_back(
					    {.desire = desire,
					     .level = creature_learning::LikesLevel(desire, state, learnt.initialThresholds.at(d))});
				}
			}
		}
		for (size_t a = 0; a < learnt.opinions.size(); ++a)
		{
			const auto name = tables != nullptr && a < tables->actions.size() ? tables->actions[a].name : std::to_string(a);
			snapshot.opinions.push_back({.action = name, .opinion = learnt.opinions[a]});
		}
		if (tables != nullptr)
		{
			const auto& knowledge = learnt.knowledge;
			for (size_t i = 0; i < tables->skills.size(); ++i)
			{
				const bool known = i < knowledge.skillsKnown.size() && knowledge.skillsKnown[i];
				snapshot.skills.push_back({.name = tables->skills[i].name, .known = known});
			}
			const auto multiplier = creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species));
			for (size_t i = 0; i < tables->miracles.size(); ++i)
			{
				const auto& rule = tables->miracles[i];
				// How far it has learnt a miracle is the times it has seen it over the times it must, whether it
				// has learnt it or not
				const auto needed = creature_watching::TimesNeeded(rule.timesToSee, multiplier);
				const auto seen = i < knowledge.miraclesSeen.size() ? knowledge.miraclesSeen[i].count : 0u;
				snapshot.miracles.push_back({
				    .name = rule.name,
				    .knownAbout = i < knowledge.miraclesKnown.size() && knowledge.miraclesKnown[i],
				    .hasLearntText = HasLearntText(i),
				    .learnt = CreatureCaveTrophies::PercentLearnt(seen, needed),
				});
			}
		}
	}
	return snapshot;
}

bool CreatureCaveSystem::ApplyTattoo(uint8_t site, uint8_t design, glm::u8vec3 colour)
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !Locator::creatureSkinSystem::has_value())
	{
		return false;
	}
	const auto* tattoos = Locator::entitiesRegistry::value().TryGet<const CreatureTattoos>(*creature);
	if (tattoos == nullptr)
	{
		return false;
	}
	const auto edit = creature_cave::Apply(tattoos->slots, site, design, colour);
	if (!edit.has_value())
	{
		return false;
	}
	Locator::creatureSkinSystem::value().SetTattoo(*creature, edit->slot, edit->tattoo);
	return true;
}

bool CreatureCaveSystem::RemoveTattoo(uint8_t site)
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !Locator::creatureSkinSystem::has_value())
	{
		return false;
	}
	const auto* tattoos = Locator::entitiesRegistry::value().TryGet<const CreatureTattoos>(*creature);
	if (tattoos == nullptr)
	{
		return false;
	}
	const auto edit = creature_cave::Remove(tattoos->slots, site);
	if (!edit.has_value())
	{
		return false;
	}
	Locator::creatureSkinSystem::value().SetTattoo(*creature, edit->slot, edit->tattoo);
	return true;
}

} // namespace openblack::ecs::systems
