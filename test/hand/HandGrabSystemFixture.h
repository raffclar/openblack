/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <bit>
#include <functional>
#include <map>
#include <numbers>
#include <ranges>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Animals/FishFarmRules.h"
#include "ECS/AltarSacrifice.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandHeldSeed.h"
#include "ECS/Components/HeldDrawTrack.h"
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

namespace hand_grab_test
{
using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

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

	entt::entity AddCrop(glm::vec3 position)
	{
		const auto crop = registry.Create();
		registry.Assign<Transform>(crop, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<MobileObject>(crop, MobileObject {.type = MobileObjectInfo::Crop});
		sizes[crop] = {.radius = 1.0f, .height = 2.0f};
		crops.insert(crop);
		return crop;
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
	[[nodiscard]] std::optional<glm::vec3> MindTurnPlaceOf(entt::entity of) const override
	{
		const auto found = mindTurnPlaces.find(of);
		return found != mindTurnPlaces.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> MindDrawnPlaceOf(entt::entity of) const override
	{
		const auto found = mindDrawnPlaces.find(of);
		return found != mindDrawnPlaces.end() ? std::optional(found->second) : std::nullopt;
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
	void CatchFirefly(entt::entity object) override { fireflyCatches.push_back(object); }
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
	[[nodiscard]] bool SendWaits() const override { return sendWaits; }
	void MarkSend() override { ++sendMarks; }
	void EndInterfaceLock(PlayerNames /*player*/) override { ++lockEnds; }
	/// The hand of this computer waits for its last send, and how many sends started the wait
	bool sendWaits {false};
	int sendMarks {0};
	int lockEnds {0};
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
	[[nodiscard]] Pose PullStartPoseOf(entt::entity object) const override
	{
		pullStarts.push_back(object);
		const auto found = pullStartPoses.find(object);
		return found != pullStartPoses.end() ? found->second : PoseOf(object);
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
	[[nodiscard]] bool GivesInItsPlace(entt::entity object) const override { return crops.contains(object); }
	entt::entity GiveInItsPlace(entt::entity object) override
	{
		const auto position = registry.Get<const Transform>(object).position;
		crops.erase(object);
		registry.Destroy(object);
		return MakeHandful(PotInfo::HandFood, position, cropFood, false);
	}
	/// The crops, which give a handful of food in their place, and the food in that handful
	std::set<entt::entity> crops;
	uint32_t cropFood {16};
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
	void HelpEvent(uint32_t event) override { usedHelpEvents.push_back(event); }
	std::vector<uint32_t> usedHelpEvents;
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
	/// The hands that a computer mind plays: where the input of the turn puts each, and where the frames draw it
	std::map<entt::entity, glm::vec3> mindTurnPlaces;
	std::map<entt::entity, glm::vec3> mindDrawnPlaces;
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
	std::vector<entt::entity> fireflyCatches;
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
	/// The things that a pull started from, with the pose that the world gave for each start
	mutable std::vector<entt::entity> pullStarts;
	std::map<entt::entity, Pose> pullStartPoses;
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

class HandGrabSystemWithWorld: public ::testing::Test
{
protected:
	HandGrabSystemWithWorld()
	{
		auto owned = std::make_unique<FakeWorld>();
		_world = owned.get();
		_system = std::make_unique<HandGrabSystem>(std::move(owned));
		// The world does each command of the hand at once, unless a test keeps the commands for a later turn
		_world->onCommand = [this](const player_commands::Action& action) {
			if (_deferCommands)
			{
				_waiting.push_back(action);
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
		_system->LockTurn(PlayerNames::PLAYER_ONE);
		_system->ProcessTurn();
	}

	/// The world does one command of the hand's player
	void Run(const player_commands::Action& action)
	{
		const auto player = _world->HandPlayer();
		std::visit(
		    [this, player](const auto& command) {
			    using Kind = std::decay_t<decltype(command)>;
			    if constexpr (std::is_same_v<Kind, player_commands::HandTake>)
			    {
				    _system->DoTake(player, command.object);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandLetGo>)
			    {
				    _system->DoLetGo(player, command);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandTap>)
			    {
				    _system->DoTap(player, command.object);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandGive>)
			    {
				    _system->DoGive(player, command.creature, command.object, command.leftHand);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandTwist>)
			    {
				    _system->DoTwist(player, command.object, command.moved);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandLock>)
			    {
				    _system->DoLock(player, command.object);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandUnlock>)
			    {
				    _system->DoUnlock(player, command.object);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandApply>)
			    {
				    _system->DoApply(player, command.target);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::HandClick>)
			    {
				    _system->DoClick(player, command, _commandTurn);
			    }
			    else if constexpr (std::is_same_v<Kind, player_commands::GiveCallOff>)
			    {
				    _system->DoGiveCallOff(player, command.creature);
			    }
		    },
		    action);
	}

	/// The turn at which the world does the commands
	uint32_t _commandTurn {0};

	/// The start of a turn: the world does the commands that wait
	void RunWaiting()
	{
		const auto commands = std::exchange(_waiting, {});
		for (const auto& command : commands)
		{
			Run(command);
		}
	}

	glm::vec3 Frame(uint32_t gameMs, glm::vec3 target = {0.0f, 0.0f, 0.0f}, glm::vec3 ground = {0.0f, 0.0f, 0.0f})
	{
		_now += gameMs;
		return _system->UpdateFrame({.target = target,
		                             .rayOrigin = target + glm::vec3(0.0f, 50.0f, 0.0f),
		                             .rayDirection = {0.0f, -1.0f, 0.0f},
		                             .cursorGround = ground,
		                             .handSize = 1.0f,
		                             .seconds = static_cast<float>(gameMs) * 0.001f,
		                             .gameMs = gameMs,
		                             .nowMs = _now,
		                             .turn = _now / 100});
	}

	bool Press() { return _system->Press(_now, _now / 100); }
	std::optional<entt::entity> Release() { return _system->Release(_now, _now / 100); }

	FakeWorld* _world {nullptr};
	std::unique_ptr<HandGrabSystem> _system;
	uint32_t _now {1000};
	/// The commands wait for a turn, as in the game, when the test asks for it
	bool _deferCommands {false};
	std::vector<player_commands::Action> _waiting;
};

class HandGrabShortTreePress: public HandGrabSystemWithWorld
{
protected:
	/// A press of eight frames of 20 ms on a light tree, with the hand far above its grip. Gives the thing that the hand
	/// holds after the turn that does the commands.
	std::optional<entt::entity> PressEightFrames(entt::entity tree)
	{
		_deferCommands = true;
		_world->underCursor = tree;
		EXPECT_TRUE(Press());
		// The hand fades into its pull for 0.13 s, and the pull starts on the frame after the fade
		for (int i = 0; i < 7; ++i)
		{
			Frame(20, {0.0f, 50.0f, 0.0f});
			EXPECT_TRUE(_world->holes.empty()) << "frame " << i;
		}
		// The first frame of the pull pulls harder than the tree weighs, thus the tree comes free at once
		Frame(20, {0.0f, 50.0f, 0.0f});
		EXPECT_EQ(_world->holes, std::vector<entt::entity> {tree});
		// The button comes up after the pull freed the tree: the hand sends the take, not a tap
		EXPECT_FALSE(Release().has_value());
		EXPECT_EQ(_world->sent.size(), 1u);
		RunWaiting();
		return _system->GetHeld();
	}
};

class HandGrabSystemGiving: public HandGrabSystemWithWorld
{
protected:
	entt::entity HoldRock()
	{
		const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
		_world->underCursor = rock;
		Press();
		for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
		{
			Frame(10);
		}
		Release();
		return rock;
	}

	entt::entity AddCreature(creature_giving::Verdict verdict)
	{
		const auto creature = _world->registry.Create();
		_world->registry.Assign<Transform>(creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		_world->creatures[creature] = verdict;
		return creature;
	}
};

} // namespace hand_grab_test
