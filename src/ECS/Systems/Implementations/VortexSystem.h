/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <entt/signal/sigh.hpp>

#include "3D/MapCoords.h"
#include "Common/FramePace.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "Magic/VortexCrossingFile.h"

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
	void UpdateFrame(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime, bool landDrawn) override;
	void UpdateDrawnFrame(ObjectDrawListSystemInterface& drawList, std::chrono::milliseconds frameRealTime,
	                      std::chrono::milliseconds frameGameTime, bool landDrawn) override;
	[[nodiscard]] std::span<const GroundMark> GetGroundMarks() const override { return _groundMarks; }
	std::optional<Taken> EnterVortex(const std::shared_ptr<particles::CarriedObject>& carried) override;
	void ThrowOutOfVortex(entt::entity object, float angle, float scale) override;
	void StruckBy(entt::entity vortex, entt::entity hitter, const components::DrawnIntoVortex::Flight& flight) override;
	[[nodiscard]] bool IsThrownOutByAVortex(entt::entity thing) const override;
	/// A save keeps a copy of the crossing file while a vortex writes or reads it, and the vortex that writes it
	void SaveState(reflection::BinaryWriter& out) const override;
	/// A load puts the copy back in place of the crossing file, and the vortex that wrote it writes on after its lines
	bool LoadState(reflection::BinaryReader& in) override;

private:
	/// A vortex's effects start where it is made
	static void StartEffects(components::Vortex& vortex);
	/// They go at once with it
	static void DeleteEffects(const components::Vortex& vortex);
	/// A fully open vortex that leads away takes what it reaches this turn
	static void Pull(entt::entity entity, const components::Vortex& vortex);
	/// A thing it takes is handed to its effect, which starts drawing it in when it next steps; a thing taken as it flew
	/// starts as it flew
	static void HandOver(entt::entity vortexEntity, const components::Vortex& vortex, entt::entity thing,
	                     const std::optional<components::DrawnIntoVortex::Flight>& flight = std::nullopt);
	/// A pile it reaches gives up a pot of what it holds, which it takes
	static void TakeFromPile(entt::entity vortexEntity, const components::Vortex& vortex, entt::entity pile);
	/// A thing thrown out at an angle, at a scale of the usual speeds, passing through what threw it
	static void Throw(entt::entity object, float angle, float scale, entt::entity thrower);
	/// A vortex that leads into the land reads the crossing file as it is made
	static void ReadCrossingFile(components::Vortex& vortex);
	/// Each turn it keeps what it threw, holds its fade until it is done, and every second turn throws the next thing out
	static void ThrowOut(entt::entity entity, components::Vortex& vortex);
	/// What comes out joins its town or flock and, but for a dove, is kept and thrown out at the angle
	static void JoinAndThrow(entt::entity entity, components::Vortex& vortex, entt::entity thing, float angle);
	/// As it goes, what it threw out can be destroyed again
	static void LetGoOfThrown(const components::Vortex& vortex);
	/// The nearest town strictly within a reach of a place, by the game's measure of distance
	[[nodiscard]] static entt::entity NearestTownWithin(const map_coords::MapCoords& place, float reach);
	/// A new vortex that leads away empties the crossing file and writes to it until it goes
	void StartCrossingFile(entt::entity vortex);
	/// The line of a thing taken, relative to the vortex
	void WriteCrossing(const components::Vortex& vortex, entt::entity thing);
	/// When the vortex that writes the crossing file goes, the file closes
	void OnVortexGone(entt::registry& registry, entt::entity vortex);
	/// Whatever a vortex was drawing in whose particle has gone, with the vortex or at its middle, goes too
	static void RemoveTheTaken();
	/// A creature it reaches sparkles away once near the middle, if it may be taken
	static void PullCreature(entt::entity entity, const components::Creature& creature, const glm::vec3& position,
	                         const map_coords::MapCoords& centre);
	/// Whether a creature may be taken by a vortex now
	[[nodiscard]] static bool CreatureMayBeTaken(entt::entity entity, const components::Creature& creature);
	/// How near the middle a creature must come to be taken
	[[nodiscard]] static float CreatureTakenDistance();
	/// The land under a vortex is levelled as far as an amount: its heights are read the first time
	static void LevelGround(components::Vortex& vortex, float amount);
	/// A load gives the land back from its file. Each loaded vortex levels the land again as far as it did at the save,
	/// and makes its swirl, its effect over the land and its glow again. It keeps the effect that carries things in.
	static void StartAgainAfterLoad();

	std::vector<GroundMark> _groundMarks;
	/// When the swirls take their next step
	frame_pace::Pace _effectPace;
	/// When the effects over the land of the vortices that the draw list draws take their next step
	frame_pace::Pace _drawnEffectPace;
	/// The crossing file, written by the vortex that last started to lead away while that vortex is in the world
	vortex::CrossingFile _crossing;
	/// Tells the crossing file when a vortex goes, also when the land clears all its things
	entt::scoped_connection _vortexGone;
};

} // namespace openblack::ecs::systems
