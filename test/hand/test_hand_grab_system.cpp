/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <bit>
#include <functional>
#include <map>
#include <numbers>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Animals/FishFarmRules.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandHeldSeed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/HandGrabSystem.h"
#include "Help/HelpProfile.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Flat land everywhere, things standing where they are put, and a record of what the hand did
class FakeWorld final: public hand_grab::HandGrabWorldInterface
{
public:
	FakeWorld()
	{
		hand = registry.Create();
		registry.Assign<Transform>(hand, glm::vec3(0.0f, 10.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		secondHand = registry.Create();
		registry.Assign<Transform>(secondHand, glm::vec3(0.0f, 10.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	}

	entt::entity AddRock(glm::vec3 position, float radius = 1.0f)
	{
		const auto rock = registry.Create();
		registry.Assign<Transform>(rock, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
		sizes[rock] = {.radius = radius, .height = 2.0f};
		return rock;
	}

	entt::entity AddGlobe(glm::vec3 position)
	{
		const auto globe = registry.Create();
		registry.Assign<Transform>(globe, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<OneOffSpellSeed>(globe, OneOffSpellSeed {.seedType = SpellSeedType::Food, .position = position});
		sizes[globe] = {.radius = 1.0f, .height = 2.0f};
		return globe;
	}

	entt::entity AddScaffold(glm::vec3 position)
	{
		const auto scaffold = registry.Create();
		registry.Assign<Transform>(scaffold, position, glm::mat3(1.0f), glm::vec3(0.7f));
		registry.Assign<Scaffold>(scaffold);
		sizes[scaffold] = {.radius = 2.0f, .height = 3.0f};
		return scaffold;
	}

	entt::entity AddShownSeed(glm::vec3 position, PlayerNames player)
	{
		const auto seed = registry.Create();
		registry.Assign<Transform>(seed, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Shield, .player = player, .followsSpell = true});
		sizes[seed] = {.radius = 1.0f, .height = 2.0f};
		return seed;
	}

	entt::entity AddTree(glm::vec3 position, float weight)
	{
		const auto tree = registry.Create();
		registry.Assign<Transform>(tree, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Tree>(tree);
		sizes[tree] = {.radius = 1.0f, .height = 10.0f};
		weights[tree] = weight;
		return tree;
	}

	entt::entity AddBigForest(glm::vec3 position)
	{
		const auto forest = registry.Create();
		registry.Assign<Transform>(forest, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<BigForest>(forest);
		sizes[forest] = {.radius = 10.0f, .height = 12.0f};
		return forest;
	}

	Registry& Entities() override { return registry; }
	[[nodiscard]] entt::entity Hand() const override
	{
		if (acting == PlayerNames::PLAYER_TWO)
		{
			return secondPlayerHasHands ? secondHand : entt::null;
		}
		return hand;
	}
	[[nodiscard]] PlayerNames HandPlayer() const override { return acting.value_or(PlayerNames::PLAYER_ONE); }
	[[nodiscard]] bool IsOtherPlayersHand(entt::entity of) const override { return otherHands.contains(of); }
	[[nodiscard]] std::optional<PlayerNames> PlayerOfHand(entt::entity of) const override
	{
		return of == secondHand ? std::optional(PlayerNames::PLAYER_TWO) : std::nullopt;
	}
	[[nodiscard]] glm::vec3 ReportedHandPlace() const override
	{
		const auto found = reportedPlaces.find(Hand());
		return found != reportedPlaces.end() ? found->second : PoseOf(Hand()).origin;
	}
	[[nodiscard]] glm::vec3 ReportedPlaceOf(entt::entity of) const override
	{
		const auto found = reportedPlaces.find(of);
		return found != reportedPlaces.end() ? found->second : ReportedHandPlace();
	}
	[[nodiscard]] glm::vec3 HeldPlaceAt(glm::vec3 point) const override { return point + heldPlaceStep; }
	void SetActingPlayer(std::optional<PlayerNames> player) override { acting = player; }
	[[nodiscard]] std::optional<PlayerNames> GetActingPlayer() const override { return acting; }
	[[nodiscard]] std::optional<PlayerNames> OwnHandPlayer() const override { return ownHandPlayer; }
	[[nodiscard]] std::optional<entt::entity> ObjectUnderCursor() const override { return underCursor; }
	[[nodiscard]] std::optional<entt::entity> ObjectNearHand() const override { return nearHand; }
	[[nodiscard]] std::optional<glm::vec3> PickedPoint() const override { return pickedPoint; }
	[[nodiscard]] std::optional<glm::vec3> ReportedCollidePoint() const override
	{
		return reportedCollide.has_value() ? reportedCollide : pickedPoint;
	}
	[[nodiscard]] bool InInfluence(PlayerNames player, glm::vec3 point) const override
	{
		return influence && (!influenceOnlyOf.has_value() || player == *influenceOnlyOf) &&
		       (!borderX.has_value() || point.x < *borderX);
	}
	void HeldThingUsedOnLand(PlayerNames player) override { usedOnLand.push_back(player); }
	[[nodiscard]] bool InBounds(glm::vec3) const override { return true; }
	[[nodiscard]] glm::vec3 LandNormalAt(glm::vec3) const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] Size SizeOf(entt::entity object) const override
	{
		const auto found = sizes.find(object);
		return found != sizes.end() ? found->second : Size {};
	}
	[[nodiscard]] float WeightOf(entt::entity object) const override
	{
		const auto found = weights.find(object);
		return found != weights.end() ? found->second : 1.0f;
	}
	[[nodiscard]] float LifeOf(entt::entity) const override { return 1.0f; }
	[[nodiscard]] bool IsFlying(entt::entity object) const override { return flying.contains(object); }
	[[nodiscard]] bool SpeciesAllowsPickUp(entt::entity) const override { return true; }
	[[nodiscard]] MobileStaticInfo StaticKindOf(MobileStaticInfo) const override { return MobileStaticInfo::Rock; }
	[[nodiscard]] MeshId StaticMeshOf(MobileStaticInfo) const override { return MeshId::Dummy; }
	[[nodiscard]] bool IsLoosePot(PotInfo type) const override { return type == PotInfo::FoodPot; }
	[[nodiscard]] bool IsOfRockMaterial(entt::entity object) const override { return registry.AllOf<MobileStatic>(object); }
	[[nodiscard]] bool IsSexuallyActive(entt::entity) const override { return true; }
	[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity object) const override
	{
		const auto found = owners.find(object);
		return found != owners.end() ? found->second : std::optional(PlayerNames::PLAYER_ONE);
	}

	void LeavePhysicsAndMap(entt::entity object) override
	{
		leftWorld.push_back(object);
		flying.erase(object);
	}
	void CreateReaction(const ReactionRequest& request) override { reactions.push_back(request); }
	void RemoveReactions(entt::entity, Reaction) override {}
	void StopReactionSpreadInHand(entt::entity object) override { spreadStopped.push_back(object); }
	void SpreadReactionsAgain(entt::entity object) override { spreadAgain.push_back(object); }
	void FireStartedMoving(entt::entity, bool) override {}
	void CatchFirefly(entt::entity) override {}
	void CountHelpEvent(uint32_t event) override { helpEvents.push_back(event); }
	void ForgetGesturePath() override { ++pathsForgotten; }
	[[nodiscard]] bool IsLocalHand() const override { return localHand; }
	void HeatHeld(entt::entity object) override { heated.push_back(object); }
	void ArtefactTaken(entt::entity object, PlayerNames) override { artefactsTaken.push_back(object); }
	void GlobeIntoHand(entt::entity globe, PlayerNames player) override { globesTaken.emplace_back(globe, player); }
	void ScaffoldIntoHand(entt::entity scaffold, PlayerNames player) override { scaffoldsTaken.emplace_back(scaffold, player); }
	[[nodiscard]] std::optional<hand_grab::HoldFacts> SeedHoldOf(entt::entity seed) const override
	{
		if (!registry.AllOf<SpellSeed>(seed))
		{
			return std::nullopt;
		}
		return hand_grab::HoldFacts {.type = HoldType::Magic, .loweringMultiplier = 0.5f, .holdRadius = 1.0f};
	}
	bool SeedIntoHand(entt::entity seed, PlayerNames player) override
	{
		seedsTakenBack.emplace_back(seed, player);
		return seedKept;
	}
	void PlaySample(uint32_t sample, glm::vec3) override { samples.push_back(sample); }
	bool TapThing(entt::entity object, glm::vec3 place, PlayerNames) override
	{
		tapped.push_back(object);
		tapPlaces.push_back(place);
		return true;
	}
	[[nodiscard]] bool ValidToTap(entt::entity object) const override { return tappable.contains(object); }
	[[nodiscard]] std::optional<creature_giving::Verdict> GiveVerdict(entt::entity target, entt::entity) const override
	{
		const auto found = creatures.find(target);
		return found != creatures.end() ? std::optional(found->second) : std::nullopt;
	}
	void SayCantPickUp(entt::entity creature) override { cantPickUp.push_back(creature); }
	bool GiveTo(entt::entity creature, entt::entity held, bool leftHand) override
	{
		given.push_back({creature, held, leftHand});
		stillTaking = true;
		return true;
	}
	[[nodiscard]] std::optional<Taking> TakingOf(entt::entity) const override { return taking; }
	[[nodiscard]] std::optional<glm::vec3> TakePointOf(entt::entity, glm::vec3) const override { return takePoint; }
	[[nodiscard]] bool StillTaking(entt::entity) const override { return stillTaking; }
	[[nodiscard]] float CreatureSizeOf(entt::entity) const override { return 2.0f; }
	void CallOffGiving(entt::entity creature) override
	{
		calledOff.push_back(creature);
		stillTaking = false;
	}
	[[nodiscard]] bool RightHanded() const override { return rightHanded; }
	[[nodiscard]] bool HoldsLooseLeash() const override { return looseLeash; }
	bool LeashTap(entt::entity object) override
	{
		if (!leashTakesTaps)
		{
			return false;
		}
		leashTapped.push_back(object);
		return true;
	}
	[[nodiscard]] uint32_t LocalRandom(uint32_t) override { return 0; }
	[[nodiscard]] bool IsNetworkGame() const override { return networkGame; }
	bool networkGame {false};
	void SendCommand(player_commands::Action action) override
	{
		sent.push_back(action);
		if (onCommand)
		{
			onCommand(action);
		}
	}
	/// The commands that the hand sent, in their order, and what the test does with each as it comes
	std::vector<player_commands::Action> sent;
	std::function<void(const player_commands::Action&)> onCommand;
	void VillagerIntoHand(entt::entity villager) override { villagersInHand.push_back(villager); }
	void AnimalIntoOwnFlock(entt::entity) override {}
	void TreeUprooted(PlayerNames, entt::entity tree) override { uprooted.push_back(tree); }
	std::optional<entt::entity> TreeFromBigForest(entt::entity bigForest) override
	{
		const auto tree = AddTree(registry.Get<const Transform>(bigForest).position, 10.0f);
		treesGiven.emplace_back(bigForest, tree);
		return tree;
	}
	void LeaveRootsHole(entt::entity tree, const glm::vec3& place, const glm::mat3& rotation, float scale) override
	{
		holes.push_back(tree);
		holePlaces.push_back({.place = place, .rotation = rotation, .scale = scale});
	}
	void RemakeDrawList(uint32_t frames) override
	{
		++drawListsRemade;
		drawListFrames.push_back(frames);
	}
	void RemovePotReaction(entt::entity) override {}
	void SetUpPotReaction(entt::entity pot, PlayerNames) override { potReactionsSetUp.push_back(pot); }
	[[nodiscard]] Pose PoseOf(entt::entity object) const override
	{
		const auto& transform = registry.Get<const Transform>(object);
		auto axes = transform.rotation;
		for (glm::length_t column = 0; column < 3; ++column)
		{
			axes[column] *= transform.scale[column];
		}
		return {.axes = axes, .origin = transform.position};
	}
	void SetPose(entt::entity object, const Pose& pose) override
	{
		auto& transform = registry.Get<Transform>(object);
		for (glm::length_t column = 0; column < 3; ++column)
		{
			const float length = glm::length(pose.axes[column]);
			transform.rotation[column] = pose.axes[column] / length;
			transform.scale[column] = length;
		}
		transform.position = pose.origin;
	}
	// As the game's world: the frames draw the pulled pose apart from the world's Transform
	void DrawPulledPose(entt::entity object, const Pose& pose) override
	{
		HandPulled pulled {.position = pose.origin, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
		for (glm::length_t column = 0; column < 3; ++column)
		{
			const float length = glm::length(pose.axes[column]);
			pulled.rotation[column] = pose.axes[column] / length;
			pulled.scale[column] = length;
		}
		registry.AssignOrReplace<HandPulled>(object, pulled);
	}
	void ForgetPulledPose(entt::entity object) override
	{
		if (registry.Valid(object) && registry.AllOf<HandPulled>(object))
		{
			registry.Remove<HandPulled>(object);
		}
	}
	[[nodiscard]] glm::mat3 UnstretchedAxes(entt::entity, const glm::mat3& axes) const override
	{
		glm::mat3 result = axes;
		for (glm::length_t column = 0; column < 3; ++column)
		{
			result[column] = glm::normalize(axes[column]);
		}
		return result;
	}
	[[nodiscard]] hand_grab::throw_lead::LedThrow ThrowStartOf(entt::entity object, bool alignToSlope, glm::vec3 velocity,
	                                                           int leadTurns) override
	{
		startsAsked.emplace_back(object, alignToSlope);
		leadsAsked.push_back(leadTurns);
		const auto* inHand = registry.TryGet<const InHand>(object);
		drawnAtStart.push_back(inHand != nullptr && inHand->drawn.has_value());
		return {.start = throwStart, .velocity = ledVelocity.value_or(velocity), .flight = flight};
	}
	[[nodiscard]] uint32_t ReportLatencyMs() const override { return reportLatencyMs; }
	[[nodiscard]] uint32_t FrameTurn() const override { return frameTurn; }
	[[nodiscard]] float FrameTurnFraction() const override { return frameTurnFraction; }
	void DrawThrownPose(entt::entity object, const physics::Pose& pose) override
	{
		drawnThrown.emplace_back(object, pose.origin);
	}
	void StartThrowFrom(entt::entity object, const player_commands::ThrowStart& start) override
	{
		startedFrom.emplace_back(object, start);
	}
	std::optional<uint32_t> PourPot(entt::entity pot, PlayerNames) override
	{
		poured.push_back(pot);
		return ++streams;
	}
	[[nodiscard]] std::optional<FieldFacts> FieldFactsOf(entt::entity field) const override
	{
		const auto found = fields.find(field);
		return found != fields.end() ? std::optional(found->second) : std::nullopt;
	}
	void ResizePot(entt::entity pot) override { resized.push_back(pot); }
	void TakeFromField(entt::entity field, uint32_t amount) override
	{
		auto& facts = fields.at(field);
		facts.food -= std::min(facts.food, amount);
	}
	[[nodiscard]] std::optional<fish_farm::Type> FishFarmOf(entt::entity farm) const override
	{
		return fishFarms.contains(farm) ? std::optional(fishFarmType) : std::nullopt;
	}
	uint32_t TakeFromFishFarm(entt::entity farm, uint32_t amount) override
	{
		return fish_farm::Take(fishFarms.at(farm), amount);
	}
	[[nodiscard]] std::optional<PotFacts> PotFactsOf(entt::entity pot) const override
	{
		const auto* data = registry.TryGet<const Pot>(pot);
		if (data == nullptr)
		{
			return std::nullopt;
		}
		const bool handful = data->type == PotInfo::HandFood;
		return PotFacts {.potType = handful ? PotType::Pot : PotType::PileFood,
		                 .resource = ResourceType::Food,
		                 .handful = PotInfo::HandFood,
		                 .amount = data->amount,
		                 .poisoned = data->poisoned};
	}
	[[nodiscard]] hand_grab::ScoopFacts ScoopFactsOf(PotInfo) const override
	{
		return {.initial = 25, .perTurn = 8, .perTurnEnd = 70, .maxPickedUp = 20000, .rampSeconds = 6.0f};
	}
	uint32_t TakeFromPile(entt::entity pile, uint32_t amount) override
	{
		auto& pot = registry.Get<Pot>(pile);
		const auto taken = std::min(amount, pot.amount);
		pot.amount -= taken;
		if (pot.amount == 0)
		{
			registry.Destroy(pile);
		}
		return taken;
	}
	[[nodiscard]] entt::entity MakeHandful(PotInfo type, glm::vec3 position, uint32_t amount, bool poisoned) override
	{
		const auto handful = registry.Create();
		registry.Assign<Transform>(handful, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Pot>(handful, Pot {.amount = amount, .maxAmount = 20000, .type = type, .poisoned = poisoned});
		sizes[handful] = {.radius = 0.5f, .height = 1.0f};
		return handful;
	}
	[[nodiscard]] std::optional<uint32_t> StartScoopStream(ResourceType, glm::vec3, bool) override
	{
		++streams;
		return streams;
	}
	[[nodiscard]] std::optional<uint32_t> StartFishScoopStream(glm::vec3 source) override
	{
		fishStreams.push_back(source);
		return ++streams;
	}
	void StopScoopStream(uint32_t stream) override { stopped.push_back(stream); }
	void MoveScoopStream(uint32_t, glm::vec3 hand) override { streamPoints.push_back(hand); }
	void PinCursor(bool pinned) override { cursorPinned = pinned; }
	[[nodiscard]] entt::entity PlayScoopSound(entt::entity playing, ResourceType resource, glm::vec3, float ramp) override
	{
		scoopSounds.push_back(ramp);
		scoopSoundResources.push_back(resource);
		// The loop starts once and goes on while it plays
		if (playing == entt::null || !scoopSoundPlaying)
		{
			++scoopSoundStarts;
			scoopSoundPlaying = true;
		}
		return k_ScoopSoundLoop;
	}
	void StopScoopSound() override
	{
		++scoopSoundStops;
		scoopSoundPlaying = false;
	}
	[[nodiscard]] float LandHeightAt(glm::vec3) const override { return 0.0f; }
	[[nodiscard]] bool StoresResource(entt::entity store, ResourceType) const override { return stores.contains(store); }
	uint32_t AddToStore(entt::entity store, ResourceType, uint32_t amount, bool /*poisoned*/) override
	{
		stored[store] += amount;
		return amount;
	}
	bool TakeIntoStore(entt::entity store, entt::entity object) override
	{
		if (!stores.contains(store))
		{
			return false;
		}
		takenWhole.emplace_back(store, object);
		registry.Destroy(object);
		return true;
	}
	[[nodiscard]] bool TakesWhole(entt::entity target, entt::entity held) const override
	{
		return stores.contains(target) || (plinths.contains(target) && gateStones.contains(held));
	}
	[[nodiscard]] bool JoinsScaffold(entt::entity target, entt::entity held) const override
	{
		return scaffolds.contains(target) && scaffolds.contains(held);
	}
	void JoinScaffold(entt::entity target, entt::entity held) override
	{
		joined.emplace_back(target, held, registry.AllOf<InHand>(held));
	}
	entt::entity RestOfScaffold(entt::entity scaffold) override
	{
		restsAsked.push_back(scaffold);
		return restOfScaffold;
	}
	[[nodiscard]] bool DropsOnTeleport(entt::entity stone, entt::entity held) const override
	{
		return teleportStones.contains(stone) && !registry.AllOf<Pot>(held);
	}
	void DropOnTeleport(entt::entity stone, entt::entity held) override
	{
		droppedOnStones.emplace_back(stone, held, registry.AllOf<InHand>(held));
	}
	bool LayGateStone(entt::entity plinth, entt::entity stone) override
	{
		if (!plinths.contains(plinth) || !gateStones.contains(stone))
		{
			return false;
		}
		laidStones.emplace_back(plinth, stone);
		return true;
	}
	void PourAt(ResourceType, glm::vec3, uint32_t amount, PlayerNames, bool /*poisoned*/) override
	{
		pouredAmounts.push_back(amount);
	}
	void UseUp(entt::entity object) override
	{
		usedUp.push_back(object);
		registry.Destroy(object);
	}
	FromHandResult LetGoFromHand(entt::entity object, const FromHand& release) override
	{
		released.emplace_back(object, release.velocity);
		releasedSpins.push_back(release.angularMomentum);
		return {.accepted = true};
	}
	[[nodiscard]] bool PlayerHasNoWindResistance(PlayerNames) const override { return false; }
	[[nodiscard]] std::optional<BodyFacts> BodyOf(entt::entity object) const override
	{
		return bodies.contains(object) ? std::optional(BodyFacts {.mass = 2.0f, .speed = 3.0f}) : std::nullopt;
	}
	void TwistBody(entt::entity object, glm::vec3 torque) override { twists.emplace_back(object, torque); }
	void DropDrag(entt::entity) override {}

	Registry registry;
	entt::entity hand {entt::null};
	/// The hand of the second player, which a command of that player acts with
	entt::entity secondHand {entt::null};
	/// The hands of the other gods of a network game, and where each last reported its hand
	std::set<entt::entity> otherHands;
	std::map<entt::entity, glm::vec3> reportedPlaces;
	/// What the map position of a held thing adds to the reported point of the hand
	glm::vec3 heldPlaceStep {0.0f};
	/// The second player has hands only once a network game gave them
	bool secondPlayerHasHands {false};
	std::optional<PlayerNames> acting;
	/// The player whose hands this computer moves, when it is not the first player
	std::optional<PlayerNames> ownHandPlayer;
	/// The players of things that are not the first player's; a missing optional is a thing of nobody's
	std::map<entt::entity, std::optional<PlayerNames>> owners;
	std::optional<entt::entity> underCursor;
	std::optional<entt::entity> nearHand;
	bool influence {true};
	/// When set, only this player has influence anywhere
	std::optional<PlayerNames> influenceOnlyOf;
	/// The point picked under the cursor, and with a border the influence reaches only west of it
	std::optional<glm::vec3> pickedPoint {glm::vec3(0.0f)};
	/// The point that the last report of the hand's player picked; the picked point of this computer when none
	std::optional<glm::vec3> reportedCollide;
	std::optional<float> borderX;
	bool looseLeash {false};
	/// The leash held loose in the hand takes taps as orders
	bool leashTakesTaps {false};
	std::vector<entt::entity> leashTapped;
	std::vector<PlayerNames> usedOnLand;
	std::map<entt::entity, Size> sizes;
	std::map<entt::entity, float> weights;
	std::set<entt::entity> flying;
	std::set<entt::entity> bodies;
	std::vector<entt::entity> leftWorld;
	std::vector<ReactionRequest> reactions;
	std::vector<entt::entity> spreadStopped;
	std::vector<entt::entity> spreadAgain;
	std::vector<entt::entity> heated;
	std::vector<uint32_t> helpEvents;
	int pathsForgotten {0};
	bool localHand {true};
	std::vector<entt::entity> artefactsTaken;
	std::vector<std::pair<entt::entity, PlayerNames>> globesTaken;
	std::vector<std::pair<entt::entity, PlayerNames>> scaffoldsTaken;
	/// The seeds that the hand took back, and whether the miracles keep such a seed in the hand
	std::vector<std::pair<entt::entity, PlayerNames>> seedsTakenBack;
	bool seedKept {true};
	std::vector<uint32_t> samples;
	std::vector<entt::entity> tapped;
	/// The place of the hand that each tap used
	std::vector<glm::vec3> tapPlaces;
	std::set<entt::entity> tappable;
	std::vector<entt::entity> villagersInHand;
	std::vector<entt::entity> uprooted;
	std::vector<std::pair<entt::entity, entt::entity>> treesGiven;
	std::vector<entt::entity> holes;
	struct HolePlace
	{
		glm::vec3 place;
		glm::mat3 rotation;
		float scale;
	};
	std::vector<HolePlace> holePlaces;
	int drawListsRemade {0};
	std::vector<uint32_t> drawListFrames;
	std::vector<entt::entity> poured;
	std::vector<entt::entity> potReactionsSetUp;
	std::map<entt::entity, FieldFacts> fields;
	std::map<entt::entity, float> fishFarms;
	fish_farm::Type fishFarmType {.foodValue = 1400.0f, .foodType = 1, .turnsPerFish = 16};
	std::vector<glm::vec3> fishStreams;
	std::vector<std::pair<entt::entity, entt::entity>> takenWhole;
	std::vector<std::pair<entt::entity, glm::vec3>> released;
	std::vector<glm::vec3> releasedSpins;
	/// The start that the world gives for a throw, and the starts that the hand asked for and used
	player_commands::ThrowStart throwStart;
	std::vector<std::pair<entt::entity, bool>> startsAsked;
	/// For each start asked for, whether the thing had a drawn place in the hand
	std::vector<bool> drawnAtStart;
	/// The lead of each throw: the latency of the reports, the turns that the hand asked for, the velocity and the
	/// flight after the lead, and where the frames drew the thrown thing
	uint32_t reportLatencyMs {0};
	std::vector<int> leadsAsked;
	std::optional<glm::vec3> ledVelocity;
	std::optional<hand_grab::throw_lead::Flight> flight;
	uint32_t frameTurn {0};
	float frameTurnFraction {0.0f};
	std::vector<std::pair<entt::entity, glm::vec3>> drawnThrown;
	std::vector<std::pair<entt::entity, player_commands::ThrowStart>> startedFrom;
	std::vector<std::pair<entt::entity, glm::vec3>> twists;
	uint32_t streams {0};
	std::vector<uint32_t> stopped;
	std::vector<glm::vec3> streamPoints;
	std::vector<entt::entity> resized;
	std::vector<float> scoopSounds;
	std::vector<ResourceType> scoopSoundResources;
	static constexpr auto k_ScoopSoundLoop = static_cast<entt::entity>(4242);
	uint32_t scoopSoundStarts {0};
	uint32_t scoopSoundStops {0};
	bool scoopSoundPlaying {false};
	bool cursorPinned {false};
	std::set<entt::entity> stores;
	std::set<entt::entity> plinths;
	std::set<entt::entity> gateStones;
	/// The teleport stones that take a dropped villager, and each drop: the stone, the villager, and whether the hand
	/// still held the villager then
	std::set<entt::entity> teleportStones;
	std::vector<std::tuple<entt::entity, entt::entity, bool>> droppedOnStones;
	std::vector<std::pair<entt::entity, entt::entity>> laidStones;
	/// The scaffolds that the hand can join, and each join: the target, the held scaffold, and whether the hand still
	/// held it then
	std::set<entt::entity> scaffolds;
	std::vector<std::tuple<entt::entity, entt::entity, bool>> joined;
	std::vector<entt::entity> restsAsked;
	entt::entity restOfScaffold {entt::null};
	std::map<entt::entity, uint32_t> stored;
	std::vector<uint32_t> pouredAmounts;
	std::vector<entt::entity> usedUp;
	/// Creatures, and whether they would take what the hand holds
	std::map<entt::entity, creature_giving::Verdict> creatures;
	std::vector<entt::entity> cantPickUp;
	struct Given
	{
		entt::entity creature;
		entt::entity held;
		bool leftHand;
	};
	std::vector<Given> given;
	std::optional<Taking> taking;
	std::optional<glm::vec3> takePoint;
	bool stillTaking {false};
	std::vector<entt::entity> calledOff;
	bool rightHanded {false};
};

} // namespace

class HandGrabSystemWithWorld: public ::testing::Test
{
protected:
	HandGrabSystemWithWorld()
	{
		auto owned = std::make_unique<FakeWorld>();
		world = owned.get();
		system = std::make_unique<HandGrabSystem>(std::move(owned));
		// The world does each command of the hand at once, unless a test keeps the commands for a later turn
		world->onCommand = [this](const player_commands::Action& action) {
			if (deferCommands)
			{
				waiting.push_back(action);
				return;
			}
			Run(action);
		};
		// The hand has been over the land a frame before anything is pressed
		Frame(0);
	}

	/// One game turn of the first player's hand: the scoop in the input of the turn, then the held thing in the hand step
	void Turn()
	{
		system->LockTurn(PlayerNames::PLAYER_ONE);
		system->ProcessTurn();
	}

	/// The world does one command of the hand's player
	void Run(const player_commands::Action& action)
	{
		const auto player = world->HandPlayer();
		std::visit(
		    [this, player](const auto& command) {
			    using Kind = std::decay_t<decltype(command)>;
			    if constexpr (std::is_same_v<Kind, player_commands::HandTake>)
			    {
				    system->DoTake(player, command.object);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandLetGo>)
			    {
				    system->DoLetGo(player, command);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandTap>)
			    {
				    system->DoTap(player, command.object);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandGive>)
			    {
				    system->DoGive(player, command.creature, command.object, command.leftHand);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandTwist>)
			    {
				    system->DoTwist(player, command.object, command.moved);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandLock>)
			    {
				    system->DoLock(player, command.object);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandUnlock>)
			    {
				    system->DoUnlock(player, command.object);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandApply>)
			    {
				    system->DoApply(player, command.target);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandClick>)
			    {
				    system->DoClick(player, command, commandTurn);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::GiveCallOff>)
			    {
				    system->DoGiveCallOff(player, command.creature);
			    }
		    },
		    action);
	}

	/// The turn at which the world does the commands
	uint32_t commandTurn {0};

	/// The start of a turn: the world does the commands that wait
	void RunWaiting()
	{
		const auto commands = std::exchange(waiting, {});
		for (const auto& command : commands)
		{
			Run(command);
		}
	}

	glm::vec3 Frame(uint32_t gameMs, glm::vec3 target = {0.0f, 0.0f, 0.0f}, glm::vec3 ground = {0.0f, 0.0f, 0.0f})
	{
		now += gameMs;
		return system->UpdateFrame({.target = target,
		                            .rayOrigin = target + glm::vec3(0.0f, 50.0f, 0.0f),
		                            .rayDirection = {0.0f, -1.0f, 0.0f},
		                            .cursorGround = ground,
		                            .handSize = 1.0f,
		                            .seconds = static_cast<float>(gameMs) * 0.001f,
		                            .gameMs = gameMs,
		                            .nowMs = now,
		                            .turn = now / 100});
	}

	bool Press() { return system->Press(now, now / 100); }
	std::optional<entt::entity> Release() { return system->Release(now, now / 100); }

	FakeWorld* world {nullptr};
	std::unique_ptr<HandGrabSystem> system;
	uint32_t now {1000};
	/// The commands wait for a turn, as in the game, when the test asks for it
	bool deferCommands {false};
	std::vector<player_commands::Action> waiting;
};

TEST_F(HandGrabSystemWithWorld, ARockIsTakenOnceTheHandHasFadedIntoItsPull)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	EXPECT_TRUE(Press());
	EXPECT_TRUE(system->IsBusy());
	// The pull waits for the 0.13 s fade, then anything but a tree comes free at once and is taken the frame after
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(*system->GetHeld(), rock);
	EXPECT_TRUE(world->registry.AllOf<InHand>(rock));
	EXPECT_EQ(world->leftWorld, std::vector<entt::entity> {rock});
	// It sounds as it is picked up, and the people about the hand see it
	ASSERT_FALSE(world->samples.empty());
	EXPECT_EQ(world->samples.front(), 10u);
	ASSERT_FALSE(world->reactions.empty());
	EXPECT_EQ(world->reactions.front().type, Reaction::ReactToHandPickUp);
	EXPECT_EQ(world->reactions.front().initiator, world->hand);
}

// The debug tools let go at once with no command. In a network game, each computer does this let go in its own turn, so
// the start must not come from the place that one computer drew.
TEST_F(HandGrabSystemWithWorld, InANetworkGameTheLetGoOfTheToolsDoesNotStartFromTheDrawnPlace)
{
	world->networkGame = true;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	ASSERT_TRUE(system->TakeIntoHand(rock));
	world->registry.Get<InHand>(rock).drawn = InHand::Drawn {.position = {1.0f, 2.0f, 3.0f}, .rotation = glm::mat3(1.0f)};
	ASSERT_TRUE(system->LetGoAt(glm::vec3(0.0f)));
	ASSERT_EQ(world->drawnAtStart.size(), 1u);
	EXPECT_FALSE(world->drawnAtStart.front());
}

TEST_F(HandGrabSystemWithWorld, InAGameAloneTheLetGoOfTheToolsStartsFromTheDrawnPlace)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	ASSERT_TRUE(system->TakeIntoHand(rock));
	world->registry.Get<InHand>(rock).drawn = InHand::Drawn {.position = {1.0f, 2.0f, 3.0f}, .rotation = glm::mat3(1.0f)};
	ASSERT_TRUE(system->LetGoAt(glm::vec3(0.0f)));
	ASSERT_EQ(world->drawnAtStart.size(), 1u);
	EXPECT_TRUE(world->drawnAtStart.front());
}

// The hand does not hold a creature as a thing. Because of this, the hand never gives a creature to a creature.
TEST_F(HandGrabSystemWithWorld, TheHandNeverTakesACreature)
{
	const auto creature = world->registry.Create();
	world->registry.Assign<Transform>(creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Creature>(creature);
	world->underCursor = creature;
	Press();
	for (int i = 0; i < 20; ++i)
	{
		Frame(10);
	}
	Release();
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_FALSE(world->registry.AllOf<InHand>(creature));
}

// Pressed with nothing under the cursor, the hand reaches for what stands nearest the land behind it; something under
// the cursor goes first
TEST_F(HandGrabSystemWithWorld, WithNothingUnderTheCursorThePressReachesForWhatIsNearTheHand)
{
	const auto near = world->AddRock({1.0f, 0.0f, 0.0f});
	world->nearHand = near;
	EXPECT_TRUE(Press());
	ASSERT_TRUE(world->registry.TryGet<HandGrab>(world->hand) != nullptr);
	EXPECT_EQ(world->registry.Get<HandGrab>(world->hand).object, near);
	Release();

	const auto under = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = under;
	now += 1000;
	EXPECT_TRUE(Press());
	EXPECT_EQ(world->registry.Get<HandGrab>(world->hand).object, under);
}

TEST_F(HandGrabSystemWithWorld, TheHandKeepsToItsPullUntilItHoldsAndRisesTheFrameItTakes)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	EXPECT_TRUE(Press());
	const glm::vec3 cursor {0.0f, 5.0f, 0.0f};
	std::optional<glm::vec3> taken;
	for (int i = 0; i < 40 && !taken.has_value(); ++i)
	{
		const auto hand = Frame(10, cursor);
		if (system->GetHeld().has_value())
		{
			taken = hand;
		}
		else
		{
			// Every frame before it is held, the frame it comes free too, the hand keeps the pose of its pull
			EXPECT_TRUE(system->GetPullPose().has_value()) << "frame " << i;
		}
	}
	ASSERT_TRUE(taken.has_value());
	EXPECT_FALSE(system->GetPullPose().has_value());
	// The frame it takes the thing the hand is already where it holds it, risen above the cursor as on the frames after
	const auto next = Frame(10, cursor);
	EXPECT_GT(next.y, cursor.y);
	EXPECT_FLOAT_EQ(taken->y, next.y);
}

TEST_F(HandGrabSystemWithWorld, AShortPressIsATap)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	EXPECT_TRUE(Press());
	Frame(50);
	const auto tapped = Release();
	ASSERT_TRUE(tapped.has_value());
	EXPECT_EQ(*tapped, rock);
	EXPECT_FALSE(system->IsBusy());
	// The tap reaches the thing
	ASSERT_EQ(world->tapped.size(), 1u);
	EXPECT_EQ(world->tapped.front(), rock);
}

// A one-shot miracle's globe is a loose thing: a quick press taps it (for its miracle), a held one carries it from the
// side, hung seven tenths of its height below the hand
TEST_F(HandGrabSystemWithWorld, AShortPressTapsAGlobe)
{
	const auto globe = world->AddGlobe({0.0f, 0.0f, 0.0f});
	world->underCursor = globe;
	EXPECT_TRUE(Press());
	Frame(50);
	const auto tapped = Release();
	ASSERT_TRUE(tapped.has_value());
	EXPECT_EQ(world->tapped, std::vector<entt::entity> {globe});
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_TRUE(world->globesTaken.empty());
}

TEST_F(HandGrabSystemWithWorld, AHeldPressCarriesAGlobeFromTheSide)
{
	const auto globe = world->AddGlobe({0.0f, 0.0f, 0.0f});
	world->underCursor = globe;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(*system->GetHeld(), globe);
	EXPECT_TRUE(world->tapped.empty());
	// The player has had its miracle within reach
	ASSERT_EQ(world->globesTaken.size(), 1u);
	EXPECT_EQ(world->globesTaken.front().first, globe);
	const auto pose = system->GetHeldPose();
	ASSERT_TRUE(pose.has_value());
	EXPECT_EQ(pose->hold, HoldType::Side);
	EXPECT_FLOAT_EQ(world->registry.Get<HandGrab>(world->hand).lowering, 0.7f * 2.0f);
}

// A held press on the player's own seed that shows over its miracle pulls it, then gives it back to the miracles. The
// grab of the hand ends: the hand holds the seed as its miracle, not as a thing.
TEST_F(HandGrabSystemWithWorld, AHeldPressTakesTheOwnShownSeedBackToTheMiracles)
{
	const auto seed = world->AddShownSeed({0.0f, 0.0f, 0.0f}, PlayerNames::PLAYER_ONE);
	world->underCursor = seed;
	EXPECT_EQ(system->WhyNotTake(seed), "");
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && world->seedsTakenBack.empty(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(world->seedsTakenBack.size(), 1u);
	EXPECT_EQ(world->seedsTakenBack.front(), std::pair(seed, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_FALSE(system->IsBusy());
	// The pick-up sound plays as the hand sends the take
	EXPECT_EQ(world->samples.size(), 1u);
	// The help counts the take, and those about the hand see it pick something up
	EXPECT_EQ(world->helpEvents, std::vector<uint32_t> {help::profile::k_PickUp});
	ASSERT_EQ(world->reactions.size(), 1u);
	EXPECT_EQ(world->reactions.front().type, Reaction::ReactToHandPickUp);
	EXPECT_EQ(world->spreadStopped, std::vector<entt::entity> {seed});
	EXPECT_EQ(world->pathsForgotten, 1);
	// It never goes into the hand as a thing
	EXPECT_TRUE(world->leftWorld.empty());
}

// A seed whose miracle has too little left goes at the take: the hand counts no take and nobody sees one
TEST_F(HandGrabSystemWithWorld, ASeedThatGoesAtTheTakeCountsNoTake)
{
	world->seedKept = false;
	const auto seed = world->AddShownSeed({0.0f, 0.0f, 0.0f}, PlayerNames::PLAYER_ONE);
	world->underCursor = seed;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && world->seedsTakenBack.empty(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(world->seedsTakenBack.size(), 1u);
	EXPECT_FALSE(system->IsBusy());
	EXPECT_TRUE(world->helpEvents.empty());
	EXPECT_TRUE(world->reactions.empty());
	EXPECT_EQ(world->pathsForgotten, 0);
}

// The hand takes back only a seed of its own player
TEST_F(HandGrabSystemWithWorld, TheHandLeavesTheShownSeedOfAnotherPlayer)
{
	const auto seed = world->AddShownSeed({0.0f, 0.0f, 0.0f}, PlayerNames::PLAYER_TWO);
	world->underCursor = seed;
	EXPECT_NE(system->WhyNotTake(seed), "");
	EXPECT_FALSE(Press());
	for (int i = 0; i < 40; ++i)
	{
		Frame(10);
	}
	EXPECT_TRUE(world->seedsTakenBack.empty());
}

// A seed that a hand holds is no thing for the hand to take
TEST_F(HandGrabSystemWithWorld, AHeldSeedIsNoThingToTake)
{
	const auto seed = world->AddShownSeed({0.0f, 0.0f, 0.0f}, PlayerNames::PLAYER_ONE);
	world->registry.Assign<HandHeldSeed>(world->secondHand, seed);
	EXPECT_NE(system->WhyNotTake(seed), "");
}

TEST_F(HandGrabSystemWithWorld, AHeldPressCarriesAScaffoldFromTheSideAndItsWorkshopIsTold)
{
	const auto scaffold = world->AddScaffold({0.0f, 0.0f, 0.0f});
	world->underCursor = scaffold;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(*system->GetHeld(), scaffold);
	EXPECT_TRUE(world->tapped.empty());
	// The hand tells the workshop one time, so that the workshop keeps the scaffold's place in its yard
	ASSERT_EQ(world->scaffoldsTaken.size(), 1u);
	EXPECT_EQ(world->scaffoldsTaken.front().first, scaffold);
	const auto pose = system->GetHeldPose();
	ASSERT_TRUE(pose.has_value());
	EXPECT_EQ(pose->hold, HoldType::Side);
}

// A scaffold put down gently keeps what its building needs: the rest is a new scaffold, and the hand takes it
TEST_F(HandGrabSystemWithWorld, TheRestOfAScaffoldPutDownStaysInTheHand)
{
	const auto scaffold = world->AddScaffold({0.0f, 0.0f, 0.0f});
	world->underCursor = scaffold;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(system->GetHeld(), std::optional(scaffold));
	world->restOfScaffold = world->AddScaffold({0.0f, 0.0f, 0.0f});
	system->ForceDrop();
	ASSERT_EQ(world->restsAsked, std::vector<entt::entity> {scaffold});
	EXPECT_EQ(system->GetHeld(), std::optional(world->restOfScaffold));
	EXPECT_TRUE(world->registry.AllOf<InHand>(world->restOfScaffold));
	EXPECT_FALSE(world->registry.AllOf<InHand>(scaffold));
	// The workshop system hears of the new scaffold in the hand
	ASSERT_EQ(world->scaffoldsTaken.size(), 2u);
	EXPECT_EQ(world->scaffoldsTaken.back().first, world->restOfScaffold);
}

// With no rest, the hand is empty after it lets go of a scaffold
TEST_F(HandGrabSystemWithWorld, AScaffoldWithNoRestLeavesTheHandEmpty)
{
	const auto scaffold = world->AddScaffold({0.0f, 0.0f, 0.0f});
	world->underCursor = scaffold;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(system->GetHeld(), std::optional(scaffold));
	system->ForceDrop();
	ASSERT_EQ(world->restsAsked, std::vector<entt::entity> {scaffold});
	EXPECT_FALSE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AShortPressOnAScaffoldTakesNothing)
{
	const auto scaffold = world->AddScaffold({0.0f, 0.0f, 0.0f});
	world->underCursor = scaffold;
	EXPECT_TRUE(Press());
	Frame(50);
	Release();
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_TRUE(world->scaffoldsTaken.empty());
}

// The hand takes a scaffold of the town of another god as any other scaffold, but only in its influence
TEST_F(HandGrabSystemWithWorld, AScaffoldOfAnotherGodsTownIsTakenOnlyInTheInfluence)
{
	const auto scaffold = world->AddScaffold({0.0f, 0.0f, 0.0f});
	world->registry.Get<Scaffold>(scaffold).town = world->registry.Create();
	world->underCursor = scaffold;
	world->influenceOnlyOf = PlayerNames::PLAYER_TWO;
	EXPECT_FALSE(Press());
	EXPECT_TRUE(world->scaffoldsTaken.empty());
	world->influenceOnlyOf.reset();
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	EXPECT_EQ(system->GetHeld(), std::optional(scaffold));
}

TEST_F(HandGrabSystemWithWorld, WithTheLeashHeldAShortPressIsAnOrderInsteadOfATap)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	world->leashTakesTaps = true;
	EXPECT_TRUE(Press());
	Frame(50);
	const auto tapped = Release();
	ASSERT_TRUE(tapped.has_value());
	ASSERT_EQ(world->leashTapped.size(), 1u);
	EXPECT_EQ(world->leashTapped.front(), rock);
	EXPECT_TRUE(world->tapped.empty());
}

TEST_F(HandGrabSystemWithWorld, ThingsOutOfTheInfluenceOrHeldByAScriptAreLeft)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	world->influence = false;
	EXPECT_FALSE(Press());
	world->influence = true;
	world->registry.Assign<CannotBePickedUp>(rock);
	EXPECT_FALSE(Press());
	world->registry.Remove<CannotBePickedUp>(rock);
	// A rock wider than the hand can lift
	const auto boulder = world->AddRock({5.0f, 0.0f, 0.0f}, 4.0f);
	world->tappable.insert(boulder);
	world->underCursor = boulder;
	EXPECT_FALSE(Press());
	// Out of the influence or held by a script nothing is tapped; a boulder the hand can't lift is tapped at once
	ASSERT_EQ(world->tapped.size(), 1u);
	EXPECT_EQ(world->tapped.front(), boulder);
}

// A hand that holds a thing does not tap the thing under it, as a temple's entrance: the press makes ready to throw
TEST_F(HandGrabSystemWithWorld, AHandThatHoldsAThingTapsNothing)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	Release();
	world->tapped.clear();
	const auto boulder = world->AddRock({5.0f, 0.0f, 0.0f}, 4.0f);
	world->underCursor = boulder;
	EXPECT_TRUE(Press());
	EXPECT_TRUE(world->tapped.empty());
}

// The influence is asked where the thing under the cursor stands, not where the hand rests on the land behind it
TEST_F(HandGrabSystemWithWorld, ATakeAsksTheInfluenceWhereThePickedThingStands)
{
	world->borderX = 10.0f;
	// Standing just inside, with the land the hand rests on outside: taken
	const auto inside = world->AddRock({8.0f, 0.0f, 0.0f});
	world->underCursor = inside;
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	EXPECT_TRUE(Press());
	Release();
	// Standing just outside, with the land the hand rests on inside: left, and not tapped
	const auto outside = world->AddRock({12.0f, 0.0f, 0.0f});
	world->underCursor = outside;
	world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	EXPECT_FALSE(Press());
	EXPECT_EQ(std::ranges::count(world->tapped, outside), 0);
}

// The turn asks the influence at the point of the last report of the hand's player, never at the cursor of this computer
TEST_F(HandGrabSystemWithWorld, ATakeAtTheTurnAsksTheInfluenceAtTheReportedPoint)
{
	world->borderX = 10.0f;
	const auto rock = world->AddRock({8.0f, 0.0f, 0.0f});
	// The cursor of this computer is outside, the report of the player inside: taken
	world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	world->reportedCollide = glm::vec3(8.0f, 0.0f, 0.0f);
	system->DoTake(world->HandPlayer(), rock);
	EXPECT_EQ(system->GetHeld(), std::optional(rock));
	system->Reset();
	// The cursor inside, the report outside: left
	const auto other = world->AddRock({9.0f, 0.0f, 0.0f});
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	world->reportedCollide = glm::vec3(12.0f, 0.0f, 0.0f);
	system->DoTake(world->HandPlayer(), other);
	EXPECT_NE(system->GetHeld(), std::optional(other));
}

// The steps of a computer god take a thing also outside the influence of its player
TEST_F(HandGrabSystemWithWorld, AComputerGodTakesAThingOutsideItsInfluence)
{
	world->borderX = 10.0f;
	const auto rock = world->AddRock({12.0f, 0.0f, 0.0f});
	world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	world->reportedCollide = glm::vec3(12.0f, 0.0f, 0.0f);
	// The take of a player outside the influence leaves the rock
	system->DoTake(world->HandPlayer(), rock);
	EXPECT_NE(system->GetHeld(), std::optional(rock));
	// The take of a computer god takes it
	system->DoComputerTake(world->HandPlayer(), rock);
	EXPECT_EQ(system->GetHeld(), std::optional(rock));
}

// The other rules of a take apply to the steps of a computer god
TEST_F(HandGrabSystemWithWorld, AComputerGodLeavesAThingThatCannotBePickedUp)
{
	world->borderX = 10.0f;
	const auto rock = world->AddRock({8.0f, 0.0f, 0.0f});
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	world->reportedCollide = glm::vec3(8.0f, 0.0f, 0.0f);
	world->registry.Assign<CannotBePickedUp>(rock);
	system->DoComputerTake(world->HandPlayer(), rock);
	EXPECT_NE(system->GetHeld(), std::optional(rock));
}

TEST_F(HandGrabSystemWithWorld, AScoopAsksTheInfluenceWhereThePileStands)
{
	world->borderX = 10.0f;
	const auto AddPile = [this](float x) {
		const auto pile = world->registry.Create();
		world->registry.Assign<Transform>(pile, glm::vec3(x, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
		world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
		return pile;
	};
	// A pile just outside isn't scooped from, though the hand rests inside
	const auto outside = AddPile(12.0f);
	world->underCursor = outside;
	world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	EXPECT_FALSE(Press());
	EXPECT_EQ(world->registry.Get<const Pot>(outside).amount, 1000u);
	// A pile just inside is, though the hand rests outside
	const auto inside = AddPile(8.0f);
	world->underCursor = inside;
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	EXPECT_TRUE(Press());
	EXPECT_EQ(world->registry.Get<const Pot>(inside).amount, 975u);
}

TEST_F(HandGrabSystemWithWorld, LettingGoAsksTheInfluenceAtThePointPicked)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	world->underCursor.reset();
	world->borderX = 10.0f;
	// Over a thing standing just outside, with the land the hand rests on inside: the press does nothing
	world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Release();
	EXPECT_TRUE(system->GetHeld().has_value());
	EXPECT_TRUE(world->released.empty());
	// Over a thing standing just inside, with the land the hand rests on outside: thrown
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(world->released.size(), 1u);
}

// The help counts each thing that the hand takes and each thing that it lets go of. A let-go that the influence
// refuses does not count.
TEST_F(HandGrabSystemWithWorld, TheHelpCountsATakeAndALetGo)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(world->helpEvents, std::vector<uint32_t> {help::profile::k_PickUp});
	world->underCursor.reset();
	world->borderX = 10.0f;
	world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(world->helpEvents.size(), 1u);
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(world->helpEvents, (std::vector<uint32_t> {help::profile::k_PickUp, help::profile::k_Throw}));
}

// The hand starts the path of its gesture again when it takes a thing and when it lets go of it. A let-go that the
// influence refuses does not.
TEST_F(HandGrabSystemWithWorld, TheGesturePathStartsAgainOnATakeAndALetGo)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(world->pathsForgotten, 1);
	world->underCursor.reset();
	world->borderX = 10.0f;
	world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(world->pathsForgotten, 1);
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(world->pathsForgotten, 2);
}

// The hand of a computer god or of another player takes and lets go of things without wiping the local player's path
TEST_F(HandGrabSystemWithWorld, TheGesturePathIsLeftAloneByAHandThatIsNotTheLocalOne)
{
	world->localHand = false;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(world->pathsForgotten, 0);
	world->underCursor.reset();
	world->borderX = 10.0f;
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_EQ(world->pathsForgotten, 0);
}

// The help counts only the events of the local hand: the hand of a computer god or of another player takes and lets go
// of things with no count
TEST_F(HandGrabSystemWithWorld, TheHelpCountsNothingForAHandThatIsNotTheLocalOne)
{
	world->localHand = false;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	world->underCursor.reset();
	world->borderX = 10.0f;
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_TRUE(world->helpEvents.empty());
}

// The hand says why a press on a thing wouldn't take it, as the inspector shows it
// The hand stops the spread of the reactions of a thing that it takes. They spread again when the hand lets go.
TEST_F(HandGrabSystemWithWorld, TheReactionsOfAHeldThingSpreadAgainOnALetGo)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(world->spreadStopped, std::vector<entt::entity> {rock});
	EXPECT_TRUE(world->spreadAgain.empty());
	world->underCursor.reset();
	world->borderX = 10.0f;
	world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_EQ(world->spreadAgain, std::vector<entt::entity> {rock});
}

TEST_F(HandGrabSystemWithWorld, ItSaysWhyAThingWouldntBeTaken)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	EXPECT_EQ(system->WhyNotTake(rock), "");
	world->influence = false;
	EXPECT_NE(system->WhyNotTake(rock).find("influence"), std::string::npos);
	world->influence = true;
	world->registry.Assign<CannotBePickedUp>(rock);
	EXPECT_NE(system->WhyNotTake(rock).find("script"), std::string::npos);
	world->registry.Remove<CannotBePickedUp>(rock);
	EXPECT_NE(system->WhyNotTake(world->AddRock({5.0f, 0.0f, 0.0f}, 4.0f)).find("can't hold"), std::string::npos);
	EXPECT_NE(system->WhyNotTake(world->registry.Create()).find("can't hold"), std::string::npos);
	// Busy taking the rock, it takes nothing else
	EXPECT_TRUE(Press());
	EXPECT_NE(system->WhyNotTake(rock).find("busy"), std::string::npos);
}

TEST_F(HandGrabSystemWithWorld, ItSaysThatAPressOnAPileScoopsFromIt)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	EXPECT_TRUE(system->WouldScoop(pile));
	EXPECT_FALSE(system->WouldScoop(world->AddRock({5.0f, 0.0f, 0.0f})));
	// Not out of the influence
	world->influence = false;
	EXPECT_FALSE(system->WouldScoop(pile));
	world->influence = true;
	// Not from an empty pile
	world->registry.Get<Pot>(pile).amount = 0;
	EXPECT_FALSE(system->WouldScoop(pile));
}

TEST_F(HandGrabSystemWithWorld, APressTheHandCantTakeClicksTheThing)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	// Even out of the influence, where it isn't tapped
	world->influence = false;
	commandTurn = now / 100;
	EXPECT_FALSE(Press());
	const auto* clicked = world->registry.TryGet<const HandClicked>(world->hand);
	ASSERT_NE(clicked, nullptr);
	EXPECT_EQ(clicked->thing, rock);
	EXPECT_EQ(clicked->thingTurn, now / 100);
	EXPECT_TRUE(world->tapped.empty());
}

TEST_F(HandGrabSystemWithWorld, APressOnAThingAScriptFixedInPlaceIsAPressOnTheLand)
{
	const auto stone = world->AddRock({0.0f, 0.0f, 0.0f});
	world->registry.Assign<CannotBePickedUp>(stone);
	world->underCursor = stone;
	commandTurn = now / 100;
	EXPECT_FALSE(Press());
	// The press does not click or tap the stone, in the influence or out of it
	EXPECT_FALSE(world->registry.AllOf<HandClicked>(world->hand) &&
	             world->registry.Get<HandClicked>(world->hand).thing != entt::null);
	world->influence = false;
	EXPECT_FALSE(Press());
	EXPECT_FALSE(world->registry.AllOf<HandClicked>(world->hand) &&
	             world->registry.Get<HandClicked>(world->hand).thing != entt::null);
	EXPECT_TRUE(world->tapped.empty());
	// Only the release clicks it, so one click of the button clicks it once
	system->ClickReleased();
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, stone);
	// A thing that takes taps, or any thing with the leash loose in the hand, is clicked by the press
	world->registry.Get<HandClicked>(world->hand).thing = entt::null;
	world->tappable.insert(stone);
	EXPECT_FALSE(Press());
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, stone);
	world->tappable.erase(stone);
	world->registry.Get<HandClicked>(world->hand).thing = entt::null;
	world->looseLeash = true;
	EXPECT_FALSE(Press());
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, stone);
}

// A tap on a stone that a script fixed in place is only a click, which the script reads. The creature gets no deed,
// no reaction and no help event from it, so it can not learn from the taps of the singing stones.
TEST_F(HandGrabSystemWithWorld, ATapOnAFixedStoneGivesTheCreatureNothingToSee)
{
	const auto stone = world->AddRock({0.0f, 0.0f, 0.0f});
	world->registry.Assign<CannotBePickedUp>(stone);
	world->underCursor = stone;
	commandTurn = now / 100;
	EXPECT_FALSE(Press());
	Frame(50);
	EXPECT_FALSE(Release().has_value());
	system->ClickReleased();
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, stone);
	EXPECT_TRUE(world->tapped.empty());
	EXPECT_TRUE(world->reactions.empty());
	EXPECT_TRUE(world->potReactionsSetUp.empty());
	EXPECT_TRUE(world->helpEvents.empty());
}

TEST(HandGrabRules, APressTapsAThingTheHandCouldHoldOrThatTakesTaps)
{
	const hand_grab::Gate holdable {.valid = true};
	const hand_grab::Gate fixed {.valid = true, .cannotBePickedUp = true};
	const hand_grab::Gate notHoldable {.valid = false};
	EXPECT_TRUE(hand_grab::PressTapsThing(holdable, false, false));
	EXPECT_FALSE(hand_grab::PressTapsThing(fixed, false, false));
	EXPECT_FALSE(hand_grab::PressTapsThing(notHoldable, false, false));
	EXPECT_TRUE(hand_grab::PressTapsThing(fixed, true, false));
	EXPECT_TRUE(hand_grab::PressTapsThing(notHoldable, false, true));
}

TEST_F(HandGrabSystemWithWorld, ATapClicksWhatIsUnderTheHandAsItIsLetGo)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	EXPECT_TRUE(Press());
	// Taking hold isn't a click
	EXPECT_FALSE(world->registry.AllOf<HandClicked>(world->hand) &&
	             world->registry.Get<HandClicked>(world->hand).thing != entt::null);
	Frame(50);
	const auto other = world->AddRock({5.0f, 0.0f, 0.0f});
	world->underCursor = other;
	Release();
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, other);
}

TEST_F(HandGrabSystemWithWorld, AHeldPressIsNoClick)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	EXPECT_TRUE(Press());
	Frame(300);
	Release();
	const auto* clicked = world->registry.TryGet<const HandClicked>(world->hand);
	EXPECT_TRUE(clicked == nullptr || clicked->thing == entt::null);
}

TEST_F(HandGrabSystemWithWorld, LettingGoOverTheLandClicksThePlace)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	commandTurn = 7;
	system->ClickReleased();
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, rock);
	world->underCursor.reset();
	Frame(10, {}, {20.0f, 0.0f, 30.0f});
	commandTurn = 8;
	system->ClickReleased();
	const auto& clicked = world->registry.Get<HandClicked>(world->hand);
	EXPECT_TRUE(clicked.thing == entt::null);
	EXPECT_EQ(clicked.place.x, map_coords::ToFixed(20.0f));
	EXPECT_EQ(clicked.place.z, map_coords::ToFixed(30.0f));
	EXPECT_EQ(clicked.placeTurn, 8u);
}

TEST_F(HandGrabSystemWithWorld, TheTurnWritesTheClicks)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	commandTurn = 12;
	system->ClickThing(rock);
	ASSERT_EQ(world->sent.size(), 1u);
	EXPECT_EQ(std::get<player_commands::HandClick>(world->sent[0]).thing, rock);
	// The click has the number of the turn that wrote it, not of the frame that sent it
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, rock);
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thingTurn, 12u);
	// A let go over the land sends the place
	world->underCursor.reset();
	Frame(10, {}, {20.0f, 0.0f, 30.0f});
	commandTurn = 13;
	system->ClickReleased();
	const auto& clicked = world->registry.Get<HandClicked>(world->hand);
	EXPECT_TRUE(clicked.thing == entt::null);
	EXPECT_EQ(clicked.place.x, map_coords::ToFixed(20.0f));
	EXPECT_EQ(clicked.placeTurn, 13u);
	// A thing that is gone by the turn is not clicked
	const auto gone = world->AddRock({5.0f, 0.0f, 0.0f});
	world->underCursor = gone;
	waiting.clear();
	world->onCommand = [this](const player_commands::Action& action) { waiting.push_back(action); };
	system->ClickReleased();
	world->registry.Destroy(gone);
	RunWaiting();
	EXPECT_TRUE(world->registry.Get<HandClicked>(world->hand).thing == entt::null);
}

TEST_F(HandGrabSystemWithWorld, ARewardWithTheLeashLooseInTheHandIsTheLand)
{
	const auto chest = world->AddRock({0.0f, 0.0f, 0.0f});
	world->registry.Assign<Reward>(chest);
	world->underCursor = chest;
	world->looseLeash = true;
	Frame(10, {}, {20.0f, 0.0f, 30.0f});
	commandTurn = 8;
	system->ClickReleased();
	const auto& clicked = world->registry.Get<HandClicked>(world->hand);
	EXPECT_TRUE(clicked.thing == entt::null);
	EXPECT_EQ(clicked.placeTurn, 8u);
	world->looseLeash = false;
	system->ClickReleased();
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, chest);
}

TEST_F(HandGrabSystemWithWorld, ClicksAreForgottenAfterFifteenSeconds)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	commandTurn = now / 100;
	system->ClickReleased();
	// 149 turns of 100 ms is under 15 seconds; 150 is just over, as a thousandth in single precision is a little more.
	// The turn forgets the clicks, not the frames.
	Frame(14900);
	system->InterfaceTurn(now, now / 100);
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, rock);
	Frame(100);
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).thing, rock);
	system->InterfaceTurn(now, now / 100);
	EXPECT_TRUE(world->registry.Get<HandClicked>(world->hand).thing == entt::null);
}

TEST_F(HandGrabSystemWithWorld, InANetworkGameEveryComputerForgetsTheClicksOfAllHands)
{
	// This computer moves the hands of the second player. The first player clicked a place, and the second a thing.
	world->networkGame = true;
	world->secondPlayerHasHands = true;
	world->ownHandPlayer = PlayerNames::PLAYER_TWO;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	system->DoClick(PlayerNames::PLAYER_ONE,
	                player_commands::HandClick {.thing = entt::null,
	                                            .release = true,
	                                            .hasPlace = true,
	                                            .placeX = map_coords::ToFixed(20.0f),
	                                            .placeZ = map_coords::ToFixed(30.0f)},
	                10);
	system->DoClick(PlayerNames::PLAYER_TWO, player_commands::HandClick {.thing = rock, .release = false}, 10);
	ASSERT_EQ(world->registry.Get<HandClicked>(world->hand).place.x, map_coords::ToFixed(20.0f));
	ASSERT_EQ(world->registry.Get<HandClicked>(world->secondHand).thing, rock);
	// Before 15 seconds, the turn keeps both clicks
	system->InterfaceTurn(15900, 159);
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).place.x, map_coords::ToFixed(20.0f));
	EXPECT_EQ(world->registry.Get<HandClicked>(world->secondHand).thing, rock);
	// After 15 seconds, the turn forgets the click of the first player too, not only the click of this computer's hand
	system->InterfaceTurn(16000, 160);
	EXPECT_EQ(world->registry.Get<HandClicked>(world->hand).place.x, 0);
	EXPECT_TRUE(world->registry.Get<HandClicked>(world->secondHand).thing == entt::null);
}

TEST_F(HandGrabSystemWithWorld, AThingInFlightIsCaughtOnlyAfterTheWait)
{
	const auto rock = world->AddRock({0.0f, 5.0f, 0.0f});
	world->underCursor = rock;
	world->flying.insert(rock);
	EXPECT_TRUE(Press());
	Frame(100);
	EXPECT_FALSE(system->GetHeld().has_value());
	Frame(130);
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(*system->GetHeld(), rock);
}

TEST_F(HandGrabSystemWithWorld, AFlyingThingThatLandsDuringTheWaitIsTakenAtOnce)
{
	const auto rock = world->AddRock({0.0f, 5.0f, 0.0f});
	world->underCursor = rock;
	world->flying.insert(rock);
	EXPECT_TRUE(Press());
	Frame(50);
	world->flying.erase(rock);
	Frame(10);
	EXPECT_TRUE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AHeavyTreeIsNeverPulledFree)
{
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 1.0e6f);
	world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 100; ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_TRUE(world->holes.empty());
	// While it pulls the hand takes the pose a tree is held with
	const auto pose = system->GetPullPose();
	ASSERT_TRUE(pose.has_value());
	EXPECT_EQ(pose->hold, HoldType::Tree);
	// Let go before it came free, it stays where it is
	EXPECT_FALSE(Release().has_value());
	EXPECT_FALSE(system->GetPullPose().has_value());
	EXPECT_FALSE(world->registry.AllOf<InHand>(tree));
}

TEST_F(HandGrabSystemWithWorld, ATreeLetGoBeforeItComesFreeIsDrawnLeaningAndKeepsItsPlaceInTheWorld)
{
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 1.0e6f);
	const auto before = world->registry.Get<const Transform>(tree);
	world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 100; ++i)
	{
		Frame(10, {20.0f, 30.0f, 0.0f});
	}
	// The frames draw the lean and the stretch; the world keeps the tree as it stood
	const auto* pulled = world->registry.TryGet<const HandPulled>(tree);
	ASSERT_NE(pulled, nullptr);
	EXPECT_TRUE(pulled->rotation != before.rotation || pulled->scale != before.scale);
	EXPECT_FALSE(Release().has_value());
	const auto& after = world->registry.Get<const Transform>(tree);
	EXPECT_EQ(after.position, before.position);
	EXPECT_EQ(after.rotation, before.rotation);
	EXPECT_EQ(after.scale, before.scale);
}

TEST_F(HandGrabSystemWithWorld, InANetworkGameAPulledThingKeepsThePlaceThatTheTurnsGaveIt)
{
	world->networkGame = true;
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 1.0e6f);
	world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 100; ++i)
	{
		Frame(10, {20.0f, 30.0f, 0.0f});
	}
	// The turns of every computer move the thing while this hand pulls at it, as they move a villager that walks
	auto& transform = world->registry.Get<Transform>(tree);
	transform.position = {4.0f, 0.5f, -3.0f};
	const auto walked = transform;
	system->PlaceHeldForTurn();
	EXPECT_EQ(transform.position, walked.position);
	EXPECT_EQ(transform.rotation, walked.rotation);
	EXPECT_EQ(transform.scale, walked.scale);
	// The let go does not put it back where the pull started
	EXPECT_FALSE(Release().has_value());
	system->PlaceHeldForTurn();
	EXPECT_EQ(transform.position, walked.position);
	EXPECT_EQ(transform.rotation, walked.rotation);
	EXPECT_EQ(transform.scale, walked.scale);
	EXPECT_FALSE(world->registry.AllOf<InHand>(tree));
}

TEST_F(HandGrabSystemWithWorld, TheRootsHoleHasThePlaceOfTheTreeBeforeThePullNotItsLean)
{
	const auto tree = world->AddTree({3.0f, 1.0f, -2.0f}, 1.0e6f);
	auto& transform = world->registry.Get<Transform>(tree);
	transform.rotation = glm::mat3(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	transform.scale = glm::vec3(1.5f);
	const auto before = transform;
	world->underCursor = tree;
	EXPECT_TRUE(Press());
	// Too heavy to come free, the tree leans towards the hand
	for (int i = 0; i < 50; ++i)
	{
		Frame(10, {20.0f, 30.0f, 0.0f});
	}
	const auto* leaned = world->registry.TryGet<const HandPulled>(tree);
	ASSERT_NE(leaned, nullptr);
	ASSERT_TRUE(leaned->rotation != before.rotation || leaned->scale != before.scale);
	ASSERT_TRUE(world->holes.empty());
	// Light again, it comes free and leaves its hole where it stood before the pull
	world->weights[tree] = 10.0f;
	for (int i = 0; i < 30 && world->holes.empty(); ++i)
	{
		Frame(10, {20.0f, 50.0f, 0.0f});
	}
	ASSERT_EQ(world->holes, std::vector<entt::entity> {tree});
	ASSERT_EQ(world->holePlaces.size(), 1u);
	EXPECT_EQ(world->holePlaces.front().place, before.position);
	EXPECT_EQ(world->holePlaces.front().rotation, before.rotation);
	EXPECT_EQ(world->holePlaces.front().scale, 1.5f);
}

TEST_F(HandGrabSystemWithWorld, ATreeThatThePullFreedIsTakenWhenTheButtonIsLetGoBeforeTheTake)
{
	deferCommands = true;
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30 && world->holes.empty(); ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	ASSERT_EQ(world->holes, std::vector<entt::entity> {tree});
	ASSERT_TRUE(world->sent.empty());
	// The take comes before the release, so a short press is no tap
	EXPECT_FALSE(Release().has_value());
	ASSERT_EQ(world->sent.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandTake>(world->sent.front()));
	EXPECT_EQ(std::get<player_commands::HandTake>(world->sent.front()).object, tree);
	RunWaiting();
	EXPECT_EQ(system->GetHeld(), std::optional(tree));
}

class HandGrabShortTreePress: public HandGrabSystemWithWorld
{
protected:
	/// A press of eight frames of 20 ms on a light tree, with the hand far above its grip. Gives the thing that the hand
	/// holds after the turn that does the commands.
	std::optional<entt::entity> PressEightFrames(entt::entity tree)
	{
		deferCommands = true;
		world->underCursor = tree;
		EXPECT_TRUE(Press());
		// The hand fades into its pull for 0.13 s, and the pull starts on the frame after the fade
		for (int i = 0; i < 7; ++i)
		{
			Frame(20, {0.0f, 50.0f, 0.0f});
			EXPECT_TRUE(world->holes.empty()) << "frame " << i;
		}
		// The first frame of the pull pulls harder than the tree weighs, thus the tree comes free at once
		Frame(20, {0.0f, 50.0f, 0.0f});
		EXPECT_EQ(world->holes, std::vector<entt::entity> {tree});
		// The button comes up after the pull freed the tree: the hand sends the take, not a tap
		EXPECT_FALSE(Release().has_value());
		EXPECT_EQ(world->sent.size(), 1u);
		RunWaiting();
		return system->GetHeld();
	}
};

TEST_F(HandGrabShortTreePress, InASinglePlayerGameAShortPressTakesALightTree)
{
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	EXPECT_EQ(PressEightFrames(tree), std::optional(tree));
	EXPECT_TRUE(world->registry.AllOf<InHand>(tree));
}

TEST_F(HandGrabShortTreePress, InANetworkGameTheSameShortPressTakesTheTreeAtTheSameFrame)
{
	world->networkGame = true;
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	EXPECT_EQ(PressEightFrames(tree), std::optional(tree));
	EXPECT_TRUE(world->registry.AllOf<InHand>(tree));
}

TEST_F(HandGrabShortTreePress, APressOnATreeOutsideTheInfluenceIsAClickAndDoesNotPull)
{
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->underCursor = tree;
	world->influence = false;
	EXPECT_FALSE(Press());
	for (int i = 0; i < 8; ++i)
	{
		Frame(20, {0.0f, 50.0f, 0.0f});
	}
	EXPECT_FALSE(system->GetPullPose().has_value());
	EXPECT_FALSE(world->registry.AllOf<HandPulled>(tree));
	EXPECT_TRUE(world->holes.empty());
	EXPECT_FALSE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, InANetworkGameTheInterfaceAsksTheInfluenceOfItsOwnHand)
{
	world->networkGame = true;
	world->ownHandPlayer = PlayerNames::PLAYER_TWO;
	world->influenceOnlyOf = PlayerNames::PLAYER_TWO;
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->underCursor = tree;
	// What the hand offers and why it would not take a thing are of the hand of this computer, not of the first player
	EXPECT_TRUE(system->IsInInfluence());
	EXPECT_EQ(system->WhyNotTake(tree), "");
	EXPECT_FALSE(system->IsInInfluenceOf(PlayerNames::PLAYER_ONE));
	world->influenceOnlyOf = PlayerNames::PLAYER_ONE;
	EXPECT_FALSE(system->IsInInfluence());
	EXPECT_NE(system->WhyNotTake(tree), "");
}

TEST_F(HandGrabSystemWithWorld, ALightTreeIsUprootedAndCreaks)
{
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(world->holes, std::vector<entt::entity> {tree});
	EXPECT_EQ(world->uprooted, std::vector<entt::entity> {tree});
	// The hand makes the land's draw list again only for a living thing that it pulls free
	EXPECT_EQ(world->drawListsRemade, 0);
	// A tree pulled out of the ground creaks rather than making the pick-up sound
	ASSERT_EQ(world->samples.size(), 1u);
	EXPECT_EQ(world->samples.front(), 32u);
}

TEST_F(HandGrabSystemWithWorld, AVillagerPulledFreeMakesTheDrawListAgain)
{
	const auto villager = world->registry.Create();
	world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Villager>(villager);
	world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	world->underCursor = villager;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(world->drawListFrames, std::vector<uint32_t> {1u});
}

TEST_F(HandGrabSystemWithWorld, AVillagerInTheHandKeepsItsWoodAndFood)
{
	const auto villager = world->registry.Create();
	world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& person = world->registry.Assign<Villager>(villager);
	person.woodHeld = 250;
	person.foodHeld = 120;
	person.woodGraphic = 2;
	world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	world->underCursor = villager;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	const auto& held = world->registry.Get<const Villager>(villager);
	EXPECT_EQ(held.woodHeld, 250);
	EXPECT_EQ(held.foodHeld, 120);
	EXPECT_EQ(held.woodGraphic, 2);
}

TEST_F(HandGrabSystemWithWorld, AVillagerLetGoBeforeItComesFreeMakesTheDrawListAgainForTwoFrames)
{
	const auto villager = world->registry.Create();
	world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Villager>(villager);
	world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	world->underCursor = villager;
	EXPECT_TRUE(Press());
	Frame(10);
	ASSERT_FALSE(system->GetHeld().has_value());
	EXPECT_TRUE(world->drawListFrames.empty());
	// The player lets go of the button before the villager comes free
	Release();
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_EQ(world->drawListFrames, std::vector<uint32_t> {2u});
}

TEST_F(HandGrabSystemWithWorld, AQuickClickOnAVillagerMakesTheDrawListAgainForTwoFrames)
{
	const auto villager = world->registry.Create();
	world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Villager>(villager);
	world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	world->underCursor = villager;
	EXPECT_TRUE(Press());
	Release();
	EXPECT_EQ(world->drawListFrames, std::vector<uint32_t> {2u});
}

TEST_F(HandGrabSystemWithWorld, ABigForestLetGoBeforeItGivesATreeDoesNotMakeTheDrawListAgain)
{
	const auto forest = world->AddBigForest({0.0f, 0.0f, 0.0f});
	world->underCursor = forest;
	EXPECT_TRUE(Press());
	Frame(100);
	Release();
	EXPECT_TRUE(world->drawListFrames.empty());
}

TEST_F(HandGrabSystemWithWorld, ABigForestGivesATreeOnceTheButtonHasBeenHeldAWhile)
{
	const auto forest = world->AddBigForest({0.0f, 0.0f, 0.0f});
	world->underCursor = forest;
	EXPECT_TRUE(Press());
	// It isn't pulled at: the hand waits, as for a thing in flight
	EXPECT_FALSE(system->GetPullPose().has_value());
	Frame(100);
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_TRUE(world->treesGiven.empty());
	Frame(130);
	ASSERT_EQ(world->treesGiven.size(), 1u);
	const auto tree = world->treesGiven.front().second;
	EXPECT_EQ(world->treesGiven.front().first, forest);
	// The hand holds the tree, never the forest, and took it as it takes a standing tree: it creaks and costs alignment,
	// but leaves no hole
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(*system->GetHeld(), tree);
	EXPECT_TRUE(world->registry.AllOf<InHand>(tree));
	EXPECT_FALSE(world->registry.AllOf<InHand>(forest));
	EXPECT_EQ(world->leftWorld, std::vector<entt::entity> {tree});
	EXPECT_EQ(world->uprooted, std::vector<entt::entity> {tree});
	EXPECT_EQ(world->samples, std::vector<uint32_t> {32u});
	EXPECT_TRUE(world->holes.empty());
	ASSERT_EQ(world->reactions.size(), 1u);
	EXPECT_EQ(world->reactions.front().type, Reaction::ReactToHandPickUp);
}

TEST_F(HandGrabSystemWithWorld, AShortPressOnABigForestGivesNoTree)
{
	const auto forest = world->AddBigForest({0.0f, 0.0f, 0.0f});
	world->underCursor = forest;
	EXPECT_TRUE(Press());
	Frame(100);
	EXPECT_EQ(Release(), std::optional(forest));
	EXPECT_TRUE(world->treesGiven.empty());
	EXPECT_EQ(world->tapped, std::vector<entt::entity> {forest});
}

TEST_F(HandGrabSystemWithWorld, TheSpringTakesHoldTheFrameAfterThePressAndThrows)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	// Held at rest, the hand rises for the rock without lagging
	const auto held = Frame(10, {0.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(held.x, 0.0f);
	// Pressed again it makes ready to throw: this frame the hand is where it should be, then the spring drags it
	Press();
	const auto first = Frame(10, {5.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(first.x, 5.0f);
	const auto dragged = Frame(10, {10.0f, 0.0f, 0.0f});
	EXPECT_GT(dragged.x, 5.0f);
	EXPECT_LT(dragged.x, 10.0f);
	Release();
	ASSERT_EQ(world->released.size(), 1u);
	EXPECT_EQ(world->released.front().first, rock);
	EXPECT_GT(world->released.front().second.x, 0.0f);
	EXPECT_FALSE(system->GetHeld().has_value());
}

// A hand demonstration gives the hand the throw it recorded with each button: let go before the spring moves the hand
// again, the throw flies at the recorded velocity; once the spring has moved it, the spring's own throw counts again
TEST_F(HandGrabSystemWithWorld, ARecordedThrowStandsUntilTheSpringNextMovesTheHand)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	Frame(10);
	Press();
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	const glm::vec3 recorded {73.9941f, 5.2013f, -52.1485f};
	system->SetRecordedThrow(recorded);
	Release();
	ASSERT_EQ(world->released.size(), 1u);
	EXPECT_EQ(world->released.front().second, recorded);

	// Taken and made ready again, a recorded throw given before the spring moves is replaced by the spring's
	world->released.clear();
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	Frame(10);
	Press();
	system->SetRecordedThrow(recorded);
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	Release();
	ASSERT_EQ(world->released.size(), 1u);
	EXPECT_NE(world->released.front().second, recorded);
	EXPECT_GT(world->released.front().second.x, 0.0f);
}

TEST_F(HandGrabSystemWithWorld, LettingGoOntoTheLandCountsAsATurnForTheInfluenceKeptPastTheBorder)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	// Taking it up isn't using it on the land
	EXPECT_TRUE(world->usedOnLand.empty());
	world->underCursor.reset();
	Frame(10);
	// Let go out of the influence the hand keeps hold, and nothing counts
	Press();
	Frame(10);
	world->influence = false;
	Frame(10);
	Release();
	EXPECT_TRUE(system->GetHeld().has_value());
	EXPECT_TRUE(world->usedOnLand.empty());
	// Let go inside it counts once, as it is thrown
	world->influence = true;
	Frame(10);
	Press();
	Frame(10);
	Release();
	EXPECT_EQ(world->usedOnLand, std::vector<PlayerNames> {PlayerNames::PLAYER_ONE});
	ASSERT_EQ(world->released.size(), 1u);
}

TEST_F(HandGrabSystemWithWorld, APressOutsideTheInfluenceDoesNotMakeReadyToThrow)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(system->GetHeld().has_value());
	world->underCursor.reset();
	world->influence = false;
	Frame(10);
	Press();
	Frame(10);
	// Back inside before the button is let go: still held, not thrown
	world->influence = true;
	Frame(10);
	Release();
	EXPECT_TRUE(system->GetHeld().has_value());
	EXPECT_TRUE(world->released.empty());
	EXPECT_TRUE(world->usedOnLand.empty());
}

TEST_F(HandGrabSystemWithWorld, WhatIsLetGoGetsItsTwistAFifthOfASecondLater)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	Frame(10);
	Press();
	Frame(10);
	Release();
	world->bodies.insert(rock);
	Frame(100, {}, {1.0f, 0.0f, 0.0f});
	EXPECT_TRUE(world->twists.empty());
	Frame(100, {}, {1.0f, 0.0f, 0.0f});
	ASSERT_EQ(world->twists.size(), 1u);
	EXPECT_EQ(world->twists.front().first, rock);
}

TEST_F(HandGrabSystemWithWorld, TheTwistIsACommandThatTheTurnDoes)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	Frame(10);
	Press();
	Frame(10);
	Release();
	world->bodies.insert(rock);
	// The commands wait for their turn from here
	world->onCommand = [this](const player_commands::Action& action) { waiting.push_back(action); };
	Frame(100, {}, {1.0f, 0.0f, 0.0f});
	Frame(100, {}, {1.0f, 0.0f, 0.0f});
	EXPECT_TRUE(world->twists.empty());
	ASSERT_FALSE(waiting.empty());
	EXPECT_TRUE(std::holds_alternative<player_commands::HandTwist>(waiting.back()));
	RunWaiting();
	ASSERT_EQ(world->twists.size(), 1u);
	EXPECT_EQ(world->twists.front().first, rock);
}

TEST_F(HandGrabSystemWithWorld, ATakeOfAnotherComputerTakesWithNoPressOfThisHand)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	// A server, or a computer that did not press: its hand takes at the turn all the same
	system->DoTake(world->HandPlayer(), rock);
	ASSERT_EQ(system->GetHeld(), rock);
	// And lets go of what it holds, though its own hand never let go of the button
	system->DoLetGo(world->HandPlayer(), player_commands::HandLetGo {.object = rock, .velocity = glm::vec3(0.0f)});
	EXPECT_FALSE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, TheTurnHasAHeldThingAtTheReportedHandAtItsOwnTurnAndSize)
{
	const auto rock = world->AddRock({5.0f, 0.0f, 5.0f});
	system->DoTake(world->HandPlayer(), rock);
	ASSERT_EQ(system->GetHeld(), rock);
	auto& transform = world->registry.Get<Transform>(rock);
	const auto held = transform;
	// The frames draw it elsewhere, turned
	transform.position = {50.0f, 9.0f, 50.0f};
	transform.rotation = glm::mat3(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	system->PlaceHeldForTurn();
	EXPECT_EQ(transform.position, world->ReportedHandPlace());
	EXPECT_EQ(transform.rotation, held.rotation);
	EXPECT_EQ(transform.scale, held.scale);
}

TEST_F(HandGrabSystemWithWorld, TheTurnHasAHeldThingAtTheMapPositionOfTheReportedHand)
{
	const auto rock = world->AddRock({5.0f, 0.0f, 5.0f});
	system->DoTake(world->HandPlayer(), rock);
	ASSERT_EQ(system->GetHeld(), rock);
	world->heldPlaceStep = {0.25f, -0.5f, 0.125f};
	system->PlaceHeldForTurn();
	EXPECT_EQ(world->registry.Get<const Transform>(rock).position, world->ReportedHandPlace() + world->heldPlaceStep);
}

TEST_F(HandGrabSystemWithWorld, AVillagerOfTheHandsPlayerAlarmsThoseAboutIt)
{
	const auto villager = world->registry.Create();
	world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Villager>(villager);
	world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	world->underCursor = villager;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	EXPECT_EQ(world->villagersInHand, std::vector<entt::entity> {villager});
	const bool alarmed = std::ranges::any_of(world->reactions, [villager](const auto& reaction) {
		return reaction.type == Reaction::ReactToVillagerInHand && reaction.initiator == villager;
	});
	EXPECT_TRUE(alarmed);
}

TEST_F(HandGrabSystemWithWorld, WhatIsHeldIsHeatedEachTurnAndDroppedOnceGone)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	// A rock taken stops being its town's artefact
	EXPECT_EQ(world->artefactsTaken, std::vector<entt::entity> {rock});
	Turn();
	EXPECT_EQ(world->heated, std::vector<entt::entity> {rock});
	world->registry.Destroy(rock);
	Turn();
	EXPECT_FALSE(system->IsBusy());
}

TEST_F(HandGrabSystemWithWorld, WhatIsHeldOutsideTheInfluenceIsNotHeated)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	world->influence = false;
	Turn();
	EXPECT_TRUE(world->heated.empty());
}

TEST_F(HandGrabSystemWithWorld, AHandfulScoopedFromAPoisonedPileIsPoisoned)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile, .poisoned = true});
	world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_TRUE(world->registry.Get<const Pot>(*handful).poisoned);
}

TEST_F(HandGrabSystemWithWorld, APileIsScoopedIntoAHandfulThatGrowsWhileTheButtonIsHeld)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	world->underCursor = pile;
	EXPECT_TRUE(Press());
	// The first scoop is taken at once
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_EQ(world->registry.Get<const Pot>(pile).amount, 975u);
	EXPECT_EQ(world->streams, 1u);
	// The cursor is pinned while it scoops
	EXPECT_TRUE(world->cursorPinned);
	// Each game turn takes more, from 8 a turn
	Turn();
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
	Turn();
	EXPECT_GT(world->registry.Get<const Pot>(*handful).amount, 33u);
	EXPECT_EQ(world->scoopSounds.size(), 2u);
	// The scooping sound is asked for every turn but its loop starts only once
	EXPECT_EQ(world->scoopSoundStarts, 1u);
	EXPECT_EQ(world->scoopSoundStops, 0u);
	// The hand stays above the pile. Above the height of the pile, it rises at least 1.9 m.
	const auto hovering = Frame(10, {30.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(hovering.x, 0.0f);
	EXPECT_FLOAT_EQ(hovering.y, 3.0f + 1.9f);
	// A larger handful hangs further below the hand, and the hand rises by that distance
	world->sizes[*handful] = {.radius = 1.0f, .height = 4.0f};
	Frame(20, {30.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(Frame(30, {30.0f, 0.0f, 0.0f}).y, 3.0f + 0.7f * 4.0f);
	// Let go, the scoop ends, its stream stops and the cursor is free
	Release();
	EXPECT_EQ(world->stopped, std::vector<uint32_t> {1u});
	EXPECT_FALSE(world->cursorPinned);
	// Its sound stops with it, and isn't asked for again
	EXPECT_EQ(world->scoopSoundStops, 1u);
	EXPECT_FALSE(world->scoopSoundPlaying);
	Turn();
	EXPECT_EQ(world->scoopSounds.size(), 2u);
	EXPECT_TRUE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AHandfulPressedOntoAStoreGoesIntoIt)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 100, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->underCursor = pile;
	Press();
	Release();
	const auto handful = *system->GetHeld();
	const auto store = world->registry.Create();
	world->stores.insert(store);
	world->underCursor = store;
	EXPECT_TRUE(Press());
	EXPECT_FALSE(system->IsBusy());
	EXPECT_EQ(world->stored[store], 25u);
	EXPECT_EQ(world->usedUp, std::vector<entt::entity> {handful});
	EXPECT_TRUE(world->released.empty());
}

TEST_F(HandGrabSystemWithWorld, ATreePressedOntoAStoreGoesIntoItWhole)
{
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->underCursor = tree;
	Press();
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	Release();
	const auto store = world->registry.Create();
	world->stores.insert(store);
	world->underCursor = store;
	EXPECT_TRUE(Press());
	ASSERT_EQ(world->takenWhole.size(), 1u);
	EXPECT_EQ(world->takenWhole.front().first, store);
	EXPECT_EQ(world->takenWhole.front().second, tree);
	EXPECT_FALSE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AGateStonePressedOntoThePlinthIsLaidInIt)
{
	const auto stone = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->gateStones.insert(stone);
	world->underCursor = stone;
	Press();
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	Release();
	const auto plinth = world->registry.Create();
	world->plinths.insert(plinth);
	world->underCursor = plinth;
	EXPECT_TRUE(Press());
	ASSERT_EQ(world->laidStones.size(), 1u);
	EXPECT_EQ(world->laidStones.front().first, plinth);
	EXPECT_EQ(world->laidStones.front().second, stone);
	// The stone is used up, leaving its ghost, and the hand is empty
	EXPECT_EQ(world->usedUp, std::vector<entt::entity> {stone});
	EXPECT_TRUE(world->takenWhole.empty());
	EXPECT_FALSE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AVillagerPressedOntoATeleportStoneLeavesTheHandOnTheStone)
{
	const auto villager = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->underCursor = villager;
	Press();
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	Release();
	const auto stone = world->registry.Create();
	world->teleportStones.insert(stone);
	world->underCursor = stone;
	world->spreadAgain.clear();
	EXPECT_TRUE(Press());
	// The villager is out of the hand before the stone takes it, and its reactions spread again
	ASSERT_EQ(world->droppedOnStones.size(), 1u);
	EXPECT_EQ(std::get<0>(world->droppedOnStones.front()), stone);
	EXPECT_EQ(std::get<1>(world->droppedOnStones.front()), villager);
	EXPECT_FALSE(std::get<2>(world->droppedOnStones.front()));
	EXPECT_EQ(world->spreadAgain, std::vector<entt::entity> {villager});
	EXPECT_TRUE(world->usedUp.empty());
	EXPECT_FALSE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AScaffoldPressedOntoAScaffoldJoinsIt)
{
	const auto held = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->scaffolds.insert(held);
	world->underCursor = held;
	Press();
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	Release();
	const auto target = world->AddTree({20.0f, 0.0f, 0.0f}, 10.0f);
	world->scaffolds.insert(target);
	world->underCursor = target;
	EXPECT_TRUE(Press());
	// The hand lets go of the held scaffold before the join, and is empty after it
	ASSERT_EQ(world->joined.size(), 1u);
	EXPECT_EQ(std::get<0>(world->joined.front()), target);
	EXPECT_EQ(std::get<1>(world->joined.front()), held);
	EXPECT_FALSE(std::get<2>(world->joined.front()));
	EXPECT_TRUE(world->takenWhole.empty());
	EXPECT_FALSE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AnythingElsePressedOntoThePlinthIsMadeReadyToThrow)
{
	const auto tree = world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	world->underCursor = tree;
	Press();
	for (int i = 0; i < 40 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	Release();
	const auto plinth = world->registry.Create();
	world->plinths.insert(plinth);
	world->underCursor = plinth;
	EXPECT_TRUE(Press());
	EXPECT_TRUE(world->laidStones.empty());
	EXPECT_EQ(system->GetHeld(), std::optional<entt::entity> {tree});
}

TEST_F(HandGrabSystemWithWorld, APotLetGoOverTheLandCallsThePeopleAgain)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 100, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->underCursor = pile;
	Press();
	Release();
	const auto handful = *system->GetHeld();
	world->underCursor.reset();
	// Made ready and let go slowly, it is poured, having called the people to it first
	Press();
	Frame(10);
	Release();
	EXPECT_EQ(world->potReactionsSetUp, std::vector<entt::entity> {handful});
}

TEST_F(HandGrabSystemWithWorld, AThingLetGoBeforeItIsTakenIsStillTheLastLetGo)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	// The first frame of the pull never pulls, so the rock is still being taken
	Frame(200);
	EXPECT_FALSE(system->GetHeld().has_value());
	Release();
	EXPECT_EQ(world->registry.Get<const HandGrab>(world->hand).released, rock);
}

TEST_F(HandGrabSystemWithWorld, AScoopThatGetsNothingMoreStopsItsSoundButKeepsTheHandful)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 30, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	// The pile gives its last 5 the next turn, then has nothing more
	Turn();
	EXPECT_EQ(world->scoopSoundStarts, 1u);
	Turn();
	// Still holding the button, the sound has stopped and the hand keeps what it took
	EXPECT_FALSE(world->scoopSoundPlaying);
	EXPECT_GE(world->scoopSoundStops, 1u);
	const auto stops = world->scoopSoundStops;
	const auto asked = world->scoopSounds.size();
	Turn();
	Turn();
	EXPECT_EQ(world->scoopSounds.size(), asked);
	EXPECT_EQ(world->scoopSoundStops, stops);
	EXPECT_EQ(system->GetHeld(), handful);
}

TEST_F(HandGrabSystemWithWorld, AFieldAndAFishFarmSoundAsFoodWhileTheyAreScooped)
{
	const auto field = world->registry.Create();
	world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fields[field] = {.food = 1000, .ripe = false};
	world->underCursor = field;
	EXPECT_TRUE(Press());
	Turn();
	Turn();
	EXPECT_EQ(world->scoopSoundStarts, 1u);
	EXPECT_EQ(world->scoopSoundResources, (std::vector<ResourceType> {ResourceType::Food, ResourceType::Food}));
	Release();
	EXPECT_FALSE(world->scoopSoundPlaying);
}

TEST_F(HandGrabSystemWithWorld, ARipeFieldGivesHalfOfEachScoop)
{
	const auto field = world->registry.Create();
	world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fields[field] = {.food = 1000, .ripe = true};
	world->underCursor = field;
	EXPECT_TRUE(Press());
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	// The first scoop of 25 is halved, and so is each turn's 8
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 12u);
	EXPECT_EQ(world->fields[field].food, 988u);
	Turn();
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 16u);
}

TEST_F(HandGrabSystemWithWorld, TheHandLocksOntoAFieldOnlyWhenItsCropCanGiveFood)
{
	const auto field = world->registry.Create();
	world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fields[field] = {.food = 1000, .lockable = false};
	// A field is a building of its town, which takes taps
	world->tappable.insert(field);
	world->underCursor = field;
	EXPECT_FALSE(Press());
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_FALSE(system->GetLockedOn().has_value());
	EXPECT_EQ(world->fields[field].food, 1000u);
	// The press taps the field instead, as a building of its town
	EXPECT_EQ(std::ranges::count(world->tapped, field), 1);
}

TEST_F(HandGrabSystemWithWorld, TheLockOntoAScoopedThingHoldsWhileTheInfluenceHolds)
{
	const auto field = world->registry.Create();
	world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fields[field] = {.food = 1000};
	world->underCursor = field;
	EXPECT_TRUE(Press());
	EXPECT_EQ(system->GetLockedOn(), std::optional(field));
	Turn();
	EXPECT_EQ(system->GetLockedOn(), std::optional(field));
	// The player's influence goes from under the hand: the lock ends, and the hand keeps its handful
	world->influence = false;
	Turn();
	EXPECT_FALSE(system->GetLockedOn().has_value());
	EXPECT_TRUE(system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AFishFarmGivesAFirstHandfulForNothingThenItsFishAsTheScoopRampsUp)
{
	const auto farm = world->registry.Create();
	world->registry.Assign<Transform>(farm, glm::vec3(5.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fishFarms[farm] = 100.0f;
	world->underCursor = farm;
	EXPECT_TRUE(Press());
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	// The first 25 come out of nothing, and the fish leap from the farm into the hand
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_FLOAT_EQ(world->fishFarms[farm], 100.0f);
	ASSERT_EQ(world->fishStreams.size(), 1u);
	EXPECT_EQ(world->fishStreams.front(), glm::vec3(5.0f, 0.0f, 5.0f));
	// Each turn the ramp's fish are taken from the farm
	Turn();
	const auto first = world->registry.Get<const Pot>(*handful).amount - 25u;
	EXPECT_GT(first, 0u);
	EXPECT_FLOAT_EQ(world->fishFarms[farm], 100.0f - static_cast<float>(first));
	// A farm with fewer left than the ramp wants still gives all of it
	world->fishFarms[farm] = 1.0f;
	const auto before = world->registry.Get<const Pot>(*handful).amount;
	Turn();
	EXPECT_GT(world->registry.Get<const Pot>(*handful).amount - before, 1u);
	EXPECT_FLOAT_EQ(world->fishFarms[farm], 0.0f);
	// An empty farm ends the scoop: its stream stops and the hand keeps its handful
	EXPECT_TRUE(world->stopped.empty());
	Turn();
	EXPECT_FALSE(world->stopped.empty());
	EXPECT_FALSE(world->cursorPinned);
	EXPECT_EQ(system->GetHeld(), handful);
}

TEST_F(HandGrabSystemWithWorld, AScoopsStreamFollowsTheHandAndAPourEndsAfterThreeQuartersOfASecond)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->underCursor = pile;
	EXPECT_TRUE(Press());
	Frame(10);
	EXPECT_FALSE(world->streamPoints.empty());
	Release();
	world->underCursor.reset();
	// Made ready and let go slowly, the handful pours, and the pour stops after 0.75 s
	Press();
	Frame(10);
	Release();
	ASSERT_EQ(world->poured.size(), 1u);
	const auto stoppedBefore = world->stopped.size();
	Frame(700);
	EXPECT_EQ(world->stopped.size(), stoppedBefore);
	Frame(100);
	EXPECT_EQ(world->stopped.size(), stoppedBefore + 1);
}

class HandGrabSystemGiving: public HandGrabSystemWithWorld
{
protected:
	entt::entity HoldRock()
	{
		const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
		world->underCursor = rock;
		Press();
		for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
		{
			Frame(10);
		}
		Release();
		return rock;
	}

	entt::entity AddCreature(creature_giving::Verdict verdict)
	{
		const auto creature = world->registry.Create();
		world->registry.Assign<Transform>(creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		world->creatures[creature] = verdict;
		return creature;
	}
};

TEST_F(HandGrabSystemGiving, OverACreatureThatWouldTakeItTheHandOffersAndTurnsItOver)
{
	const auto rock = HoldRock();
	ASSERT_EQ(system->GetHeld(), rock);
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	world->underCursor = creature;
	Frame(16);
	EXPECT_EQ(system->GetOfferedCreature(), creature);
	// The palm turns over, eased towards the turn afresh every frame over 0.4 s, so it settles rather than arriving
	for (int i = 0; i < 120; ++i)
	{
		Frame(16);
	}
	EXPECT_NEAR(system->GetOfferTurn(), std::numbers::pi_v<float>, 1e-3f);
	// Off the creature the offer lasts 1.1 s, turning back once less than 0.3 s is left
	world->underCursor.reset();
	Frame(700);
	EXPECT_EQ(system->GetOfferedCreature(), creature);
	Frame(200);
	Frame(200);
	EXPECT_FALSE(system->GetOfferedCreature().has_value());
}

TEST_F(HandGrabSystemGiving, ThePressWhileOfferingGivesItToTheCreatureWithTheMatchingHand)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	world->underCursor = creature;
	Frame(16);
	// The cursor has moved off it, but the offer stands
	world->underCursor.reset();
	Frame(16);
	world->helpEvents.clear();
	// Given at the press, with no wait for the button to be held
	EXPECT_TRUE(Press());
	ASSERT_EQ(world->given.size(), 1u);
	// The help counts the gift once
	EXPECT_EQ(std::ranges::count(world->helpEvents, help::profile::k_Give), 1);
	EXPECT_EQ(world->given.front().creature, creature);
	EXPECT_EQ(world->given.front().held, rock);
	EXPECT_TRUE(world->given.front().leftHand);
	// Still held until the creature takes it, and not made ready to throw
	EXPECT_EQ(system->GetHeld(), rock);
	Release();
	EXPECT_TRUE(world->released.empty());
	// Taken, the hand holds nothing and offers nothing
	EXPECT_TRUE(system->HandOver(rock));
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_FALSE(world->registry.AllOf<InHand>(rock));
	EXPECT_FALSE(system->GetOfferedCreature().has_value());
	EXPECT_FALSE(system->HandOver(rock));
}

// The gift of a hand that is not the local one gives the thing with no count for the help
TEST_F(HandGrabSystemGiving, TheGiftOfAHandThatIsNotTheLocalOneCountsNothing)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	world->underCursor = creature;
	Frame(16);
	world->helpEvents.clear();
	world->localHand = false;
	EXPECT_TRUE(Press());
	ASSERT_EQ(world->given.size(), 1u);
	EXPECT_EQ(world->given.front().held, rock);
	EXPECT_TRUE(world->helpEvents.empty());
}

TEST_F(HandGrabSystemGiving, TheGiveOfThePressIsACommandThatWaitsForItsTurn)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	world->underCursor = creature;
	Frame(16);
	deferCommands = true;
	EXPECT_TRUE(Press());
	// The press sends the give at once; the creature gets the thing at the turn that does the command
	ASSERT_EQ(waiting.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandGive>(waiting.front()));
	EXPECT_EQ(std::get<player_commands::HandGive>(waiting.front()).creature, creature);
	EXPECT_EQ(std::get<player_commands::HandGive>(waiting.front()).object, rock);
	EXPECT_TRUE(world->given.empty());
	// Held down, the button sends no second give
	Frame(300);
	EXPECT_EQ(waiting.size(), 1u);
	RunWaiting();
	ASSERT_EQ(world->given.size(), 1u);
	EXPECT_EQ(world->given.front().creature, creature);
}

TEST_F(HandGrabSystemGiving, TheTurnGivesWithTheSideOfTheHandOfTheComputerThatSentTheGive)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	world->underCursor = creature;
	Frame(16);
	deferCommands = true;
	// The giver has a right hand
	world->rightHanded = true;
	EXPECT_TRUE(Press());
	ASSERT_EQ(waiting.size(), 1u);
	EXPECT_FALSE(std::get<player_commands::HandGive>(waiting.front()).leftHand);
	// Another computer, with a left hand, does the turn: the creature takes it with the giver's side
	world->rightHanded = false;
	RunWaiting();
	ASSERT_EQ(world->given.size(), 1u);
	EXPECT_EQ(world->given.front().held, rock);
	EXPECT_FALSE(world->given.front().leftHand);
}

TEST_F(HandGrabSystemGiving, WhileTheCreatureReachesTheHandMeetsItsHandAtTheMomentOfTaking)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	world->underCursor = creature;
	Frame(16);
	Press();
	Frame(230);
	world->takePoint = glm::vec3(5.0f, 3.0f, 0.0f);
	world->taking = hand_grab::HandGrabWorldInterface::Taking {.object = rock, .msFromTake = 0.0f};
	const auto atTake = Frame(16, {0.0f, 0.0f, 0.0f});
	// At the creature's hand, the held thing's hang below and the hand's rise above cancelling for a palm hold
	EXPECT_NEAR(atTake.x, 5.0f, 1e-4f);
	// Half a second from the take it is where it should be
	world->taking->msFromTake = -500.0f;
	const auto before = Frame(16, {0.0f, 0.0f, 0.0f});
	EXPECT_NEAR(before.x, 0.0f, 1e-4f);
}

TEST_F(HandGrabSystemGiving, MovingTheHandFifteenSizesFromTheReachingCreatureCallsItOff)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	world->underCursor = creature;
	Frame(16);
	Press();
	Frame(230);
	world->underCursor.reset();
	world->takePoint = glm::vec3(0.0f, 0.0f, 0.0f);
	// It is measured from where the hand was drawn the frame before
	Frame(16, {29.0f, 0.0f, 0.0f});
	Frame(16, {29.0f, 0.0f, 0.0f});
	EXPECT_TRUE(world->calledOff.empty());
	Frame(16, {40.0f, 0.0f, 0.0f});
	Frame(16, {40.0f, 0.0f, 0.0f});
	EXPECT_EQ(world->calledOff, std::vector<entt::entity> {creature});
	EXPECT_FALSE(system->GetOfferedCreature().has_value());
}

TEST_F(HandGrabSystemGiving, TheTurnCallsOffTheGiftThatTheHandSentOneTime)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	world->underCursor = creature;
	Frame(16);
	Press();
	Frame(230);
	world->underCursor.reset();
	world->takePoint = glm::vec3(0.0f, 0.0f, 0.0f);
	deferCommands = true;
	Frame(16, {40.0f, 0.0f, 0.0f});
	Frame(16, {40.0f, 0.0f, 0.0f});
	Frame(16, {40.0f, 0.0f, 0.0f});
	// The frames send the call off one time, and do not call off the creature or forget the gift
	const auto callOffs = std::ranges::count_if(waiting, [](const player_commands::Action& action) {
		return std::holds_alternative<player_commands::GiveCallOff>(action);
	});
	EXPECT_EQ(callOffs, 1);
	EXPECT_TRUE(world->calledOff.empty());
	EXPECT_TRUE(system->GetOfferedCreature().has_value());
	// The turn calls it off
	RunWaiting();
	EXPECT_EQ(world->calledOff, std::vector<entt::entity> {creature});
	EXPECT_FALSE(system->GetOfferedCreature().has_value());
}

TEST_F(HandGrabSystemGiving, ACreatureThatCantPickItUpSaysSoAndIsNotOffered)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::CantPickUp);
	world->underCursor = creature;
	Frame(16);
	EXPECT_EQ(world->cantPickUp, std::vector<entt::entity> {creature});
	EXPECT_FALSE(system->GetOfferedCreature().has_value());
	Press();
	Frame(230);
	EXPECT_TRUE(world->given.empty());
}

// The hand forgets the last thing it took and the last thing it let go of once that thing is gone
TEST_F(HandGrabSystemWithWorld, TheLastThingsTakenAndLetGoAreForgottenWhenTheyGo)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	EXPECT_TRUE(Press());
	Release();
	auto& grab = world->registry.Get<HandGrab>(world->hand);
	const auto taken = world->AddRock({2.0f, 0.0f, 0.0f});
	const auto dropped = world->AddRock({4.0f, 0.0f, 0.0f});
	grab.lastPickedUp = taken;
	grab.lastDropped = dropped;
	Turn();
	EXPECT_EQ(world->registry.Get<HandGrab>(world->hand).lastPickedUp, taken);
	EXPECT_EQ(world->registry.Get<HandGrab>(world->hand).lastDropped, dropped);
	world->registry.Destroy(taken);
	world->registry.Destroy(dropped);
	Turn();
	EXPECT_EQ(world->registry.Get<HandGrab>(world->hand).lastPickedUp, entt::entity {entt::null});
	EXPECT_EQ(world->registry.Get<HandGrab>(world->hand).lastDropped, entt::entity {entt::null});
}

TEST_F(HandGrabSystemWithWorld, TheHandShowsWhatItHoldsAfterEachTurnOfAScoopAndAtTheLetGoOfAPileOrAFishFarm)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->underCursor = pile;
	// Nothing as the scoop starts
	EXPECT_TRUE(Press());
	EXPECT_FALSE(system->TakeAmountToolTip().has_value());
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	// Each turn that it takes some, what it holds now, once
	Turn();
	EXPECT_EQ(system->TakeAmountToolTip(), std::optional(world->registry.Get<const Pot>(*handful).amount));
	EXPECT_FALSE(system->TakeAmountToolTip().has_value());
	// At the let go too
	Release();
	EXPECT_EQ(system->TakeAmountToolTip(), std::optional(world->registry.Get<const Pot>(*handful).amount));
}

TEST_F(HandGrabSystemWithWorld, AFieldAndAFishFarmShowWhatTheHandHoldsEachTurnButOnlyAFishFarmAtTheLetGo)
{
	// The field of another player
	const auto field = world->registry.Create();
	world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fields[field] = {.food = 1000, .ripe = false};
	world->owners[field] = PlayerNames::PLAYER_TWO;
	world->underCursor = field;
	EXPECT_TRUE(Press());
	EXPECT_FALSE(system->TakeAmountToolTip().has_value());
	const auto fromField = system->GetHeld();
	ASSERT_TRUE(fromField.has_value());
	Turn();
	EXPECT_EQ(system->TakeAmountToolTip(), std::optional(world->registry.Get<const Pot>(*fromField).amount));
	Release();
	EXPECT_FALSE(system->TakeAmountToolTip().has_value());
	system->HandOver(*fromField);

	const auto farm = world->registry.Create();
	world->registry.Assign<Transform>(farm, glm::vec3(5.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fishFarms[farm] = 100.0f;
	world->underCursor = farm;
	EXPECT_TRUE(Press());
	EXPECT_FALSE(system->TakeAmountToolTip().has_value());
	const auto fromFarm = system->GetHeld();
	ASSERT_TRUE(fromFarm.has_value());
	Turn();
	EXPECT_EQ(system->TakeAmountToolTip(), std::optional(world->registry.Get<const Pot>(*fromFarm).amount));
	Release();
	EXPECT_EQ(system->TakeAmountToolTip(), std::optional(world->registry.Get<const Pot>(*fromFarm).amount));
}

// The let-go of a scoop from a field of the local player shows what the hand holds, as the hand lets go. A field of no
// town does not.
TEST_F(HandGrabSystemWithWorld, AFieldOfTheLocalPlayerShowsWhatTheHandHoldsAtTheLetGo)
{
	static_assert(hand_grab::FieldShowsHeldAtLetGo(true, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	static_assert(!hand_grab::FieldShowsHeldAtLetGo(true, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	static_assert(!hand_grab::FieldShowsHeldAtLetGo(true, std::nullopt, PlayerNames::PLAYER_ONE));
	static_assert(!hand_grab::FieldShowsHeldAtLetGo(false, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	const auto field = world->registry.Create();
	world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fields[field] = {.food = 1000, .ripe = false};
	world->underCursor = field;
	EXPECT_TRUE(Press());
	const auto fromField = system->GetHeld();
	ASSERT_TRUE(fromField.has_value());
	Turn();
	EXPECT_TRUE(system->TakeAmountToolTip().has_value());
	Release();
	EXPECT_EQ(system->TakeAmountToolTip(), std::optional(world->registry.Get<const Pot>(*fromField).amount));
	EXPECT_FALSE(system->TakeAmountToolTip().has_value());
	system->HandOver(*fromField);

	const auto noTown = world->registry.Create();
	world->registry.Assign<Transform>(noTown, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->fields[noTown] = {.food = 1000, .ripe = false};
	world->owners[noTown] = std::nullopt;
	world->underCursor = noTown;
	EXPECT_TRUE(Press());
	ASSERT_TRUE(system->GetHeld().has_value());
	Turn();
	EXPECT_TRUE(system->TakeAmountToolTip().has_value());
	Release();
	EXPECT_FALSE(system->TakeAmountToolTip().has_value());
}

TEST_F(HandGrabSystemWithWorld, AHeldThingThatIsDestroyedLeavesTheHandAtTheNextTurn)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(system->GetHeld(), std::optional(rock));
	world->registry.Destroy(rock);
	Turn();
	EXPECT_FALSE(system->GetHeld().has_value());
}

// The hand sends the take as a command, with its pick-up sound. The thing stays on the land until the turn that does
// the command, and the hand keeps its grip on it until then.
TEST_F(HandGrabSystemWithWorld, ATakeWaitsForTheTurnThatDoesItsCommand)
{
	deferCommands = true;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && world->sent.empty(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(world->sent.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandTake>(world->sent.front()));
	EXPECT_EQ(std::get<player_commands::HandTake>(world->sent.front()).object, rock);
	EXPECT_FALSE(world->samples.empty());
	EXPECT_FALSE(world->registry.AllOf<InHand>(rock));
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_TRUE(system->IsBusy());
	// More frames send no second take
	Frame(10);
	EXPECT_EQ(world->sent.size(), 1u);
	RunWaiting();
	EXPECT_EQ(system->GetHeld(), std::optional(rock));
	EXPECT_TRUE(world->registry.AllOf<InHand>(rock));
}

// The hand lets go of the thing as a command, with the velocity that it had when the player let go of the button. It
// holds the thing until the turn that does the command.
TEST_F(HandGrabSystemWithWorld, ALetGoWaitsForItsTurnWithTheVelocityOfTheRelease)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(system->GetHeld(), std::optional(rock));
	deferCommands = true;
	world->sent.clear();
	Press();
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	Release();
	ASSERT_EQ(world->sent.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandLetGo>(world->sent.front()));
	const auto letGo = std::get<player_commands::HandLetGo>(world->sent.front());
	EXPECT_EQ(letGo.object, rock);
	EXPECT_GT(letGo.velocity.x, 0.0f);
	EXPECT_TRUE(world->released.empty());
	EXPECT_EQ(system->GetHeld(), std::optional(rock));
	EXPECT_FALSE(system->IsAboutToThrow());
	// A press before the turn does nothing more
	EXPECT_TRUE(Press());
	Release();
	EXPECT_EQ(world->sent.size(), 1u);
	RunWaiting();
	ASSERT_EQ(world->released.size(), 1u);
	EXPECT_EQ(world->released.front().first, rock);
	EXPECT_EQ(world->released.front().second, letGo.velocity);
	EXPECT_FALSE(system->GetHeld().has_value());
}

// The release asks the world where the thing starts, as this computer drew it, and the command takes the start. The
// turn that does the command puts the thing at the start and lets it go with the spin of the start. The hand does not
// ask again at the turn.
TEST_F(HandGrabSystemWithWorld, ALetGoTakesTheStartOfTheReleaseToItsTurn)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(system->GetHeld(), std::optional(rock));
	world->throwStart = {
	    .angularMomentum = {0.5f, -0.0f, 1.0e-40f}, .origin = {1.0f, 2.0f, 3.0f}, .angles = {0.25f, -1.5f, 3.0f}};
	deferCommands = true;
	world->sent.clear();
	Press();
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	Release();
	ASSERT_EQ(world->sent.size(), 1u);
	const auto letGo = std::get<player_commands::HandLetGo>(world->sent.front());
	ASSERT_EQ(world->startsAsked.size(), 1u);
	EXPECT_EQ(world->startsAsked.front().first, rock);
	// A fast throw does not lie along the slope
	EXPECT_FALSE(world->startsAsked.front().second);
	EXPECT_EQ(letGo.start.origin, world->throwStart.origin);
	EXPECT_EQ(letGo.start.angles, world->throwStart.angles);
	EXPECT_TRUE(world->startedFrom.empty());
	RunWaiting();
	EXPECT_EQ(world->startsAsked.size(), 1u);
	ASSERT_EQ(world->startedFrom.size(), 1u);
	EXPECT_EQ(world->startedFrom.front().first, rock);
	EXPECT_EQ(world->startedFrom.front().second.origin, world->throwStart.origin);
	EXPECT_EQ(world->startedFrom.front().second.angles, world->throwStart.angles);
	ASSERT_EQ(world->releasedSpins.size(), 1u);
	EXPECT_EQ(std::bit_cast<uint32_t>(world->releasedSpins.front().y), 0x80000000U);
	EXPECT_EQ(world->releasedSpins.front().x, 0.5f);
}

// The release asks for as many turns of lead as the reports come late. The command takes the velocity after the lead.
// Until the turn of the command, the frames draw the thing along the flight from the turn of the send.
TEST_F(HandGrabSystemWithWorld, AThrowLeadsByTheLatencyOfTheReports)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(system->GetHeld(), std::optional(rock));
	world->reportLatencyMs = 250;
	world->ledVelocity = glm::vec3(7.0f, -3.0f, 2.0f);
	world->flight = hand_grab::throw_lead::Flight {
	    .places = {{.axes = glm::mat3(1.0f), .centre = {0.0f, 5.0f, 0.0f}},
	               {.axes = glm::mat3(1.0f), .centre = {2.0f, 6.0f, 0.0f}},
	               {.axes = glm::mat3(1.0f), .centre = {4.0f, 6.0f, 0.0f}}},
	};
	world->frameTurn = 30;
	deferCommands = true;
	world->sent.clear();
	Press();
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	Release();
	ASSERT_EQ(world->leadsAsked.size(), 1U);
	EXPECT_EQ(world->leadsAsked.front(), 2);
	ASSERT_EQ(world->sent.size(), 1U);
	const auto letGo = std::get<player_commands::HandLetGo>(world->sent.front());
	EXPECT_EQ(letGo.velocity, *world->ledVelocity);
	// The frames draw the thing on the flight: half of the way through the turn after the send
	world->frameTurn = 31;
	world->frameTurnFraction = 0.5f;
	system->DrawThrownForFrame();
	ASSERT_EQ(world->drawnThrown.size(), 1U);
	EXPECT_EQ(world->drawnThrown.front().first, rock);
	EXPECT_NEAR(world->drawnThrown.front().second.x, 3.0f, 1.0e-4f);
	EXPECT_NEAR(world->drawnThrown.front().second.y, 6.0f, 1.0e-4f);
	// The turn of the command lets go, and the frames draw the thing no more
	RunWaiting();
	world->drawnThrown.clear();
	system->DrawThrownForFrame();
	EXPECT_TRUE(world->drawnThrown.empty());
}

// A hand that must let go puts the thing down from where it is: it asks for no start and goes to none
TEST_F(HandGrabSystemWithWorld, AForcedDropHasNoStart)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(system->GetHeld(), std::optional(rock));
	system->ForceDrop();
	EXPECT_TRUE(world->startsAsked.empty());
	EXPECT_TRUE(world->startedFrom.empty());
	ASSERT_EQ(world->releasedSpins.size(), 1u);
	EXPECT_EQ(world->releasedSpins.front(), glm::vec3(0.0f));
}

// A short press sends the tap. The world taps the thing at the turn that does the command.
TEST_F(HandGrabSystemWithWorld, ATapWaitsForTheTurnThatDoesItsCommand)
{
	deferCommands = true;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	Frame(50);
	Release();
	EXPECT_TRUE(world->tapped.empty());
	// The tap and the click of the thing under the hand go to the turn
	ASSERT_EQ(world->sent.size(), 2u);
	EXPECT_EQ(std::ranges::count_if(
	              world->sent, [](const auto& action) { return std::holds_alternative<player_commands::HandTap>(action); }),
	          1);
	EXPECT_EQ(std::ranges::count_if(
	              world->sent, [](const auto& action) { return std::holds_alternative<player_commands::HandClick>(action); }),
	          1);
	RunWaiting();
	ASSERT_EQ(world->tapped.size(), 1u);
	EXPECT_EQ(world->tapped.front(), rock);
}

// The turn does the tap at the place of the hand that the last report gave, not where the game draws the hand
TEST_F(HandGrabSystemWithWorld, ATapUsesThePlaceOfTheHandThatTheLastReportGave)
{
	deferCommands = true;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	Frame(50);
	Release();
	ASSERT_TRUE(world->tapped.empty());
	// A report comes before the turn of the tap. The game draws the hand at a different place.
	const glm::vec3 reported {12.0f, 3.0f, -34.0f};
	world->reportedPlaces[world->hand] = reported;
	ASSERT_NE(world->PoseOf(world->hand).origin, reported);
	RunWaiting();
	ASSERT_EQ(world->tapPlaces.size(), 1u);
	EXPECT_EQ(world->tapPlaces.front(), reported);
}

// The hand's work of the turn takes a thing in flight once the button was held long enough, also with no frame between
TEST_F(HandGrabSystemWithWorld, TheTurnWorkOfTheHandTakesAFlyingThingAfterTheWait)
{
	deferCommands = true;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->flying.insert(rock);
	world->underCursor = rock;
	Press();
	system->InterfaceTurn(now + 100, (now + 100) / 100);
	EXPECT_TRUE(world->sent.empty());
	system->InterfaceTurn(now + 300, (now + 300) / 100);
	ASSERT_EQ(world->sent.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<player_commands::HandTake>(world->sent.front()));
	RunWaiting();
	EXPECT_EQ(system->GetHeld(), std::optional(rock));
}

// A new land clears the hand: what it held is let go
TEST_F(HandGrabSystemWithWorld, ANewLandLetsGoOfWhatTheHandHeld)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	system->DoTake(world->HandPlayer(), rock);
	ASSERT_EQ(system->GetHeld(), std::optional(rock));
	system->Reset();
	EXPECT_FALSE(system->GetHeld().has_value());
}

// A take that the hand no longer waits for does nothing at its turn
// A new land clears the hand and its thing goes with the old land: the take that waits does nothing
TEST_F(HandGrabSystemWithWorld, ATakeOfAThingThatWentWithTheLandDoesNothing)
{
	deferCommands = true;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->flying.insert(rock);
	world->underCursor = rock;
	Press();
	system->InterfaceTurn(now + 300, (now + 300) / 100);
	ASSERT_EQ(world->sent.size(), 1u);
	system->Reset();
	world->registry.Destroy(rock);
	RunWaiting();
	EXPECT_FALSE(system->GetHeld().has_value());
}

// A command of another player does not move this hand
TEST_F(HandGrabSystemWithWorld, TheHandDoesOnlyTheCommandsOfItsOwnPlayer)
{
	deferCommands = true;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->flying.insert(rock);
	world->underCursor = rock;
	Press();
	system->InterfaceTurn(now + 300, (now + 300) / 100);
	system->DoTake(PlayerNames::PLAYER_TWO, rock);
	EXPECT_FALSE(system->GetHeld().has_value());
	RunWaiting();
	EXPECT_EQ(system->GetHeld(), std::optional(rock));
}

TEST_F(HandGrabSystemWithWorld, APressOnAPileSendsTheLockAndTheTurnStartsTheScoop)
{
	deferCommands = true;
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->underCursor = pile;
	// The press sends the lock and takes nothing from the pile
	EXPECT_TRUE(Press());
	ASSERT_EQ(waiting.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandLock>(waiting.front()));
	EXPECT_EQ(std::get<player_commands::HandLock>(waiting.front()).object, pile);
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_EQ(world->registry.Get<const Pot>(pile).amount, 1000u);
	// Let go before the turn, the end of the lock follows the lock
	Release();
	ASSERT_EQ(waiting.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandUnlock>(waiting.back()));
	EXPECT_EQ(std::get<player_commands::HandUnlock>(waiting.back()).object, pile);
	// The turn takes the first handful, then ends the scoop: the hand keeps its handful
	RunWaiting();
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_EQ(world->registry.Get<const Pot>(pile).amount, 975u);
	Turn();
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_EQ(system->TakeAmountToolTip(), std::optional(25u));
}

TEST_F(HandGrabSystemWithWorld, TheLockAndItsEndOfAnotherPlayerScoopWithThatPlayersHand)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->secondPlayerHasHands = true;
	system->DoLock(PlayerNames::PLAYER_TWO, pile);
	// The second player's hand holds the handful, and the first player's hand holds nothing
	EXPECT_FALSE(system->GetHeld().has_value());
	const auto* second = world->registry.TryGet<const HandGrab>(world->secondHand);
	ASSERT_NE(second, nullptr);
	ASSERT_TRUE(world->registry.Valid(second->object));
	const auto handful = second->object;
	EXPECT_EQ(world->registry.Get<const Pot>(handful).amount, 25u);
	EXPECT_EQ(second->scoopSource, pile);
	// The end of the lock of the first player leaves the second player's scoop
	system->DoUnlock(PlayerNames::PLAYER_ONE, pile);
	EXPECT_EQ(world->registry.Get<const HandGrab>(world->secondHand).scoopSource, pile);
	system->DoUnlock(PlayerNames::PLAYER_TWO, pile);
	EXPECT_TRUE(world->registry.Get<const HandGrab>(world->secondHand).scoopSource == entt::null);
	EXPECT_FALSE(world->acting.has_value());
}

// The questions of this computer are about the hand of its own god; the turn asks about the hand of a player by name
TEST_F(HandGrabSystemWithWorld, ThisComputerAsksAboutTheHandOfItsOwnGod)
{
	world->networkGame = true;
	world->secondPlayerHasHands = true;
	world->ownHandPlayer = PlayerNames::PLAYER_TWO;
	const auto first = world->AddRock({0.0f, 0.0f, 0.0f});
	system->DoTake(PlayerNames::PLAYER_ONE, first);
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	system->DoLock(PlayerNames::PLAYER_TWO, pile);
	const auto handful = world->registry.Get<const HandGrab>(world->secondHand).object;
	EXPECT_EQ(system->GetHeld(), std::optional(handful));
	EXPECT_EQ(system->GetLockedOn(), std::optional(pile));
	EXPECT_EQ(system->GetHeldOf(PlayerNames::PLAYER_ONE), std::optional(first));
	EXPECT_FALSE(system->GetLockedOnOf(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_EQ(system->GetLockedOnOf(PlayerNames::PLAYER_TWO), std::optional(pile));
	// The questions leave no acting player behind them
	EXPECT_FALSE(world->acting.has_value());
}

// A creature takes a gift from the hand that holds it, whichever god's hand that is
TEST_F(HandGrabSystemWithWorld, ACreatureTakesTheGiftFromTheHandOfTheGodThatHoldsIt)
{
	world->secondPlayerHasHands = true;
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	system->DoTake(PlayerNames::PLAYER_TWO, rock);
	ASSERT_EQ(system->GetHeldOf(PlayerNames::PLAYER_TWO), std::optional(rock));
	EXPECT_TRUE(system->HandOver(rock));
	EXPECT_FALSE(system->GetHeldOf(PlayerNames::PLAYER_TWO).has_value());
	EXPECT_FALSE(world->acting.has_value());
}

TEST_F(HandGrabSystemWithWorld, TheEndOfTheLockOfAnotherThingLeavesTheScoop)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	system->DoUnlock(world->HandPlayer(), *handful);
	Turn();
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
}

TEST_F(HandGrabSystemWithWorld, TheSeedInTheHandOfAnotherGodIsDrawnAtTheHandItReported)
{
	auto& registry = world->registry;
	const auto seed = registry.Create();
	registry.Assign<Transform>(seed, glm::vec3(1.0f, 2.0f, 3.0f), glm::mat3(2.0f), glm::vec3(1.0f));
	registry.Assign<HandHeldSeed>(world->secondHand, HandHeldSeed {.seed = seed});
	const auto ownSeed = registry.Create();
	registry.Assign<Transform>(ownSeed, glm::vec3(7.0f, 8.0f, 9.0f), glm::mat3(2.0f), glm::vec3(1.0f));
	registry.Assign<HandHeldSeed>(world->hand, HandHeldSeed {.seed = ownSeed});
	world->otherHands.insert(world->secondHand);
	world->reportedPlaces[world->secondHand] = glm::vec3(40.0f, 5.0f, 60.0f);
	system->PlaceOthersHeldForFrame(20);
	// Upright at the reported hand, as another god's held thing is
	const auto& placed = registry.Get<const Transform>(seed);
	EXPECT_EQ(placed.position, glm::vec3(40.0f, 5.0f, 60.0f));
	EXPECT_EQ(placed.rotation, glm::mat3(1.0f));
	// The seed of the own hand is where the hand's pose puts it
	EXPECT_EQ(registry.Get<const Transform>(ownSeed).position, glm::vec3(7.0f, 8.0f, 9.0f));
}

// The scoop continues in the input of the turn, and the hand step of the turn takes no handful
TEST_F(HandGrabSystemWithWorld, TheScoopContinuesInTheInputOfTheTurnAndNotInTheHandStep)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	system->ProcessTurn();
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u);
	system->LockTurn(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
	// Out of the influence, the scoop stops in the input of the turn
	world->influence = false;
	system->LockTurn(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
	EXPECT_FALSE(world->cursorPinned);
	EXPECT_FALSE(world->acting.has_value());
}

// The scoop of another god continues with the hand of that god, and the first player's hand stays empty
TEST_F(HandGrabSystemWithWorld, TheScoopOfAnotherGodContinuesWithThatGodsHand)
{
	const auto pile = world->registry.Create();
	world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	world->secondPlayerHasHands = true;
	system->DoLock(PlayerNames::PLAYER_TWO, pile);
	const auto handful = world->registry.Get<const HandGrab>(world->secondHand).object;
	system->LockTurn(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(world->registry.Get<const Pot>(handful).amount, 25u);
	system->LockTurn(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(world->registry.Get<const Pot>(handful).amount, 25u + 8u);
	EXPECT_FALSE(system->GetHeld().has_value());
	EXPECT_FALSE(world->acting.has_value());
}

// A held thing that goes leaves the hand in the input of the next turn
TEST_F(HandGrabSystemWithWorld, AHeldThingThatGoesLeavesTheHandInTheInputOfTheTurn)
{
	const auto rock = world->AddRock({0.0f, 0.0f, 0.0f});
	world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(system->GetHeld().has_value());
	world->registry.Destroy(rock);
	system->LockTurn(PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(system->IsBusy());
}
