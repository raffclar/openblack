/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "MagicSystem.h"
#include "Particles/ParticleSpellLink.h"

// The links between a miracle and its effect

namespace openblack::ecs::systems
{

/// What a miracle's particle effect tells it, and asks of it
class MagicSystem::SpellLink final: public particles::SpellSink
{
public:
	SpellLink(MagicSystem& system, entt::entity spell);
	bool SpellEvent(const particles::SpellEventInfo& event) override;
	[[nodiscard]] int PowerUpLevel() const override;
	[[nodiscard]] bool IsMyInterfaceCasting() const override;
	[[nodiscard]] bool IsHumanPlayerCasting() const override;
	[[nodiscard]] bool IsCreatureCasting() const override;
	[[nodiscard]] bool IsScriptCasting() const override;
	[[nodiscard]] int CreatureSpellKind() const override;
	[[nodiscard]] entt::entity Spell() const override;
	[[nodiscard]] float TribalPower() const override;
	[[nodiscard]] std::optional<float> RainAmount() const override;
	[[nodiscard]] std::optional<float> MaxDirectionChange() const override;
	void CloseDown() override;
	void MoveTo(glm::vec3 position) override;
	[[nodiscard]] float EffectRadius() const override;

private:
	MagicSystem& _system;
	entt::entity _spell;
};

/// The held miracle's in-hand effect: this computer's player holds it, and it sends nothing back
class MagicSystem::HandEffectLink final: public particles::SpellSink
{
public:
	explicit HandEffectLink(const MagicSystem& system);
	bool SpellEvent(const particles::SpellEventInfo& event) override;
	[[nodiscard]] int PowerUpLevel() const override;
	[[nodiscard]] bool IsMyInterfaceCasting() const override;
	[[nodiscard]] bool IsHumanPlayerCasting() const override;

private:
	const MagicSystem& _system;
};

} // namespace openblack::ecs::systems
