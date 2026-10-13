/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <fstream>
#include <memory>
#include <optional>
#include <vector>

#include "3D/MapCoords.h"
#include "ECS/Systems/VortexSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct Creature;
struct Vortex;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class VortexSystem final: public VortexSystemInterface
{
public:
	entt::entity Create(glm::vec3 position, VortexType type, float altitude) override;
	bool StartFadeOut(entt::entity vortex) override;
	bool SetArrival(entt::entity vortex, const components::Vortex::Arrival& arrival) override;
	[[nodiscard]] float GetOpenness(entt::entity vortex) const override;
	void ProcessTurn() override;
	void UpdateFrame(float gameSeconds) override;
	[[nodiscard]] std::span<const GroundMark> GetGroundMarks() const override { return _groundMarks; }
	std::optional<Taken> EnterVortex(const std::shared_ptr<particles::CarriedObject>& carried) override;
	void ThrowOutOfVortex(entt::entity object, float angle, float scale) override;

private:
	/// A vortex's effects start where it is made
	static void StartEffects(components::Vortex& vortex);
	/// They go at once with it
	static void DeleteEffects(const components::Vortex& vortex);
	/// A fully open vortex that leads away takes what it reaches this turn
	static void Pull(entt::entity entity, const components::Vortex& vortex);
	/// A thing it takes is handed to its effect, which starts drawing it in when it next steps
	static void HandOver(entt::entity vortexEntity, const components::Vortex& vortex, entt::entity thing);
	/// A pile it reaches gives up a pot of what it holds, which it takes
	static void TakeFromPile(entt::entity vortexEntity, const components::Vortex& vortex, entt::entity pile);
	/// A new vortex that leads away empties the crossing file and writes to it until it goes
	void StartCrossingFile(entt::entity vortex);
	/// The line of a thing taken, relative to the vortex
	void WriteCrossing(const components::Vortex& vortex, entt::entity thing);
	/// Whatever a vortex was drawing in whose particle has gone, with the vortex or at its middle, goes too
	static void RemoveTheTaken();
	/// A creature it reaches sparkles away once near the middle, if it may be taken
	static void PullCreature(entt::entity entity, const components::Creature& creature, const glm::vec3& position,
	                         const map_coords::MapCoords& centre);
	/// How near the middle a creature must come to be taken
	[[nodiscard]] static float CreatureTakenDistance();
	/// The land under a vortex is levelled as far as an amount: its heights are read the first time
	static void LevelGround(components::Vortex& vortex, float amount);

	std::vector<GroundMark> _groundMarks;
	/// The crossing file, written by the vortex that last started leading away while it lasts
	std::ofstream _crossing;
	entt::entity _crossingVortex {entt::null};
};

} // namespace openblack::ecs::systems
