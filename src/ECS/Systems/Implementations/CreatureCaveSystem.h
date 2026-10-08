/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <functional>

#include "Creature/CreatureTattoo.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::creature_mind_tables
{
struct Tables;
} // namespace openblack::creature_mind_tables

namespace openblack::ecs::systems
{

class CreatureCaveSystem final: public CreatureCaveSystemInterface
{
public:
	/// What the cave asks of the creature's other services: the mind's tables of actions, skills and miracles, and a
	/// tattoo put in a creature's slot (an empty one takes it off). Without the tables the cave knows the creature's body
	/// and mind but not what it has learnt; without the tattoos it can't change them.
	struct Services
	{
		std::function<const creature_mind_tables::Tables*()> mindTables;
		std::function<void(entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo)> setTattoo;
	};

	explicit CreatureCaveSystem(Services services = {});

	void Update() override;
	void SetInterface(const gui::GameInterface* interface) override { _interface = interface; }
	[[nodiscard]] const gui::GameInterface* GetInterface() const override { return _interface; }
	void Open() override;
	void Close() override;
	[[nodiscard]] bool IsOpen() const override { return _screen.open; }
	[[nodiscard]] bool InTemple() const override;
	bool Escape() override;
	[[nodiscard]] creature_cave::Screen& GetScreen() override { return _screen; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override;
	[[nodiscard]] std::optional<creature_cave::Snapshot> Snapshot() const override;
	bool ApplyTattoo(uint8_t site, uint8_t design, glm::u8vec3 colour) override;
	bool RemoveTattoo(uint8_t site) override;

private:
	/// Puts an edit of the creature's tattoos through the tattoo service, false when there is none to do it
	bool SetTattoo(entt::entity creature, const std::optional<creature_cave::TattooEdit>& edit) const;

	Services _services;
	const gui::GameInterface* _interface {nullptr};
	creature_cave::Screen _screen;
};

} // namespace openblack::ecs::systems
