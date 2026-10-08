/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "PhysicsBody.h"

namespace openblack
{
struct GObjectInfo;
}

namespace openblack::ecs::physics
{
/// PhysicsObject: one body of the physics system, thrown or knocked (awake) or a resting obstacle near a moving body
/// (asleep proxy).
struct PhysicsObject
{
	// The bits of `flags`
	static constexpr uint32_t k_Awake = 0x1;
	/// Pushed by a Living (a villager is not raised over it by RaiseUntilNotIntersecting)
	static constexpr uint32_t k_PushedByLiving = 0x2;
	static constexpr uint32_t k_FromHand = 0x4;
	static constexpr uint32_t k_Landed = 0x8;
	static constexpr uint32_t k_NoObjectCollision = 0x10;

	entt::entity entity {entt::null};
	entt::entity thrower {entt::null};
	PhysicsBody body;
	/// The turn-start matrix's rows (right / up / fwd = glm's columns 0 / 1 / 2): a villager's or an animal's end of
	/// physics reads the land type from its right.y and the villager's heading from its up / fwd rows. The current matrix
	/// is the body's. Written with turnStartCentre by SyncTurnStart
	glm::mat3 turnStartRotation {1.0f};
	/// The turn-start matrix's translation (the current one is the centre of mass): the drawing interpolates from this
	/// matrix
	glm::vec3 turnStartCentre {0.0f};
	/// A game turn has started since the body was added: until then it is drawn where it is. Before the first turn the
	/// two matrices are equal anyway: SetUpPos, AdjustToGroundLevel and so RaiseUntilNotIntersecting when it raises the
	/// body all copy the current matrix into the turn-start one (PhysicsObjects::SyncTurnStart)
	bool turnStarted {false};
	uint32_t flags {0};
	bool villager {false};
	/// 1 a villager, 2 a felled tree, 3 a felled tree that has toppled, else 0
	uint8_t kind {0};
	/// The player's hand threw it, directly or through what it hit (inherited by proxies)
	bool byPlayer {false};
	glm::vec3 forceSum {0.0f}; ///< The force of the touched substeps of this turn
	float impact {0.0f};       ///< |forceSum| x 0.05, the mean force of the turn
	PhysicsObject* hitBy {nullptr};
	/// the squared distance to the camera at the last substep (the fly-by whoosh)
	float cameraDistance2 {1e9f};

	/// G of the turn: impact / (mass x g), 1 while lying on the ground
	[[nodiscard]] float GLoad() const { return impact / (body.Mass() * PhysicsBody::k_Gravity); }
};

/// The object classes whose physics hooks (start, impact reaction, end of physics, sinking) another system owns. One
/// class per object (PhysicsObjects::ClassOf).
enum class PhysicsClass : uint8_t
{
	Villager,
	Animal,
	Tree,
	DeadTree,
	Pot,
	Rock,
	Fragment,
	Building, ///< Abode and StoragePit
	Shield,   ///< A physical shield (MapShield)
	/// Creature: never flies, only a resting body thrown objects hit
	Creature,
	Scaffold, ///< Scaffold (components::Scaffold)
	Other,
	_Count
};

/// What the physics knows of a body's impact at the end of a game turn.
struct ImpactInfo
{
	float g {0.0f};                  ///< impact / (mass x g), PhysicsObject::GLoad
	entt::entity hitBy {entt::null}; ///< The body that hit it, or null
	entt::entity thrower {entt::null};
	bool byPlayer {false}; ///< The hand threw it
};

/// The physics system.
class PhysicsObjects
{
public:
	/// A class's physics virtuals, set by the system that owns the class. An empty function keeps the physics' own
	/// code for that class (or nothing, when it has none).
	struct ClassHandlers
	{
		/// The class's part of starting physics: the object flies (FLYING). AddObject calls it once the body is built
		/// (fromHand: AddObject's own flag, the debug throws; (approximate, debug path) for those the original would run the
		/// class's start with no hand, a scaffold's move included, but they come with fromHand). From the hand
		/// (from_hand::InitialisePhysicsFromHand, through InitialisePhysicsOfClass) it comes last, after the whole of the
		/// object's start from the hand (AddObject, AdjustToGroundLevel, RaiseUntilNotIntersecting, the LANDED test,
		/// RemoveObject or the dropped resource and the flying-object reaction) and only when the object did not land: FLYING
		/// is set only when the body is not LANDED and still belongs to this object (a put-down villager or animal never
		/// enters FLYING).
		std::function<void(entt::entity, PhysicsObject&, bool fromHand)> initialisePhysics;
		/// The class's part of the impact reaction. Returns false when the object is gone (consumed, dead, turned into
		/// something else).
		std::function<bool(entt::entity, PhysicsObject&, const ImpactInfo&)> reactToImpact;
		/// The class's part of the end of physics, after the transform is synced and the object's flying-object reactions
		/// are gone. Returns the entity that stays (a tree becomes a DeadTree), or entt::null for none; putting it back in
		/// the map cells stays with the physics, unless callsBackInMap (the class calls BackInMap itself).
		std::function<entt::entity(entt::entity, PhysicsObject&)> endPhysics;
		/// Asked once the body is low in the water and denser than it: true ends its physics.
		std::function<bool(entt::entity, PhysicsObject&)> hasSunk;
		/// Every frame while the object moves (roots follow a tree...).
		std::function<void(entt::entity)> moved;
		/// The landing sound: RemoveObjectWithEndPhysics plays it for the object endPhysics returned, when LANDED and on
		/// land (only trees have one)
		std::function<void(entt::entity)> dropSfx;
		/// Set when the class builds its own body, not from its mesh (a creature: its bounding sphere, mass 1000, not
		/// dynamic, a vertex per skeleton element). Returns false for no body. A class with it set and empty gets none.
		std::function<bool(entt::entity, PhysicsBody&)> setUpBody;
		/// The class's own weight when it overrides it, unclamped (a creature's mass)
		std::function<float(entt::entity)> weight;
		/// The class's start of physics for the call that adds no body (a particle system takes the object; with a body the
		/// physics calls initialisePhysics): when not FLYING, StorePreviousState unless IN_HAND, and SetTopState(FLYING)
		/// must take; a villager drops its resource first. False refuses the take.
		std::function<bool(entt::entity)> initialisePhysicsWithoutBody;
		/// The class's start of physics for a resting proxy a moving body pushed (GameTurnUpdate): true when it started, and
		/// only then is the body woken. Villager: drops its resource, then the Living part (StorePreviousState unless
		/// IN_HAND, SetTopState(FLYING) must take); Animal: the Living part; the object part (CanWakeKnockedProxy) last.
		std::function<bool(entt::entity, PhysicsObject&)> initialisePhysicsKnocked;
		/// The class's part of a throw from the hand, before the object's own: a villager thrown with dontReplant that
		/// belongs to no player or to the hand's player stops being a disciple; the handler tests membership with the hand's
		/// player (PLAYER_ONE).
		std::function<void(entt::entity, bool dontReplant)> throwFromHand;
		/// The class's override of CanBecomeAPhysicsObject (Scaffold); empty: the physics' own answer
		std::function<bool(entt::entity)> canBecomePhysicsObject;
		/// The class's part of the start from the hand, before the object's own (Scaffold: DestroyThingsInWay when its flag
		/// is set)
		std::function<void(entt::entity)> beforeInitialisePhysicsFromHand;
		/// The class's part of the start from the hand, after the object's own returned a body, with its flags (FromHand,
		/// Landed) final (Scaffold: sets a flag when Landed and not dontReplant). Not called when no body was made
		std::function<void(entt::entity, const PhysicsObject&, bool dontReplant)> initialisedPhysicsFromHand;
		/// The class's endPhysics calls BackInMap itself, in the middle of its work, as Villager, Animal and Scaffold do;
		/// false: the physics calls it after endPhysics
		bool callsBackInMap {false};
	};

	/// The class whose handlers an object takes.
	[[nodiscard]] static PhysicsClass ClassOf(entt::entity entity);
	static void SetClassHandlers(PhysicsClass type, ClassHandlers handlers);
	/// The handlers of the object's class (ClassOf); the physics' own dispatch uses the same table.
	[[nodiscard]] static const ClassHandlers& Handlers(entt::entity entity);

	/// Loads Data\PhysicsConstants.txt.
	static void LoadConstants();
	[[nodiscard]] static const PhysicsData& Constants(int type);
	/// The row of the physics constants the object uses
	[[nodiscard]] static int ConstantsType(entt::entity entity);
	/// Moving bodies hit it (it becomes a proxy near them).
	[[nodiscard]] static bool InteractsWithPhysicsObjects(entt::entity entity);
	/// Whether the object can be thrown or knocked into the physics
	[[nodiscard]] static bool CanBecomeAPhysicsObject(entt::entity entity);
	/// A knocked resting proxy is woken only when its start of physics succeeds: it can become a physics object and no
	/// particle system carries it
	[[nodiscard]] static bool CanWakeKnockedProxy(entt::entity entity);
	/// The class's body setup into a body that is not in the list: PhysicsBody::Initialise, the constants, the vertices and
	/// the pose from the object's Transform (the hand's release prediction body)
	[[nodiscard]] static bool SetUpBody(entt::entity entity, PhysicsBody& body, bool dynamic);
	/// The object's weight, at least 0.01
	[[nodiscard]] static float Weight(entt::entity entity);
	/// The object's info (GObjectInfo), or null.
	[[nodiscard]] static const GObjectInfo* ObjectInfo(entt::entity entity);

	/// The object flies from where its transform is.
	static PhysicsObject* AddObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                                entt::entity thrower = entt::null, bool fromHand = false);
	/// The original's angular velocity: in the body's axes and with its sign
	struct BodySpin
	{
		glm::vec3 w {0.0f};
	};
	/// AddObject with the angular velocity as the original takes it (PhysicsBody::SetAngularVelocityFromBody), bit-exact; the
	/// world-omega overload above goes through R^T.
	static PhysicsObject* AddObject(entt::entity entity, glm::vec3 velocity, BodySpin spin, entt::entity thrower = entt::null,
	                                bool fromHand = false);
	/// AddObject for the start from the hand (physics::from_hand): the FromHand flag and the player are set, but the
	/// flying-object reaction (only spread when the object does not land) and the class's initialisePhysics (after the
	/// landing test) are left to the caller.
	static PhysicsObject* AddObjectFromHand(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity);
	/// The class's ClassHandlers::initialisePhysics, if any, for a body AddObjectFromHand built (the caller only calls
	/// it when the object did not land).
	static void InitialisePhysicsOfClass(PhysicsObject& po, bool fromHand);
	/// The class's ClassHandlers::throwFromHand, if any (from_hand::Throw)
	static void ThrowFromHandOfClass(entt::entity entity, bool dontReplant);
	/// The turn-start matrix becomes the current one, as at the end of SetUpPos and AdjustToGroundLevel:
	/// turnStartRotation / turnStartCentre = the body's. Called by Add, AddProxy, BeginTurn, RaiseUntilNotIntersecting
	/// after each raise, and by every caller of PhysicsBody::AdjustToGroundLevel right after it.
	static void SyncTurnStart(PhysicsObject& po);
	/// What a villager drops when released without landing: AddObject with the velocities and no thrower, then with a
	/// body: the angular momentum when given (L in world space), NoObjectCollision, PhysicsBody::AdjustToGroundLevel(false,
	/// true) (SyncTurnStart) and RaiseUntilNotIntersecting.
	static PhysicsObject* AddDroppedObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
	                                       std::optional<glm::vec3> angularMomentum);
	/// The same with the dropped object's w as the original passes it (body axes, its sign: BodySpin); the
	/// angularMomentum, when given, is in this port's sign
	static PhysicsObject* AddDroppedObject(entt::entity entity, glm::vec3 velocity, BodySpin spin,
	                                       std::optional<glm::vec3> angularMomentum);
	/// Takes the object out of the physics without EndPhysics.
	static void RemoveObject(entt::entity entity);
	/// The dropped object's part after the body is added, shared by both AddDroppedObject
	static PhysicsObject* FinishDroppedObject(PhysicsObject& body, std::optional<glm::vec3> angularMomentum);
	/// The object takes the body's pose (angles, position, altitude), its EndPhysics runs, then the landing sound
	/// (ClassHandlers::dropSfx) of the object it returned if LANDED and on land, the flying-object reactions go
	/// (TODO(reactions)) and the body is removed.
	static void RemoveObjectWithEndPhysics(entt::entity entity);
	/// The map part of the end of physics: inside the map the object goes back in the map cells, outside it is deleted.
	/// Villager and Animal call it themselves in the middle of their end of physics. False when deleted.
	static bool BackInMap(entt::entity entity);
	/// Resting bodies are made for the objects of the map cells under the body's square (C +- R) that
	/// InteractsWithPhysicsObjects, then the body goes up by the larger penetration of both ways until no overlapping
	/// body pushes it more than 0.001. Each raise goes through PhysicsBody::SetUpPos, so the turn-start matrix follows
	/// (SyncTurnStart).
	static void RaiseUntilNotIntersecting(PhysicsObject& po);
	[[nodiscard]] static PhysicsObject* Find(entt::entity entity);
	/// Is the object flying (in physics and not a resting proxy)?
	[[nodiscard]] static bool IsFlying(entt::entity entity);
	/// A Living pushes the object (a villager clearing its way): its body (the one in the physics, else a new one when
	/// no particle system carries it) gets PushedByLiving and external force += F d / |d|, d = T - the object's
	/// MapCoords point, F = weight x 9.81 (the pusher is not read). Returns 0.0 (the only caller drops it).
	static float PushObject(entt::entity object, entt::entity pusher);
	/// The object's rotation from a body's (or an atom's) rows, as the original sets its angles from them: a Villager,
	/// Animal, Tree or Feature takes only the Y angle and stands upright; a MobileStatic (DeadTree, Fragment, the
	/// statics) or MobileObject (Pot, Scaffold, OneOffSpellSeed) takes RotationYXZ(y, x, z); anything else keeps the rows
	/// (pending: Football and the Ball have no component yet).
	[[nodiscard]] static glm::mat3 RotationFromRows(entt::entity entity, const glm::mat3& rows);
	/// The draw's quarter turn of an animated mesh: a Villager's or an Animal's drawn rotation (openblack's Transform)
	/// from the original's rows (their bodies are in the original's axes).
	[[nodiscard]] static glm::mat3 DrawQuarterTurn(const glm::mat3& rows);
	/// From AddObject and the start from the hand (physics::from_hand): every creature that can catch the object starts
	/// catching it. The creature's part is the hook.
	static void CheckAllCreaturesForCatching(entt::entity object, PhysicsObject& po);
	/// The creature's part of CheckAllCreaturesForCatching (pending: the creature walk, the eligibility, 0.8 x weight,
	/// CanBePickedUp, the reach test and the catch plan)
	static void SetCreatureCatchHook(std::function<void(entt::entity object, PhysicsObject& po)> hook);

	/// Once a game turn, unpaused: the turn's start, its 20 substeps of 0.005 s in one go and its end (impacts, sounds,
	/// reactions).
	static void GameTurnUpdate();
	/// Every frame: the dust, and the pose each moving body is drawn at, between the start and the end of the last turn
	/// by the turn fraction (components::PhysicsDrawPose).
	static void UpdateFrame(float turnFraction, float seconds);
	static void Clear();

	/// The hand starts a release prediction: the old prediction ends and the new one is active with the held object
	static void BeginPrediction(entt::entity object);
	/// The prediction body and its history from from_hand::PredictRelease, for the drawing; the turn counter restarts
	/// at 0
	static void SetPrediction(entt::entity object, PhysicsBody body, std::vector<std::pair<glm::mat3, glm::vec3>> history);
	/// When the prediction is this object's, it ends (the start from the hand).
	static void EndPredictionOf(entt::entity object);
	/// The prediction is active and its object is this one: the hand skips drawing the held object then (its
	/// PhysicsDrawPose is the prediction's)
	[[nodiscard]] static bool IsPredictionObject(entt::entity object);
	/// Every physics object, read only (the renderer's shadows)
	static void ForEach(const std::function<void(const PhysicsObject&)>& func);

	PhysicsObjects() = delete;
};
} // namespace openblack::ecs::physics
