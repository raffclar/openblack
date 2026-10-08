/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <unordered_map>

#include "ECS/Systems/SpellSystemInterface.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellGrid.h"
#include "Particles/SpellLink.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class SpellSystem final: public SpellSystemInterface
{
public:
	[[nodiscard]] std::vector<entt::entity>& Spells() override;
	void SetSink(entt::entity spell, std::unique_ptr<psys::SpellSink> sink) override;
	void EraseSink(entt::entity spell) override;
	void Clear() override;

	[[nodiscard]] magic::SpellOps& Ops(components::SpellClass spellClass) override;
	[[nodiscard]] bool TakeOpsRegistration() override;
	[[nodiscard]] bool TakeNotPortedWarning(components::SpellClass spellClass) override;
	[[nodiscard]] magic::SpellGrid& Grid() override;
	[[nodiscard]] std::vector<entt::entity>& ShieldSpells() override;
	[[nodiscard]] std::vector<entt::entity>& StormSpells() override;

private:
	static constexpr size_t k_Classes = static_cast<size_t>(components::SpellClass::_COUNT);

	std::vector<entt::entity> _spells;
	// A map here, not a component on the spell: each sink is destroyed when the system drops it, not when the entity
	// goes. Each sink on the heap, so a rehash does not move it
	std::unordered_map<entt::entity, std::unique_ptr<psys::SpellSink>> _sinks;
	std::array<magic::SpellOps, k_Classes> _ops {};
	bool _opsRegistered {false};
	std::array<bool, k_Classes> _notPortedWarned {};
	magic::SpellGrid _grid;
	std::vector<entt::entity> _shieldSpells;
	std::vector<entt::entity> _stormSpells;
};
} // namespace openblack::ecs::systems
