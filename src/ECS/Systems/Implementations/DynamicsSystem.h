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
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Common/MachineClock.h"
#include "ECS/DynamicsWorld.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Physics/BodyShapes.h"
#include "Physics/PairRules.h"
#include "Physics/TurnRules.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class LandIslandInterface;
namespace ecs
{
class PhysicsGround;
namespace components
{
struct Transform;
}
} // namespace ecs
} // namespace openblack

namespace openblack::ecs::systems
{

class DynamicsSystem final: public DynamicsSystemInterface
{
public:
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;
	/// The physics in the game's own world
	DynamicsSystem();
	/// The physics in a world of its own, as tests give it
	explicit DynamicsSystem(std::unique_ptr<dynamics::World> world);
	~DynamicsSystem() override;

	void ResetSimulation() override;
	void SetClassHooks(std::unique_ptr<PhysicsClassHooks> hooks) override;
	void ProcessTurn() override;
	void UpdateFrame(float turnFraction) override;
	[[nodiscard]] std::span<const entt::entity> DrawnBodies() const override { return _drawnBodies; }

	PhysicsStarted InitialisePhysics(entt::entity object, const PhysicsStart& start) override;
	PhysicsStarted StartPhysicsAsObject(entt::entity object, const PhysicsStart& start) override;
	PhysicsEntry* AddObject(entt::entity object, const PhysicsStart& start) override;
	PhysicsEntry* AddObstacle(entt::entity object) override;
	[[nodiscard]] PhysicsEntry* Find(entt::entity object) override;
	[[nodiscard]] bool CanBecomePhysicsObject(entt::entity object) const override;
	[[nodiscard]] bool IsFlying(entt::entity object) const override;
	[[nodiscard]] float WeightOf(entt::entity object) const override;
	[[nodiscard]] bool PhysicallyDestroysAbodes(entt::entity object) const override;
	entt::entity RemoveObject(entt::entity object, bool insert, bool endPhysics) override;
	entt::entity EndPhysicsAsObject(entt::entity object, bool insert, bool hasBody) override;
	void RaiseClearOfWhatIsUnder(PhysicsEntry& entry) override;
	void SettleOnLand(PhysicsEntry& entry, bool noPullDown, bool alignToSlope) override;
	FromHandResult LetGoFromHand(entt::entity object, const FromHand& release) override;
	[[nodiscard]] std::optional<hand_grab::throw_lead::LedThrow>
	StartOfThrow(entt::entity object, const std::optional<std::pair<glm::mat3, glm::vec3>>& drawn, bool alignToSlope,
	             glm::vec3 velocity, int leadTurns) override;
	float PushObject(entt::entity object) override;
	[[nodiscard]] const physics::Ground* GetGround() const override;
	[[nodiscard]] std::vector<physics::Ellipsoid> CreatureSkeleton(entt::entity creature) override;

	void RecordHit(entt::entity hit, entt::entity hitter) override;
	[[nodiscard]] entt::entity GetHitObject() const override;
	[[nodiscard]] entt::entity GetObjectWhichHit() const override;

	void ForEachEntry(const std::function<void(const PhysicsEntry&)>& visit) const override;

private:
	[[nodiscard]] Registry& Entities();
	[[nodiscard]] const Registry& Entities() const;
	/// An object the physics may still work on: one that exists
	[[nodiscard]] bool IsAvailable(entt::entity object) const;
	/// The info weight of an object's kind
	[[nodiscard]] float InfoWeight(entt::entity object) const;
	/// The model of a static, from its row of the tables; none for anything else
	[[nodiscard]] std::optional<MeshId> StaticModel(entt::entity object) const;
	/// A toy a player's hand let go may set the player's creature thinking of playing with it, whether it landed or flew
	void ConsiderToyPlay(entt::entity object, const FromHand& release);
	/// The kind of an object as the physics sees it now
	[[nodiscard]] physics_classes::ClassFacts FactsOf(entt::entity object) const;
	/// A body for an object at its place, moving or resting; none for an object without a shape
	std::unique_ptr<physics::Body> MakePieceBody(entt::entity piece, const physics::Material& material);
	/// The body of a thing at its Transform, or at `place` (a turn with no scale, and an origin) when there is one
	[[nodiscard]] std::unique_ptr<physics::Body>
	MakeBody(entt::entity object, const physics_classes::ClassFacts& facts,
	         const std::optional<std::pair<glm::mat3, glm::vec3>>& place = std::nullopt);
	/// The creature's body: an obstacle of its bones' ellipsoids, as it stands posed now
	[[nodiscard]] std::unique_ptr<physics::Body> MakeCreatureBody(entt::entity creature, const physics::Material& material);
	/// The list grows by a few slots when it is full, before a body is added to it
	void MakeRoomForOne();
	/// A resting obstacle for an object a moving body may hit
	PhysicsEntry* AddProxy(entt::entity object);
	/// Takes an entry out of the list, the last moving into its place
	void RemoveAt(size_t index);
	[[nodiscard]] std::optional<size_t> IndexOf(entt::entity object) const;
	/// The land as it is now, none without one
	[[nodiscard]] const PhysicsGround* Land();

	/// Where an object goes as it follows its body: to the body's object origin, to its centre (a villager coming to
	/// rest), or it stays where it is and only turns
	enum class Place : uint8_t
	{
		Origin,
		Centre,
		Kept,
	};
	/// The object takes its body's angles, and its place
	void SyncObject(const PhysicsEntry& entry, Place place);
	/// The entry learns what its object's kind does in the physics
	static void TakeKind(PhysicsEntry& entry, const physics_classes::ClassFacts& facts);

	void BeginTurn();
	void WakeNearMovingBodies();
	void Step(const physics::Ground& ground);
	void EndTurn();
	/// What breaks buildings, come to rest or taken out, is no longer passed through by the buildings it struck
	void ForgetAsBuildingHitter(entt::entity object);
	/// The sound and the dust of a knock
	void AttemptCollisionSound(PhysicsEntry& entry);
	void AddLandingDust(glm::vec3 centre, float radius, uint32_t argb);

private:
	[[nodiscard]] PhysicsClassHooks& Hooks();

	std::unique_ptr<dynamics::World> _world;
	std::unique_ptr<PhysicsClassHooks> _hooks;
	std::unique_ptr<PhysicsGround> _ground;
	const LandIslandInterface* _groundLand {nullptr};
	/// The bodies, in the order the physics goes through them
	std::vector<std::unique_ptr<PhysicsEntry>> _entries;
	/// The moving bodies that the last frame drew, in the order of _entries
	std::vector<entt::entity> _drawnBodies;
	/// The slots the list has room for: it grows only when a body is added to a full list, never while the moving
	/// bodies wake what is near them
	size_t _capacity {0};
	/// While the physics runs its turn, bodies aren't taken out of it from outside
	bool _inTurnUpdate {false};
	physics::turn::SoundPairs _soundPairs;
	/// What decides the pairs of bodies in a step, one for each body, kept so that a step does not ask for new memory
	std::vector<physics::PairMember> _pairMembers;
	/// Each creature model's bones' boxes, by the model, worked out once
	std::unordered_map<entt::id_type, std::vector<physics::shapes::BoneBox>> _creatureBoxes;
	/// The machine's milliseconds as the game reads them, which pick the fly-by whoosh
	std::function<uint64_t()> _ticks {[]() { return static_cast<uint64_t>(machine_clock::Ticks()); }};
	entt::entity _hitObject {entt::null};
	entt::entity _objectWhichHit {entt::null};
};
} // namespace openblack::ecs::systems
