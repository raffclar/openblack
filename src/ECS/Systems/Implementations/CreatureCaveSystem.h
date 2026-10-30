/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Common/GameRandom.h"
#include "Creature/CaveCreature.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"

namespace openblack::ecs::components
{
struct CreatureCopy;
} // namespace openblack::ecs::components

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureCaveSystem final: public CreatureCaveSystemInterface
{
public:
	void Update(std::chrono::duration<float, std::milli> frameTime) override;
	void SetInterface(const TempleScreenInterface* interface) override { _interface = interface; }
	[[nodiscard]] const TempleScreenInterface* GetInterface() const override { return _interface; }
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
	void SetCreatureShown(bool shown) override { _creatureShown = shown; }
	[[nodiscard]] bool IsCreatureShown() const override { return _creatureShown; }
	[[nodiscard]] std::optional<entt::entity> GetCaveCreature() const override { return _caveCreature; }
	[[nodiscard]] std::span<const creature_tattoo_editor::SiteOnScreen> GetTattooSitesOnScreen() const override
	{
		return _tattooSitesOnScreen;
	}
	[[nodiscard]] std::optional<creature_tattoo_editor::Orbit> GetCaveCreatureView() const override;
	[[nodiscard]] bool CursorOverCaveCreature(const screen_pick::View& view) const override;

private:
	/// Makes the copy of the player's creature by the fire as the creature room is first drawn on a visit, and drops it
	/// as the player leaves the temple
	void KeepCaveCreature();
	void MakeCaveCreature(entt::entity creature);
	void DropCaveCreature();
	/// The copy's frame: what it does, its turn, and its pose
	void UpdateCaveCreature(float milliseconds);
	/// While the tattoo editor shows the copy: which tattoo place is under the pointer, as the copy is posed now
	void HoverTattooSites(entt::entity copy, glm::ivec2 pointer);
	/// Starts the copy turning on the spot to face a heading
	static void StartCaveTurn(const components::CreatureCopy& copy, cave_creature::State& state, GameHeading heading);

	const TempleScreenInterface* _interface {nullptr};
	creature_cave::Screen _screen;
	bool _creatureShown {true};
	/// The creature room has been made on this visit to the temple
	bool _caveMade {false};
	std::optional<entt::entity> _caveCreature;
	std::vector<creature_tattoo_editor::SiteOnScreen> _tattooSitesOnScreen;
	/// The game's random numbers. The copy draws its coin tosses and fidgets on the local seed
	GameRandomSource _random;
};

} // namespace openblack::ecs::systems
