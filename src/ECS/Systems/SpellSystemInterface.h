/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Spell.h"

namespace openblack::psys
{
class SpellSink;
} // namespace openblack::psys

namespace openblack::magic
{
struct SpellOps;
class SpellGrid;
} // namespace openblack::magic

namespace openblack::ecs::systems
{
/// The live spells, the sinks their particle effects talk to, each spell class's operations, the spell presence
/// grid and the shield and storm classes' own lists (magic's Spell code goes through it).
/// A sink is owned here, not by the spell entity: the particle system keeps a plain pointer to it, which must stay
/// valid until the spell's deletion erases it.
class SpellSystemInterface
{
public:
	virtual ~SpellSystemInterface() = default;

	/// Every spell, the newest first
	[[nodiscard]] virtual std::vector<entt::entity>& Spells() = 0;
	/// The spell's sink, replacing the one it had
	virtual void SetSink(entt::entity spell, std::unique_ptr<psys::SpellSink> sink) = 0;
	virtual void EraseSink(entt::entity spell) = 0;
	/// A land is loaded: no spells and no sinks (the entities go with the registry). The operations stay.
	virtual void Clear() = 0;

	/// The class's operations; empty until the class registers them
	[[nodiscard]] virtual magic::SpellOps& Ops(components::SpellClass spellClass) = 0;
	/// True only on the first call: the caller then registers every spell class
	[[nodiscard]] virtual bool TakeOpsRegistration() = 0;
	/// True only on the first call for a class: the warning that the class runs as a plain spell is logged once
	[[nodiscard]] virtual bool TakeNotPortedWarning(components::SpellClass spellClass) = 0;

	/// Where spells were this turn; Clear leaves it (a land load clears it on its own)
	[[nodiscard]] virtual magic::SpellGrid& Grid() = 0;

	/// The shield spells and the storm spells, each newest first (by its class's first use of it); each class clears its
	/// own on a land load
	[[nodiscard]] virtual std::vector<entt::entity>& ShieldSpells() = 0;
	[[nodiscard]] virtual std::vector<entt::entity>& StormSpells() = 0;
};
} // namespace openblack::ecs::systems
