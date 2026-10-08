/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The CHL spell natives (CHLApi.cpp forwards to them). The arguments are popped in the handlers' order.

#include "CHLSpells.h"

#include <cstdint>

#include <limits>

#include <LHVM.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/MagicTables.h"
#include "Magic/Script/ScriptPlayer.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
constexpr int k_MagicTypes = 42;

glm::vec3 PopPosition(lhvm::LHVM& vm)
{
	const auto z = vm.Popf();
	const auto y = vm.Popf();
	const auto x = vm.Popf();
	return {x, y, z};
}

/// Truncated, then ScriptPlayerToGamePlayer (Magic/Script/ScriptPlayer.h): 0 the neutral player, n game player n - 1;
/// none from 9 on
bool ScriptPlayer(float value, PlayerNames& player)
{
	return magic::ScriptPlayerToGamePlayer(static_cast<int32_t>(value), player); // truncated
}

bool ValidMagic(int32_t magic)
{
	if (magic <= 0 || magic >= k_MagicTypes)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic");
		return false;
	}
	return true;
}

/// The process info of a script cast: hand = from, camera forward = target - from, dir, power 1, 1.0 x curl, enabled
psys::ProcessInfo ScriptInfo(const glm::vec3& position, const glm::vec3& from, const glm::vec3& direction, float curl)
{
	psys::ProcessInfo info {
	    .handPos = from,
	    .cameraForward = position - from,
	    .direction = direction,
	    .power = 1.0f,
	    .curl = 1.0f * curl,
	    .enabled = true,
	};
	return info;
}
} // namespace

namespace openblack::magic::script
{
entt::entity CastSpellAtPos(const glm::vec3& position, MagicType magic, const glm::vec3& from,
                            ecs::components::SpellCreator creator, bool check, float radius, float time, float curl,
                            const glm::vec3& direction)
{
	if (creator.kind == ecs::components::SpellCreator::Kind::None)
	{
		creator = creator::NeutralPlayer();
	}
	// TODO: a WorshipSpellIcon creator still needing chants fails, and after the cast its store loses the seed's cost
	// (GetChantRequiredForSpellSeed / RemoveFromChantStore)
	const auto& tables = Locator::infoConstants::value();
	SpellCastData castData {radius, GetMagicEffectInfo(tables, magic).initialChants, time, -1};
	const auto mapPosition = ToMap(position);
	if (check && !cast_rules::CanCastAt(magic, mapPosition))
	{
		return entt::null;
	}
	entt::entity spell = entt::null;
	CastAtPos(magic, creator, mapPosition, &spell, &castData, ScriptInfo(ToWorld(mapPosition), from, direction, curl));
	return spell;
}

void SpellAtThing()
{
	// curl, duration, radius, from, the thing, magic. An Object -> cast on the object (its availability check off, then
	// CastAtObject); else CastSpellAtPos at its position. Pushes the spell.
	auto& vm = Locator::vm::value();
	const auto curl = vm.Popf();
	const auto duration = vm.Popf();
	const auto radius = vm.Popf();
	const auto from = PopPosition(vm);
	const auto thing = static_cast<entt::entity>(vm.Pop().uintVal);
	const auto magic = vm.Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const bool valid = registry.Valid(thing);
	if (!valid)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid for cast");
	}
	if (!ValidMagic(magic))
	{
		return;
	}
	entt::entity spell = entt::null;
	if (valid)
	{
		const auto type = static_cast<MagicType>(magic);
		const auto& tables = Locator::infoConstants::value();
		SpellCastData castData {radius, GetMagicEffectInfo(tables, type).initialChants, duration, -1};
		const auto* transform = registry.TryGet<const ecs::components::Transform>(thing);
		const auto position = transform != nullptr ? transform->position : glm::vec3(0.0f);
		CastAtObject(type, creator::NeutralPlayer(), thing, &spell, &castData,
		             ScriptInfo(position, from, glm::vec3(0.0f), curl));
	}
	if (spell == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
	}
	vm.Pusho(spell == entt::null ? 0 : static_cast<uint32_t>(spell)); // AddScriptGameThing
}

void SpellAtPos()
{
	// curl, duration, radius, from, position, magic; the neutral player casts it with no CanCast check and no direction
	auto& vm = Locator::vm::value();
	const auto curl = vm.Popf();
	const auto duration = vm.Popf();
	const auto radius = vm.Popf();
	const auto from = PopPosition(vm);
	const auto position = PopPosition(vm);
	const auto magic = vm.Pop().intVal;
	if (!ValidMagic(magic))
	{
		return;
	}
	const auto spell =
	    CastSpellAtPos(position, static_cast<MagicType>(magic), from, {}, false, radius, duration, curl, glm::vec3(0.0f));
	if (spell == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		vm.Pusho(0);
		return;
	}
	vm.Pusho(static_cast<uint32_t>(spell));
}

void SpellAtPoint()
{
	// radius, position, magic. SHIELD / PHYSICAL_SHIELD -> the shield there; else the first available spell of
	// that magic within radius of the position
	auto& vm = Locator::vm::value();
	const auto radius = vm.Popf();
	const auto position = ToMap(PopPosition(vm));
	const auto magic = static_cast<MagicType>(vm.Pop().intVal);
	entt::entity found = entt::null;
	if (magic != MagicType::Shield && magic != MagicType::PhysicalShield)
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto spell : Spells())
		{
			const auto& component = registry.Get<ecs::components::Spell>(spell);
			if (component.magicType == magic &&
			    glm::length(glm::vec2(component.position.x - position.x, component.position.z - position.z)) < radius)
			{
				found = spell;
				break;
			}
		}
	}
	vm.Pusho(found == entt::null ? 0 : static_cast<uint32_t>(found));
}

void SetPlayerMagic()
{
	// player, magic, on -> players::SetMagicTypeEnabled
	auto& vm = Locator::vm::value();
	const auto playerValue = vm.Popf();
	const auto magic = vm.Pop().intVal;
	const auto on = vm.Pop().intVal;
	PlayerNames player {};
	if (ValidMagic(magic) && ScriptPlayer(playerValue, player))
	{
		players::SetMagicTypeEnabled(player, static_cast<MagicType>(magic), on != 0);
	}
}

void HasPlayerMagic()
{
	// player, magic -> HasMagicTypeEverBeenEnabled (1 with no such player; 0 for a bad magic)
	auto& vm = Locator::vm::value();
	const auto playerValue = vm.Popf();
	const auto magic = vm.Pop().intVal;
	if (!ValidMagic(magic))
	{
		vm.Pushb(false);
		return;
	}
	bool result = true;
	PlayerNames player {};
	if (ScriptPlayer(playerValue, player))
	{
		result = players::HasMagicTypeEverBeenEnabled(player, static_cast<MagicType>(magic));
	}
	vm.Pushb(result);
}

void PlayerSpellCastTime()
{
	// (the turn - the last cast's turn) x turn ms x 0.001: seconds since the player's last cast; FLT_MAX without one
	auto& vm = Locator::vm::value();
	PlayerNames player {};
	if (!ScriptPlayer(vm.Popf(), player))
	{
		vm.Pushf(std::numeric_limits<float>::max());
		return;
	}
	const auto& last = players::MagicOf(player).lastCast;
	const auto turns = static_cast<int32_t>(CurrentTurn()) - static_cast<int32_t>(last.turn);
	vm.Pushf(static_cast<float>(turns) * static_cast<float>(k_TurnMs) * 0.001f);
}

void PlayerSpellLastCast()
{
	// the magic type of the player's last cast (0 without a player)
	auto& vm = Locator::vm::value();
	PlayerNames player {};
	if (!ScriptPlayer(vm.Popf(), player))
	{
		vm.Pushi(0);
		return;
	}
	vm.Pushi(static_cast<int32_t>(players::MagicOf(player).lastCast.magicType));
}

void GetLastSpellCastPos()
{
	// the world point of the player's last cast (0, 0, 0 without a player)
	auto& vm = Locator::vm::value();
	PlayerNames player {};
	glm::vec3 point(0.0f);
	if (ScriptPlayer(vm.Popf(), player))
	{
		point = ToWorld(players::MagicOf(player).lastCast.position);
	}
	vm.Pushv(point.x);
	vm.Pushv(point.y);
	vm.Pushv(point.z);
}

void GetManaForSpell()
{
	// GetChantsRequiredToCreate: the magic's costToCreate
	auto& vm = Locator::vm::value();
	const auto magic = vm.Pop().intVal;
	const float mana = magic >= 0 && magic < k_MagicTypes
	                       ? GetChantsRequiredToCreate(Locator::infoConstants::value(), static_cast<MagicType>(magic))
	                       : 0.0f;
	vm.Pushf(mana);
}
} // namespace openblack::magic::script
