/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Animals/FishFarmRules.h"
#include "Commands/PlayerCommands.h"
#include "Creature/CreatureGiving.h"
#include "ECS/AltarOffering.h"
#include "Enums.h"
#include "HandGrabRules.h"
#include "ThrowLead.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{
struct FromHand;
struct FromHandResult;
} // namespace openblack::ecs::systems

namespace openblack::hand_grab
{

/// What the god hand needs from the world to pick things up, hold them and let them go: what is under the cursor, the
/// facts of things the tables and models hold, and the other systems a pick-up or a release reaches. The game's world
/// answers from the game; tests answer with fakes.
class HandGrabWorldInterface
{
public:
	/// While it lives, the world acts with the hand of a player (see SetActingPlayer)
	class ActingPlayer
	{
	public:
		ActingPlayer(HandGrabWorldInterface& world, std::optional<PlayerNames> player)
		    : _world(world)
		    , _before(world.GetActingPlayer())
		{
			if (player.has_value())
			{
				_world.SetActingPlayer(player);
			}
		}
		~ActingPlayer() { _world.SetActingPlayer(_before); }
		ActingPlayer(const ActingPlayer&) = delete;
		ActingPlayer& operator=(const ActingPlayer&) = delete;
		ActingPlayer(ActingPlayer&&) = delete;
		ActingPlayer& operator=(ActingPlayer&&) = delete;

	private:
		HandGrabWorldInterface& _world;
		std::optional<PlayerNames> _before;
	};

public:
	virtual ~HandGrabWorldInterface() = default;

	/// The things, and the hand's own state, which is kept on the hand
	[[nodiscard]] virtual ecs::Registry& Entities() = 0;
	[[nodiscard]] virtual entt::entity Hand() const = 0;
	/// The player whose hand it is
	[[nodiscard]] virtual PlayerNames HandPlayer() const = 0;
	/// A command of a turn acts with the hand of its player: Hand and HandPlayer give that hand until the command ends.
	/// With none, they give the hand of the first player, which the turns read on every computer.
	virtual void SetActingPlayer([[maybe_unused]] std::optional<PlayerNames> player) {}
	[[nodiscard]] virtual std::optional<PlayerNames> GetActingPlayer() const { return std::nullopt; }
	/// The player of the hand that this computer moves with its pointer, when it is not the first player: the input and
	/// the frames of this computer act with that hand
	[[nodiscard]] virtual std::optional<PlayerNames> OwnHandPlayer() const { return std::nullopt; }

	/// The thing the interface picks under the cursor, none over the land or nothing
	[[nodiscard]] virtual std::optional<entt::entity> ObjectUnderCursor() const = 0;
	/// With nothing under the cursor, the thing standing nearest the land behind the hand that the hand reaches for (see
	/// hand_reach), none when nothing is near enough
	[[nodiscard]] virtual std::optional<entt::entity> ObjectNearHand() const = 0;
	/// The point the interface picks under the cursor: the position of the thing picked, else the land or sea under the
	/// cursor; none with neither under it. It is where the hand's influence is asked for what it does.
	[[nodiscard]] virtual std::optional<glm::vec3> PickedPoint() const = 0;
	/// Whether a point is in a player's influence
	[[nodiscard]] virtual bool InInfluence(PlayerNames player, glm::vec3 point) const = 0;
	/// Whether the hand, at its point, is in its player's influence: while it keeps any of it past the border it is
	[[nodiscard]] virtual bool HandInInfluence(PlayerNames player, glm::vec3 hand) const { return InInfluence(player, hand); }
	/// The hand let go of what it holds onto the land: what it keeps of its player's influence past the border wanes, or
	/// comes back, as for one more turn
	virtual void HeldThingUsedOnLand(PlayerNames /*player*/) {}
	/// Whether a point is on the map
	[[nodiscard]] virtual bool InBounds(glm::vec3 point) const = 0;
	/// The flat normal of the land at a point
	[[nodiscard]] virtual glm::vec3 LandNormalAt(glm::vec3 point) const = 0;

	/// How big a thing stands at its scale
	struct Size
	{
		float radius {0.0f};
		float height {0.0f};
	};
	[[nodiscard]] virtual Size SizeOf(entt::entity object) const = 0;
	/// Its weight: its kind's weight at its scale
	[[nodiscard]] virtual float WeightOf(entt::entity object) const = 0;
	[[nodiscard]] virtual float LifeOf(entt::entity object) const = 0;
	/// Whether it flies in the physics
	[[nodiscard]] virtual bool IsFlying(entt::entity object) const = 0;
	/// Whether its kind of animal may be picked up by a player
	[[nodiscard]] virtual bool SpeciesAllowsPickUp(entt::entity animal) const = 0;
	/// A static's kind of thing (rock, toy, ...) and its model, from its row of the tables
	[[nodiscard]] virtual MobileStaticInfo StaticKindOf(MobileStaticInfo type) const = 0;
	[[nodiscard]] virtual MeshId StaticMeshOf(MobileStaticInfo type) const = 0;
	/// Whether a pot of its kind is a loose pot, rather than a pile or a handful
	[[nodiscard]] virtual bool IsLoosePot(PotInfo type) const = 0;
	/// Whether it is made of the physics' rock material, which doesn't lean or stretch as it is pulled
	[[nodiscard]] virtual bool IsOfRockMaterial(entt::entity object) const = 0;
	/// Whether a villager is old enough to make love and not yet too old
	[[nodiscard]] virtual bool IsSexuallyActive(entt::entity villager) const = 0;
	/// The player a thing belongs to, none for a thing of nobody's
	[[nodiscard]] virtual std::optional<PlayerNames> PlayerOf(entt::entity object) const = 0;

	/// Taking a thing: it leaves the physics without landing and the map's cells
	virtual void LeavePhysicsAndMap(entt::entity object) = 0;
	/// The people and animals about react to something
	struct ReactionRequest
	{
		entt::entity initiator {entt::null};
		Reaction type {Reaction::None};
		PlayerNames player {PlayerNames::NEUTRAL};
		glm::vec3 position {0.0f};
	};
	virtual void CreateReaction(const ReactionRequest& request) = 0;
	virtual void RemoveReactions(entt::entity initiator, Reaction type) = 0;
	/// A burning thing starts moving, into a hand (or it is a villager) or not
	virtual void FireStartedMoving(entt::entity object, bool inHand) = 0;
	/// A thing goes into the hand from where it stood: a firefly hiding exactly there is caught, for its reward
	virtual void CatchFirefly(entt::entity object) = 0;
	/// The help counts an event of the hand of the local player (see help::profile)
	virtual void CountHelpEvent(uint32_t event) = 0;
	/// The hand of the local player takes a thing or lets go of it: the hand forgets the path of the gesture it draws
	virtual void ForgetGesturePath() {}
	/// A held thing catches from the fires in its cell
	virtual void HeatHeld(entt::entity object) = 0;
	/// A sound of the in-game bank where something is
	virtual void PlaySample(uint32_t sample, glm::vec3 position) = 0;
	/// The hand taps a thing at a point, for a player, as a rock breaks when tapped; whether the thing took the tap
	virtual bool TapThing(entt::entity object, glm::vec3 handPoint, PlayerNames player) = 0;
	/// Whether what the hand holds can be given to a thing: none when the thing isn't a creature
	[[nodiscard]] virtual std::optional<creature_giving::Verdict> GiveVerdict(entt::entity /*target*/,
	                                                                          entt::entity /*held*/) const
	{
		return std::nullopt;
	}
	/// The creature's help says it couldn't pick up what it is offered
	virtual void SayCantPickUp(entt::entity /*creature*/) {}
	/// The creature is given what the hand holds: it turns to the player and reaches for it, with its left hand for the
	/// player's left hand. Whether it took the giving.
	virtual bool GiveTo(entt::entity /*creature*/, entt::entity /*held*/, bool /*leftHand*/) { return false; }
	/// While the creature reaches for what the hand gave it: the thing it reaches for and how many milliseconds it is from
	/// taking it, none before it reaches out
	struct Taking
	{
		entt::entity object {entt::null};
		float msFromTake {0.0f};
	};
	[[nodiscard]] virtual std::optional<Taking> TakingOf(entt::entity /*creature*/) const { return std::nullopt; }
	/// Where the creature's hand takes it from the hand, facing the hand
	[[nodiscard]] virtual std::optional<glm::vec3> TakePointOf(entt::entity /*creature*/, glm::vec3 /*hand*/) const
	{
		return std::nullopt;
	}
	/// Whether the creature is still about taking what it was given, turning to the player or reaching
	[[nodiscard]] virtual bool StillTaking(entt::entity /*creature*/) const { return false; }
	/// The creature's size, which bounds how far the hand may go while it reaches
	[[nodiscard]] virtual float CreatureSizeOf(entt::entity /*creature*/) const { return 1.0f; }
	/// The hand went too far while the creature reached: the creature gives up taking it
	virtual void CallOffGiving(entt::entity /*creature*/) {}
	/// Whether the player's hand is the right hand
	[[nodiscard]] virtual bool RightHanded() const { return false; }
	/// Whether the hand's player holds their creature's leash loose in the hand, tied to nothing
	[[nodiscard]] virtual bool HoldsLooseLeash() const = 0;
	/// A tap on a thing, let go before the hand took it, given to the leash held loose in the hand: whether the leash took
	/// it as an order to the creature, in place of the thing's own tap
	virtual bool LeashTap(entt::entity object) = 0;
	/// A local random number below a count
	[[nodiscard]] virtual uint32_t LocalRandom(uint32_t count) = 0;
	/// Sends a command of the hand's player. The world does the command at the start of a later turn.
	virtual void SendCommand(player_commands::Action action) = 0;
	/// Whether the world is a network game, in which the turn writes the clicks of the hand that the scripts read
	[[nodiscard]] virtual bool IsNetworkGame() const { return false; }
	/// A villager goes into the hand: it remembers what it was doing and is held
	virtual void VillagerIntoHand(entt::entity villager) = 0;
	/// An animal leaves its flock for one of its own, the old one going if it is left empty
	virtual void AnimalIntoOwnFlock(entt::entity animal) = 0;
	/// A rock, static or dead tree taken stops being its town's artefact, remembering who took it
	virtual void ArtefactTaken(entt::entity object, PlayerNames player) = 0;
	/// A one-shot miracle's globe goes into the hand: the player has now had its miracle within reach
	virtual void GlobeIntoHand(entt::entity /*globe*/, PlayerNames /*player*/) {}
	/// Pulling up a tree moves the player's alignment
	virtual void TreeUprooted(PlayerNames player, entt::entity tree) = 0;
	/// A big forest gives a tree for the hand, taking a tree's wood from it (which may shrink it or use it up): a full
	/// grown conifer made at its place. None when it gives none.
	virtual std::optional<entt::entity> TreeFromBigForest(entt::entity /*bigForest*/) { return std::nullopt; }
	/// A tree pulled out of the ground leaves its roots in a hole, with a puff of smoke. The hole has the place, the
	/// turn and the size that the tree had before the pull. Only the computer that pulls the tree draws the hole.
	virtual void LeaveRootsHole(entt::entity tree, const glm::vec3& place, const glm::mat3& rotation, float scale) = 0;
	/// Make the land's draw list again on the next frames, this number of frames
	virtual void RemakeDrawList(uint32_t frames) = 0;
	/// A pot taken stops calling the people to it
	virtual void RemovePotReaction(entt::entity pot) = 0;
	/// A pot let go over the land calls the people to it again, when it calls nobody yet, for the player letting it go
	virtual void SetUpPotReaction(entt::entity pot, PlayerNames player) = 0;
	/// Where a thing is drawn, its scaled axes and its origin
	struct Pose
	{
		glm::mat3 axes {1.0f};
		glm::vec3 origin {0.0f};
	};
	[[nodiscard]] virtual Pose PoseOf(entt::entity object) const = 0;
	/// The place of the hand in the world: where the last report of its player put it. The turns measure from here.
	[[nodiscard]] virtual glm::vec3 ReportedHandPlace() const { return PoseOf(Hand()).origin; }
	/// The point under the cursor that the last report of the hand's player picked, none before the first report. The
	/// turns ask the influence there.
	[[nodiscard]] virtual std::optional<glm::vec3> ReportedCollidePoint() const { return PickedPoint(); }
	/// Where the last report of its player put a hand
	[[nodiscard]] virtual glm::vec3 ReportedPlaceOf([[maybe_unused]] entt::entity hand) const { return ReportedHandPlace(); }
	/// The players of a network game, other than the first, whose gods have hands of their own, in the order of the
	/// players
	[[nodiscard]] virtual std::vector<PlayerNames> OtherHandPlayers() const { return {}; }
	/// Whether a hand is the hand of another player of a network game, which this computer does not move or draw
	[[nodiscard]] virtual bool IsOtherPlayersHand([[maybe_unused]] entt::entity hand) const { return false; }
	/// A point as the game keeps a place on the map: the ground under it, the height above the ground and the place on
	/// the map's grid of fixed points
	[[nodiscard]] virtual glm::vec3 AsMapPlace(glm::vec3 point) const { return point; }
	virtual void SetPose(entt::entity object, const Pose& pose) = 0;
	/// The pose that the frames draw a thing in while the hand pulls at it. The world keeps the thing as it stood.
	virtual void DrawPulledPose(entt::entity object, const Pose& pose) { SetPose(object, pose); }
	/// The frames draw the thing at its own place again, as the pull ends
	virtual void ForgetPulledPose([[maybe_unused]] entt::entity object) {}
	/// A thing's axes at its own size, as they are once it stops being stretched
	[[nodiscard]] virtual glm::mat3 UnstretchedAxes(entt::entity object, const glm::mat3& axes) const = 0;

	/// Letting go, on the computer of the player that lets go: where the thing starts as it leaves the hand, and its
	/// velocity. The thing starts as this computer last drew it, comes up out of the land, and lies along the land or
	/// not. Then it flies for the turns of lead.
	[[nodiscard]] virtual throw_lead::LedThrow ThrowStartOf(entt::entity object, bool alignToSlope, glm::vec3 velocity,
	                                                        int leadTurns) = 0;
	/// The latency of the reports of this computer's hand, in milliseconds (see throw_lead::ReportLatency)
	[[nodiscard]] virtual uint32_t ReportLatencyMs() const { return 0; }
	/// The turn that the frames of this computer are in, and how far through it they are
	[[nodiscard]] virtual uint32_t FrameTurn() const { return 0; }
	[[nodiscard]] virtual float FrameTurnFraction() const { return 0.0f; }
	/// The frames draw a thing that this computer threw at a place on the flight of its lead. The axes have no size.
	virtual void DrawThrownPose([[maybe_unused]] entt::entity object, [[maybe_unused]] const physics::Pose& pose) {}
	/// Letting go, on every computer: the thing goes to the start that the command has, at its own size
	virtual void StartThrowFrom(entt::entity object, const player_commands::ThrowStart& start) = 0;
	/// A pot let go slowly is poured out where it is: what it holds goes to what takes it there or into a pile, and the
	/// pot goes
	virtual std::optional<uint32_t> PourPot(entt::entity pot, PlayerNames player) = 0;

	/// What a pile or pot holds, and what its handful's table says of scooping it
	struct PotFacts
	{
		PotType potType {PotType::Pot};
		ResourceType resource {ResourceType::Food};
		/// The handful the hand scoops out of it
		PotInfo handful {PotInfo::HandFood};
		uint32_t amount {0};
		/// Its food is poisoned, and poisons what it is given to
		bool poisoned {false};
	};
	[[nodiscard]] virtual std::optional<PotFacts> PotFactsOf(entt::entity pot) const = 0;
	/// What a field has for the hand to scoop: its food, and whether its crop is ripe (a ripe crop gives half)
	struct FieldFacts
	{
		uint32_t food {0};
		bool ripe {false};
		/// The hand can lock onto it only when its crop has grown at all and it has more than one food
		bool lockable {true};
	};
	[[nodiscard]] virtual std::optional<FieldFacts> FieldFactsOf(entt::entity field) const = 0;
	/// Food is taken out of a field by the hand
	virtual void TakeFromField(entt::entity field, uint32_t amount) = 0;
	/// The hand took a thing at a point: the player of most influence there is where the hand's resource came from
	virtual void NoteTakenAt(glm::vec3 /*point*/) {}
	/// The hand scooped from a pile or a fish farm: the player of its town is where the hand's resource came from
	virtual void NoteTakenFromOwnerOf(entt::entity /*thing*/) {}
	/// What a fish farm's table says of its fish, for the hand scooping them
	[[nodiscard]] virtual std::optional<fish_farm::Type> FishFarmOf(entt::entity farm) const = 0;
	/// Fish are taken out of a fish farm by the hand: no more than it has. What was taken.
	virtual uint32_t TakeFromFishFarm(entt::entity farm, uint32_t amount) = 0;
	/// A pot, a handful among them, is drawn at the size of what it holds
	virtual void ResizePot(entt::entity pot) = 0;
	[[nodiscard]] virtual ScoopFacts ScoopFactsOf(PotInfo handful) const = 0;
	/// Some of what a pile holds is taken out of it: no more than it has. The pile goes once it has nothing left, unless
	/// it is a store's. What was taken.
	virtual uint32_t TakeFromPile(entt::entity pile, uint32_t amount) = 0;
	/// A handful the hand holds is made, holding so much, where the hand is; poisoned when scooped from poison
	[[nodiscard]] virtual entt::entity MakeHandful(PotInfo type, glm::vec3 position, uint32_t amount, bool poisoned) = 0;
	/// The stream of what is scooped flowing from its source into the hand starts, and stops; its effect
	[[nodiscard]] virtual std::optional<uint32_t> StartScoopStream(ResourceType resource, glm::vec3 source, bool poisoned) = 0;
	/// The stream of fish leaping from a fish farm into the hand starts
	[[nodiscard]] virtual std::optional<uint32_t> StartFishScoopStream(glm::vec3 source) = 0;
	virtual void StopScoopStream(uint32_t stream) = 0;
	/// The stream flows to where the hand now is
	virtual void MoveScoopStream(uint32_t stream, glm::vec3 hand) = 0;
	/// The cursor is pinned where it is while the hand scoops, and let go when it stops
	virtual void PinCursor(bool pinned) = 0;
	/// The scooping sound, asked for every game turn of a scoop: food pours and wood rattles in a loop that starts once,
	/// where the hand is, and goes on until it is stopped, its pitch following the scoop's ramp. Gives the sound playing.
	[[nodiscard]] virtual entt::entity PlayScoopSound(entt::entity playing, ResourceType resource, glm::vec3 hand,
	                                                  float ramp) = 0;
	/// The scooping sounds stop, food's and wood's
	virtual void StopScoopSound() = 0;
	[[nodiscard]] virtual float LandHeightAt(glm::vec3 point) const = 0;
	/// Whether something stores a resource, as a village store stores food and wood
	[[nodiscard]] virtual bool StoresResource(entt::entity store, ResourceType resource) const = 0;
	/// A store takes what it will of some resource; what it took
	virtual uint32_t AddToStore(entt::entity store, ResourceType resource, uint32_t amount, bool poisoned) = 0;
	/// A store takes a thing whole, for the resource it is worth, and the thing goes; whether it took it
	virtual bool TakeIntoStore(entt::entity store, entt::entity object) = 0;
	/// Whether a thing takes a held thing whole when the hand gives it: a worship site's altar that takes its kind, a
	/// gate stone plinth that takes a gate stone, or a store that takes what it is worth. Nothing changes.
	[[nodiscard]] virtual bool TakesWhole(entt::entity /*target*/, entt::entity /*held*/) const { return false; }
	/// Lays a gate stone in the gate stone plinth it is given to; false for anything else, which is left as it is
	virtual bool LayGateStone(entt::entity plinth, entt::entity stone) = 0;
	/// Some resource poured onto a point: to the stores and piles of it about the point, or a new pile
	virtual void PourAt(ResourceType resource, glm::vec3 point, uint32_t amount, PlayerNames player, bool poisoned) = 0;
	/// A thing is used up: a ghost of it flickers out where it was, and it goes
	virtual void UseUp(entt::entity object) = 0;
	/// The held thing is given to a worship site's altar: sacrificed there and gone, or refused and kept in the hand
	virtual ecs::altar_offering::Answer OfferAtAltar(entt::entity /*altar*/, entt::entity /*held*/)
	{
		return ecs::altar_offering::Answer::NotAnAltar;
	}
	/// The player at this machine does one of the things the help system counts
	virtual void HelpEvent(uint32_t /*event*/) {}
	/// It goes into the physics from the hand, put down or thrown
	virtual ecs::systems::FromHandResult LetGoFromHand(entt::entity object, const ecs::systems::FromHand& release) = 0;
	/// Whether the player's hand throws things without the air's drag, as a script can set
	[[nodiscard]] virtual bool PlayerHasNoWindResistance(PlayerNames player) const = 0;
	/// A body's mass and speed, none for a thing not in the physics
	struct BodyFacts
	{
		float mass {0.0f};
		float speed {0.0f};
	};
	[[nodiscard]] virtual std::optional<BodyFacts> BodyOf(entt::entity object) const = 0;
	/// A turning force on a body for the rest of the game turn
	virtual void TwistBody(entt::entity object, glm::vec3 torque) = 0;
	/// The body of what the hand let go flies without the air's drag
	virtual void DropDrag(entt::entity object) = 0;
};

} // namespace openblack::hand_grab
