/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The links between a miracle and its particle effect. The effect tells the miracle about events and asks it for its
// data.

#define LOCATOR_IMPLEMENTATIONS

#include "MagicSpellLinks.h"

#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

MagicSystem::SpellLink::SpellLink(MagicSystem& system, entt::entity spell)
    : _system(system)
    , _spell(spell)
{
}

bool MagicSystem::SpellLink::SpellEvent(const particles::SpellEventInfo& event)
{
	return _system.OnSpellEvent(_spell, event);
}

int MagicSystem::SpellLink::PowerUpLevel() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	return spell != nullptr ? magic::GetPowerUpLevel(Locator::infoConstants::value(), spell->magicType)
	                        : magic::k_BasePowerUpLevel;
}

bool MagicSystem::SpellLink::IsMyInterfaceCasting() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	return spell != nullptr && spell->fromLocalHand;
}

bool MagicSystem::SpellLink::IsHumanPlayerCasting() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	return spell != nullptr && spell->humanCasting;
}

bool MagicSystem::SpellLink::IsCreatureCasting() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	return spell != nullptr && spell->caster.kind == components::SpellCaster::Kind::Creature;
}

bool MagicSystem::SpellLink::IsScriptCasting() const
{
	// The neutral player casts by script
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	return spell != nullptr && spell->caster.kind == components::SpellCaster::Kind::Player &&
	       spell->caster.player == PlayerNames::NEUTRAL;
}

int MagicSystem::SpellLink::CreatureSpellKind() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	const auto* info = spell != nullptr
	                       ? magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(Locator::infoConstants::value(), spell->magicType)
	                       : nullptr;
	return info != nullptr ? static_cast<int>(info->creatureReceiveSpellType) : -1;
}

entt::entity MagicSystem::SpellLink::Spell() const
{
	return _spell;
}

float MagicSystem::SpellLink::TribalPower() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	return spell != nullptr ? _system.TribalPower(*spell) : 1.0f;
}

std::optional<float> MagicSystem::SpellLink::RainAmount() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	if (spell == nullptr)
	{
		return std::nullopt;
	}
	const auto* storm = magic::GetMagicInfoAs<GMagicStormAndTornadoInfo>(Locator::infoConstants::value(), spell->magicType);
	return storm != nullptr ? std::optional(storm->rainAmount) : std::nullopt;
}

std::optional<float> MagicSystem::SpellLink::MaxDirectionChange() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	const auto* info = spell != nullptr
	                       ? magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(Locator::infoConstants::value(), spell->magicType)
	                       : nullptr;
	return info != nullptr ? std::optional(info->maxDirnChangeWhenCtrCasting) : std::nullopt;
}

void MagicSystem::SpellLink::CloseDown()
{
	_system.CloseDown(_spell);
}

void MagicSystem::SpellLink::MoveTo(glm::vec3 position)
{
	if (auto* spell = EntityRegistry().TryGet<components::Spell>(_spell))
	{
		spell->position = position;
	}
}

float MagicSystem::SpellLink::EffectRadius() const
{
	const auto* spell = EntityRegistry().TryGet<const components::Spell>(_spell);
	return spell != nullptr ? magic::GetMagicEffectInfo(Locator::infoConstants::value(), spell->magicType).radius
	                        : particles::SpellSink::EffectRadius();
}

MagicSystem::HandEffectLink::HandEffectLink(const MagicSystem& system)
    : _system(system)
{
}

bool MagicSystem::HandEffectLink::SpellEvent(const particles::SpellEventInfo& /*event*/)
{
	return false;
}

int MagicSystem::HandEffectLink::PowerUpLevel() const
{
	if (const auto held = _system.GetHeldSeed())
	{
		if (const auto* seed = EntityRegistry().TryGet<const SpellSeed>(*held))
		{
			return seed->powerUp;
		}
	}
	return magic::k_BasePowerUpLevel;
}

bool MagicSystem::HandEffectLink::IsMyInterfaceCasting() const
{
	return true;
}

bool MagicSystem::HandEffectLink::IsHumanPlayerCasting() const
{
	return true;
}
