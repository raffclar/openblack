/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureTurnToFace.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS 1
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/FightTranscriptSystem.h"
#include "camera/scenarios/FakeTemple.h"
#include "camera/scenarios/Mock.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;

namespace
{
/// Counts the strength gains of the actions that each creature gets, and does nothing else
class CountedGains final: public ecs::systems::CreaturePhysiologySystemInterface
{
public:
	void ProcessTurn() override {}
	float Eat(entt::entity /*creature*/, float /*foodValue*/) override { return 0.0f; }
	void Drink(entt::entity /*creature*/) override {}
	void EmptyBowels(entt::entity /*creature*/) override {}
	void SitTurn(entt::entity /*creature*/) override {}
	void WakeFromFaint(entt::entity /*creature*/) override {}
	void FinishAction(entt::entity /*creature*/, std::string_view /*action*/) override {}
	void ModifyStrength(entt::entity /*creature*/, float /*amount*/) override {}
	void GainStrengthOf(entt::entity creature, std::string_view action) override
	{
		if (action == "Fight")
		{
			++gains[creature];
		}
	}
	void SetTimeScale(float /*scale*/) override {}
	[[nodiscard]] float GetTimeScale() const override { return 1.0f; }
	void SetFaintingEnabled(bool /*enabled*/) override {}
	[[nodiscard]] bool IsFaintingEnabled() const override { return true; }

	std::map<entt::entity, uint32_t> gains;
};

/// Keeps the reactions made, and does nothing else
class RecordedReactions final: public ecs::systems::ReactionSystemInterface
{
public:
	uint32_t Create(const Source& source) override
	{
		made.push_back(source);
		return static_cast<uint32_t>(made.size());
	}
	void Move(uint32_t, const glm::vec3&, const glm::vec3&, float) override {}
	[[nodiscard]] std::optional<Active> Find(uint32_t) const override { return std::nullopt; }
	void RemoveFrom(entt::entity) override {}
	void RemoveFrom(entt::entity, Reaction) override {}
	void Remove(uint32_t) override {}
	void SetInitiator(uint32_t, entt::entity) override {}
	[[nodiscard]] bool IsActive(uint32_t) const override { return false; }
	void StopReacting(entt::entity) override {}
	[[nodiscard]] bool HasReaction(entt::entity) const override { return false; }
	void ProcessTurn() override {}
	void Reset() override {}
	void SetLandBalance(Balance /*which*/, float /*balance*/) override {}
	[[nodiscard]] float GetLandBalance(Balance /*which*/) const override { return 1.0f; }
	[[nodiscard]] std::span<const Active> GetReactions() const override { return {}; }
	[[nodiscard]] std::vector<Active> ReactionsAt(const glm::vec3&) const override { return {}; }

	std::vector<Source> made;
};

/// A mind that keeps the plans that it is made to carry out, and does nothing else
class ForcedMinds final: public ecs::systems::CreatureMindSystemInterface
{
public:
	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(size_t, const glm::vec3&, std::optional<entt::entity>, std::optional<PlayerNames>) override {}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity creature, creature_face::Cue cue) override
	{
		feelings.emplace_back(creature, cue);
		return std::nullopt;
	}
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, double) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity creature) override
	{
		pooedDirectly = creature;
		return true;
	}
	bool Puke(entt::entity) override { return false; }
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
	void FightOrdered(entt::entity creature, entt::entity asker) override { ordered.emplace_back(creature, asker); }
	void AbandonAction(entt::entity creature) override { abandoned.push_back(creature); }
	void ObeyEnded(entt::entity creature, entt::entity asker) override { obeyEnded.emplace_back(creature, asker); }
	bool ForceDesireOn(entt::entity creature, creature_desires::Desire desire, entt::entity /*thing*/) override
	{
		desired.emplace_back(creature, desire);
		return true;
	}
	void ChangeDesires(entt::entity, const std::function<void(creature_desires::Desires&)>&) override {}
	void SetAgendaPriority(entt::entity, float) override {}
	bool ForceActivity(entt::entity creature, const ForcedActivity& plan) override
	{
		forced.emplace_back(creature, plan.desire, std::string(plan.action), plan.actionObject, plan.activityObject);
		return true;
	}

	struct Forced
	{
		Forced(entt::entity who, creature_desires::Desire wanted, std::string done, entt::entity on,
		       std::optional<entt::entity> about)
		    : creature(who)
		    , desire(wanted)
		    , action(std::move(done))
		    , object(on)
		    , activityObject(about)
		{
		}
		entt::entity creature;
		creature_desires::Desire desire;
		std::string action;
		entt::entity object;
		std::optional<entt::entity> activityObject;
	};
	[[nodiscard]] bool IsLyingDead(entt::entity) override
	{
		++deathChecks;
		return false;
	}
	creature_mind::HandsCheck CheckHandsBeforeAction(entt::entity creature) override
	{
		if (auto found = handsOf.find(creature); found != handsOf.end())
		{
			auto& [checks, done] = found->second;
			++done;
			return done <= checks.size() ? checks.at(done - 1) : creature_mind::HandsCheck::Free;
		}
		++handChecks;
		if (handChecks <= hands.size())
		{
			return hands.at(handChecks - 1);
		}
		return creature_mind::HandsCheck::Free;
	}
	std::vector<Forced> forced;
	/// Each creature told to obey in a fight, with the creature that told it
	std::vector<std::pair<entt::entity, entt::entity>> ordered;
	std::vector<std::pair<entt::entity, creature_face::Cue>> feelings;
	/// Each creature whose action stopped as a failure
	std::vector<entt::entity> abandoned;
	/// Each creature whose plan to obey ended, with the creature that it obeyed
	std::vector<std::pair<entt::entity, entt::entity>> obeyEnded;
	/// Each desire made fully dominant on a creature
	std::vector<std::pair<entt::entity, creature_desires::Desire>> desired;
	std::optional<entt::entity> pooedDirectly;
	/// What the checks of the hands before a faint do in turn, then Free
	std::vector<creature_mind::HandsCheck> hands;
	size_t handChecks {0};
	/// What the checks of the hands of one creature do in turn, then Free, and the count of its checks
	std::map<entt::entity, std::pair<std::vector<creature_mind::HandsCheck>, size_t>> handsOf;
	/// How many times a creature asked to fight was checked for death, one time for each question
	size_t deathChecks {0};
};

/// Legs that do nothing but count the creatures that they stop at once
class StoppingLegs final: public ecs::systems::CreatureLocomotionSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float) override {}
	void ShowTurnEnd(entt::entity) override {}
	MoveResult MoveTo(entt::entity, glm::vec2, Pace, float, float) override { return MoveResult::Busy; }
	MoveResult LeadTo(entt::entity, glm::vec2, float, float) override { return MoveResult::Busy; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Busy; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Busy; }
	MoveResult FleeFrom(entt::entity, glm::vec2, Pace) override { return MoveResult::Busy; }
	bool TurnToFace(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeading(entt::entity, GameHeading) override { return false; }
	bool TurnToFaceAtOnce(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeadingAtOnce(entt::entity, GameHeading) override { return false; }
	void Stop(entt::entity) override {}
	void StopAtOnce(entt::entity creature) override { stopped.push_back(creature); }
	void ResetPace(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	void PlaceWithoutWalking(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return false; }
	[[nodiscard]] float KeptFraction(entt::entity) const override { return 0.0f; }
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override
	{
		throw std::logic_error("the fights do not ask for the walkable land");
	}
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity, glm::ivec2) override { return {}; }

	std::vector<entt::entity> stopped;
};

/// A hand that holds the creature it is told to hold, and does nothing else
class HoldingHand final: public ecs::systems::CreatureHandSystemInterface
{
public:
	bool Grab() override { return false; }
	[[nodiscard]] bool MayHold(entt::entity) const override { return true; }
	[[nodiscard]] creature_hand::InteractTip InteractTipOver(entt::entity, bool) const override
	{
		return creature_hand::InteractTip::None;
	}
	[[nodiscard]] bool IsClick() const override { return false; }
	std::optional<HandPose> Update(const glm::vec3&, const glm::vec3&, glm::vec2, float, const input_steps::Steps&) override
	{
		return std::nullopt;
	}
	void Release() override {}
	bool Stroke(entt::entity, creature_feedback::BodyPart) override { return false; }
	bool Slap(entt::entity, float, bool, bool) override { return false; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return held; }
	[[nodiscard]] bool IsLocked(entt::entity creature) const override { return held == creature; }
	[[nodiscard]] bool IsHeldByCommand() const override { return false; }
	[[nodiscard]] std::optional<entt::entity> CreatureUnderCursor() const override { return std::nullopt; }
	[[nodiscard]] float GetFeedbackSum() const override { return 0.0f; }
	[[nodiscard]] float GetLastFeedbackSum() const override { return 0.0f; }
	bool TakeFocusTip() override { return false; }

	std::optional<entt::entity> held;
};

/// The game's random numbers: they give the numbers in the list in turn, then 0, and count the draws of each range
class ListedGameRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t range, DrawSite /*site*/) override
	{
		ranges.push_back(range);
		if (next < numbers.size())
		{
			return numbers.at(next++);
		}
		return 0;
	}
	float SyncedFloatRand(float, DrawSite) override { return 0.0f; }
	uint32_t LocalRand(int32_t) override { return 0; }
	float LocalFloatRand(float) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream) override {}

	std::vector<uint32_t> numbers;
	size_t next {0};
	std::vector<uint32_t> ranges;
};

/// Two creatures in a duel in an arena on flat land, neither with a body to click on. The camera shows their fight.
/// The hands of a network game: the players that have hands (the human gods), and the player of this computer
class FakeHands final: public ecs::systems::HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, 2> GetPlayerHands() const noexcept override
	{
		return GetHandsOf(PlayerNames::PLAYER_ONE).value_or(std::array {entt::entity {entt::null}, entt::entity {entt::null}});
	}
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetPlayerHandPositions() const noexcept override { return {}; }
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> /*picked*/, bool /*holdPoint*/) override {}
	[[nodiscard]] std::optional<std::array<entt::entity, 2>> GetHandsOf(PlayerNames player) const noexcept override
	{
		const auto found = hands.find(player);
		return found != hands.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] PlayerNames GetOwnPlayer() const noexcept override { return own; }
	void MarkOwnSend() override { ++sends; }
	[[nodiscard]] bool OwnSendWaits() const noexcept override { return waits; }

	std::map<PlayerNames, std::array<entt::entity, 2>> hands;
	PlayerNames own {PlayerNames::PLAYER_ONE};
	/// The hand of this computer waits for its last send
	bool waits {false};
	/// How many sends started the wait
	int sends {0};
};

/// The players of a land where computer minds play some of the gods
class FakePlayers final: public ecs::systems::PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames /*name*/) const override { return std::nullopt; }
	entt::entity EnsurePlayer(PlayerNames /*name*/) override { return entt::null; }
	void AddCreature(entt::entity /*creature*/) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames /*name*/) const override { return std::nullopt; }
	void KeepForNextLand() override {}
	void ForgetKept() override {}
	[[nodiscard]] bool KeepAlignmentAt(uint32_t /*turn*/) override { return false; }
	void TakeUpKept(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] bool IsComputerPlayer(PlayerNames name) const override { return computers.contains(name); }

	std::set<PlayerNames> computers;
};

/// A clock that only tells if the game is a network game, and the real time of the frame
class FakeTime final: public ecs::systems::TimeSystemInterface
{
public:
	void Start() override {}
	void Update() override {}
	void StartDraw() override {}
	[[nodiscard]] std::chrono::milliseconds GetElapsedTime() const override { return {}; }
	void StartGameClock(bool /*paused*/) override {}
	void RestartTurns() override {}
	void ResetTimerToTurn() override {}
	void ResumeAtTurn(uint32_t /*turn*/, int32_t /*remainderMs*/) override {}
	[[nodiscard]] int32_t GetTurnRemainderMs() const override { return 0; }
	[[nodiscard]] bool IsTurnDue() override { return false; }
	void StartTurn() override {}
	void UpdateFrame() override {}
	void StepInput(std::chrono::microseconds /*stepTime*/) override {}
	[[nodiscard]] const input_steps::Steps& GetInputSteps() const override { return inputSteps; }
	void PinDate(std::optional<int64_t> /*pinnedDate*/) override {}
	[[nodiscard]] uint32_t GetTurn() const override { return 0; }
	[[nodiscard]] float GetTurnFraction() const override { return 0.0f; }
	[[nodiscard]] std::chrono::milliseconds GetFrameGameTime() const override { return {}; }
	[[nodiscard]] std::chrono::milliseconds GetFrameRealTime() const override { return realTime; }
	void SetPaused(bool /*paused*/) override {}
	[[nodiscard]] bool IsPaused() const override { return false; }
	void SetPlayerPause(bool /*paused*/) override {}
	void SetHeld(bool /*held*/) override {}
	[[nodiscard]] bool IsHeld() const override { return false; }
	void SetNetworkGame(bool value) override { networkGame = value; }
	[[nodiscard]] bool IsNetworkGame() const override { return networkGame; }
	void SetSpeed(float /*speed*/) override {}
	[[nodiscard]] float GetSpeed() const override { return 1.0f; }
	[[nodiscard]] uint32_t GetTicks() const override { return 0; }
	[[nodiscard]] int64_t GetUnixTime() const override { return 0; }
	void RestartClock(std::optional<int64_t> /*unixTime*/) override {}
	[[nodiscard]] std::optional<int64_t> GetPinnedDate() const override { return std::nullopt; }

	bool networkGame {false};
	std::chrono::milliseconds realTime {0};
	input_steps::Steps inputSteps {};
};

/// A leash that is on every creature, and counts the creatures it is taken off
class HeldLeash final: public ecs::systems::LeashSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float /*seconds*/) override {}
	void ShowPosts(bool /*handShown*/) override {}
	bool PressAction(PlayerNames /*player*/, const glm::vec3& /*rayOrigin*/, const glm::vec3& /*rayDirection*/,
	                 bool /*doublePress*/, bool /*mayTie*/) override
	{
		return false;
	}
	bool TapThing(PlayerNames /*player*/, entt::entity /*object*/) override { return false; }
	[[nodiscard]] bool TakesTap(PlayerNames /*player*/, entt::entity /*object*/) const override { return false; }
	[[nodiscard]] std::optional<float> RopeUnderCursor(PlayerNames /*player*/, const RopeFrame& /*frame*/) override
	{
		return std::nullopt;
	}
	void SetRopeHovered(PlayerNames /*player*/, bool /*hovered*/) override {}
	[[nodiscard]] leash_snap::Hand RopeHand(PlayerNames /*player*/) const override { return leash_snap::Hand::Away; }
	bool PressRope(PlayerNames /*player*/) override { return false; }
	void LetGoOfRope(PlayerNames /*player*/, std::optional<glm::vec3> /*place*/) override {}
	void HandleInput(const glm::vec3& /*rayOrigin*/, const glm::vec3& /*rayDirection*/, bool /*actionTaken*/) override {}
	[[nodiscard]] bool Knows(entt::entity /*creature*/, LeashType /*type*/) const override { return false; }
	void SetKnown(entt::entity /*creature*/, LeashType /*type*/, bool /*known*/) override {}
	[[nodiscard]] bool IsLeashable(entt::entity /*creature*/) const override { return false; }
	bool SetLeashable(entt::entity /*creature*/, bool /*leashable*/) override { return false; }
	void SetOwner(entt::entity /*creature*/, PlayerNames /*owner*/) override {}
	void ClaimOnArrival(entt::entity /*creature*/) override {}
	[[nodiscard]] creature_leash::Refusal WhyNot(PlayerNames /*player*/, entt::entity /*creature*/,
	                                             LeashType /*type*/) const override
	{
		return {};
	}
	[[nodiscard]] std::optional<Refused> LastRefusal() const override { return std::nullopt; }
	bool PutOn(entt::entity /*creature*/, LeashType /*type*/) override { return false; }
	void TakeOff(entt::entity creature) override { takenOff.push_back(creature); }
	bool Toggle(entt::entity /*creature*/) override { return false; }
	bool ChangeType(entt::entity /*creature*/, LeashType /*type*/) override { return false; }
	bool PickType(PlayerNames /*player*/, LeashType /*type*/) override { return false; }
	bool TieTo(entt::entity /*creature*/, entt::entity /*object*/) override { return false; }
	void UntieToHand(entt::entity /*creature*/) override {}
	void PlayHandAttachSound(entt::entity /*creature*/) override {}
	void SetWorks(entt::entity /*creature*/, bool /*works*/) override {}
	void SetDrawn(bool /*drawn*/) override {}
	void ConfineToHome(entt::entity /*creature*/, float /*radius*/) override {}
	void ClearConfinement(entt::entity /*creature*/) override {}
	[[nodiscard]] bool FreeOfHome(entt::entity /*creature*/) const override { return true; }
	[[nodiscard]] bool IsLeashed(entt::entity /*creature*/) const override { return true; }
	[[nodiscard]] std::optional<entt::entity> TiedTo(entt::entity /*creature*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> HolderPoint(entt::entity /*creature*/) const override { return std::nullopt; }
	[[nodiscard]] LeashType TypeOf(entt::entity /*creature*/) const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames player) const override
	{
		return player == PlayerNames::PLAYER_ONE ? playersCreature : std::nullopt;
	}
	bool PressKey(PlayerNames /*player*/, creature_leash::LeashKey /*key*/) override { return false; }
	bool TapCreature(PlayerNames /*player*/, entt::entity /*creature*/) override { return false; }
	bool Shake(PlayerNames /*player*/) override { return false; }
	void PlacePosts(PlayerNames /*owner*/, const std::array<glm::vec3, 3>& /*points*/) override {}
	bool OrderAt(PlayerNames /*player*/, const glm::vec3& /*place*/) override { return false; }
	bool OrderOn(PlayerNames /*player*/, entt::entity /*object*/) override { return false; }
	[[nodiscard]] std::optional<glm::vec3> OrderTarget(entt::entity /*creature*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<uint32_t> ToolTip(PlayerNames /*player*/,
	                                              std::optional<entt::entity> /*hovered*/) const override
	{
		return std::nullopt;
	}
	bool TapPost(PlayerNames /*player*/, entt::entity /*post*/) override { return false; }

	std::optional<entt::entity> playersCreature;
	std::vector<entt::entity> takenOff;
};

class CreatureFightSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_first = MakeCreature(registry, PlayerNames::PLAYER_ONE, {-15.0f, 0.0f, 0.0f});
		_second = MakeCreature(registry, PlayerNames::PLAYER_TWO, {15.0f, 0.0f, 0.0f});
		// The arena has appeared
		const auto arena = registry.Create();
		for (const auto& [self, other] : {std::pair(_first, _second), std::pair(_second, _first)})
		{
			registry.Assign<CreatureFighting>(self, CreatureFighting {
			                                            .stage = CreatureFighting::Stage::Duel,
			                                            .opponent = other,
			                                            .arena = {.centre = {0.0f, 0.0f}, .radius = 40.0f},
			                                            .madeArena = self == _first,
			                                            .arenaEntity = arena,
			                                        });
		}
		ShowTheFight({0.0f, 0.0f, 0.0f});
	}
	void TearDown() override
	{
		_fights.Reset();
		Locator::pickingSystem::reset();
		Locator::handSystem::reset();
		Locator::playerSystem::reset();
		Locator::time::reset();
		Locator::camera::reset();
		Locator::entitiesRegistry::reset();
	}

	/// The world camera looks at the arena. A double click sends it to a place, and the update after it starts.
	void ShowTheFight(const glm::vec3& place)
	{
		if (!Locator::camera::has_value())
		{
			auto& camera = Locator::camera::emplace<Camera>();
			camera.SetOrigin({0.0f, 30.0f, 40.0f});
			camera.SetFocus({0.0f, 0.0f, 0.0f});
		}
		_fights.FlewTo(place);
		_fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	}

	static entt::entity MakeCreature(Registry& registry, PlayerNames owner, glm::vec3 at)
	{
		const auto creature = registry.Create();
		registry.Assign<Transform>(creature, at, glm::mat3(1.0f), glm::vec3(1.0f));
		auto& body = registry.Assign<Creature>(creature);
		body.owner = owner;
		registry.Assign<CreatureAnimation>(creature);
		registry.Assign<CreatureLocomotion>(creature);
		return creature;
	}

	/// A press looking down at a point on the ground
	bool PressGround(glm::vec2 point, fight::Button button, uint32_t milliseconds = 0, uint32_t turn = 0)
	{
		return _fights.Press({point.x, 100.0f, point.y}, {0.0f, -1.0f, 0.0f}, button, milliseconds, turn);
	}

	[[nodiscard]] const fight::MoveQueue& QueueOf(entt::entity creature) const
	{
		return Locator::entitiesRegistry::value().Get<const CreatureFighting>(creature).fighter.queue;
	}

	ecs::systems::CreatureFightSystem _fights;
	entt::entity _first {entt::null};
	entt::entity _second {entt::null};
};
} // namespace

TEST_F(CreatureFightSystemTest, ThePlayersFighterIsTheirFirstCreature)
{
	EXPECT_EQ(_fights.PlayersFighter(), _first);
	// A creature the player got later doesn't take its place
	auto& registry = Locator::entitiesRegistry::value();
	MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_EQ(_fights.PlayersFighter(), _first);
	EXPECT_EQ(_fights.HandTip(_first), fight::Tip::Block);
	EXPECT_EQ(_fights.HandTip(_second), fight::Tip::Attack);
	EXPECT_EQ(_fights.HandTip(std::nullopt), fight::Tip::Manoeuvre);
}

TEST_F(CreatureFightSystemTest, TheMoveButtonReplacesAndTheActionButtonQueues)
{
	EXPECT_TRUE(PressGround({-15.0f, 10.0f}, fight::Button::Action));
	_fights.Release(0, 0);
	EXPECT_TRUE(PressGround({-15.0f, -10.0f}, fight::Button::Action));
	_fights.Release(0, 0);
	EXPECT_EQ(QueueOf(_first).Size(), 2u);
	EXPECT_TRUE(PressGround({-25.0f, 0.0f}, fight::Button::Move));
	_fights.Release(0, 0);
	EXPECT_EQ(QueueOf(_first).Size(), 1u);
	// The ground beyond the arena isn't the fight's
	EXPECT_FALSE(PressGround({45.0f, 0.0f}, fight::Button::Action));
	EXPECT_EQ(QueueOf(_first).Size(), 1u);
}

TEST_F(CreatureFightSystemTest, TheTurnGivesTheSentMovesToTheSendingPlayersCreature)
{
	// A press on the opponent: the blow goes to the sender's creature and waits for its charge
	_fights.DoMove(PlayerNames::PLAYER_ONE, _second, fight::AttackMove(fight::Band::High), false);
	ASSERT_EQ(QueueOf(_first).Size(), 1u);
	EXPECT_FALSE(QueueOf(_first).Moves()[0].chargeMs.has_value());
	// A press on a thing that is gone by the turn gives nothing
	_fights.DoMove(PlayerNames::PLAYER_ONE, entt::null, fight::BlockMove(), false);
	EXPECT_EQ(QueueOf(_first).Size(), 1u);
	// The other player's press goes to the other player's creature
	_fights.DoMove(PlayerNames::PLAYER_TWO, _first, fight::BlockMove(), false);
	EXPECT_EQ(QueueOf(_second).Size(), 1u);
	EXPECT_EQ(QueueOf(_first).Size(), 1u);
	// The let go charges the blow that waits
	_fights.DoCharge(PlayerNames::PLAYER_ONE, 400.0f);
	ASSERT_TRUE(QueueOf(_first).Moves()[0].chargeMs.has_value());
	EXPECT_EQ(*QueueOf(_first).Moves()[0].chargeMs, 400.0f);
}

TEST_F(CreatureFightSystemTest, TheTurnStepsFromWhereTheCreatureStandsThen)
{
	// The press was made with the creature at its place; by the turn it stands past the point, so it steps back
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<Transform>(_first).position = {-5.0f, 0.0f, 0.0f};
	_fights.DoGroundPress(PlayerNames::PLAYER_ONE, _first, {-10.0f, 0.0f, 0.0f}, true);
	registry.Get<Transform>(_first).position = {-15.0f, 0.0f, 0.0f};
	_fights.DoGroundPress(PlayerNames::PLAYER_ONE, _first, {-10.0f, 0.0f, 0.0f}, false);
	ASSERT_EQ(QueueOf(_first).Size(), 2u);
	EXPECT_NE(QueueOf(_first).Moves()[0].move, QueueOf(_first).Moves()[1].move);
	// Only the sender's own creature steps
	_fights.DoGroundPress(PlayerNames::PLAYER_TWO, _first, {-10.0f, 0.0f, 0.0f}, false);
	EXPECT_EQ(QueueOf(_first).Size(), 2u);
}

TEST_F(CreatureFightSystemTest, ADoubleClickIntoTheArenaShowsTheFightAtOnce)
{
	EXPECT_TRUE(_fights.IsCameraOnFight());
	EXPECT_TRUE(_fights.DirectsFight());
	_fights.Reset();
	EXPECT_FALSE(_fights.IsCameraOnFight());
	// A place out of the arena does not show the fight
	ShowTheFight({45.0f, 0.0f, 0.0f});
	EXPECT_FALSE(_fights.IsCameraOnFight());
	// A double click that a new land forgot does not show the fight
	_fights.FlewTo({0.0f, 0.0f, 0.0f});
	_fights.Reset();
	_fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	EXPECT_FALSE(_fights.IsCameraOnFight());
}

TEST_F(CreatureFightSystemTest, ADoubleClickMeasuresFromWhereTheCameraWasBeforeItsFlight)
{
	_fights.Reset();
	auto& camera = Locator::camera::value();
	// The camera is far from the arena when the player double clicks into it
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	_fights.FlewTo({0.0f, 0.0f, 0.0f});
	// The flight brings the camera near before the next update, but the fight is not shown
	camera.SetOrigin({0.0f, 30.0f, 40.0f});
	camera.SetFocus({0.0f, 0.0f, 0.0f});
	_fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	EXPECT_FALSE(_fights.IsCameraOnFight());
	// A fight of the player's creature under a script's control is shown from any distance
	Locator::entitiesRegistry::value().Assign<ScriptControlled>(_second);
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(_fights.IsCameraOnFight());
}

TEST_F(CreatureFightSystemTest, WithoutTheFightViewTheHandDoesNotDirectTheFight)
{
	_fights.Reset();
	EXPECT_FALSE(_fights.DirectsFight());
	EXPECT_FALSE(PressGround({-15.0f, 10.0f}, fight::Button::Action));
	EXPECT_TRUE(QueueOf(_first).Empty());
	EXPECT_EQ(_fights.HandTip(_second), std::nullopt);
	// The duel still has the player's fighter, for the gestures
	EXPECT_EQ(_fights.PlayersFighter(), _first);
}

TEST_F(CreatureFightSystemTest, ANewLandForgetsThePressAndTheView)
{
	EXPECT_TRUE(PressGround({-15.0f, 10.0f}, fight::Button::Move, 100, 3));
	EXPECT_TRUE(_fights.IsPressed());
	_fights.SetFightExit(false);
	_fights.Reset();
	EXPECT_FALSE(_fights.IsPressed());
	EXPECT_FALSE(_fights.IsCameraOnFight());
	EXPECT_TRUE(_fights.GetFightExit());
	// The creatures of the old land are gone; letting go of the button touches nothing
	Locator::entitiesRegistry::value().Destroy(_first);
	_fights.Release(400, 5);
	EXPECT_FALSE(_fights.IsPressed());
}

namespace
{
/// The player goes into the temple: the player's camera of the world continues behind it with its model
std::unique_ptr<Camera> GoIntoTemple(FakeTemple& temple)
{
	auto& camera = Locator::camera::value();
	auto behind = camera.ContinueWith(camera.SetModel(std::make_unique<DefaultWorldCameraModel>()));
	temple.worldCamera = behind.get();
	temple.active = true;
	return behind;
}

/// The player comes out of the temple: the player's camera takes back the camera of the world
void ComeOutOfTemple(FakeTemple& temple, std::unique_ptr<Camera>& behind)
{
	Locator::camera::value().TakeBackFrom(*behind);
	behind.reset();
	temple.worldCamera = nullptr;
	temple.active = false;
}
} // namespace

TEST_F(CreatureFightSystemTest, TheFightViewGoesOnBehindTheTempleAndStaysWhenThePlayerComesOut)
{
	ASSERT_TRUE(_fights.IsCameraOnFight());
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	auto behind = GoIntoTemple(temple);
	_fights.Update(std::chrono::duration<float, std::milli>(16.0f));
	EXPECT_TRUE(_fights.IsCameraOnFight());
	EXPECT_NE(dynamic_cast<const FightCameraModel*>(&behind->GetModel()), nullptr);
	// With no place picked on the map, the fight view stays on the player's camera
	ComeOutOfTemple(temple, behind);
	_fights.ComeOutOfTemple(false);
	EXPECT_TRUE(_fights.IsCameraOnFight());
	EXPECT_NE(dynamic_cast<const FightCameraModel*>(&Locator::camera::value().GetModel()), nullptr);
	Locator::temple::reset();
}

TEST_F(CreatureFightSystemTest, APlacePickedOnTheMapInTheTempleEndsTheFightView)
{
	ASSERT_TRUE(_fights.IsCameraOnFight());
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	auto behind = GoIntoTemple(temple);
	_fights.Update(std::chrono::duration<float, std::milli>(16.0f));
	ComeOutOfTemple(temple, behind);
	_fights.ComeOutOfTemple(true);
	EXPECT_FALSE(_fights.IsCameraOnFight());
	// The player's own camera of the world has the camera again
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&Locator::camera::value().GetModel()), nullptr);
	Locator::temple::reset();
}

TEST_F(CreatureFightSystemTest, AFightThatIsOverWhenThePlayerComesOutOfTheTempleEndsTheFightView)
{
	ASSERT_TRUE(_fights.IsCameraOnFight());
	// The duel is over: the arena is no longer on
	Locator::entitiesRegistry::value().Get<CreatureFighting>(_first).stage = CreatureFighting::Stage::Celebrate;
	_fights.ComeOutOfTemple(false);
	EXPECT_FALSE(_fights.IsCameraOnFight());
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&Locator::camera::value().GetModel()), nullptr);
}

TEST_F(CreatureFightSystemTest, AScriptHandsAFighterToTheComputerOrToNobody)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(_first).fighter.control = fight::Control::Player;
	EXPECT_TRUE(_fights.IsAutoFighting(_first));
	_fights.SetAutoFighting(_first, false);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).fighter.control, fight::Control::None);
	EXPECT_FALSE(_fights.IsAutoFighting(_first));
	// Moves queued for it don't hand it back to anyone
	EXPECT_TRUE(_fights.QueueMove(_first, fight::AttackMove(fight::Band::Low), false));
	EXPECT_FALSE(_fights.IsAutoFighting(_first));
	EXPECT_EQ(_fights.QueuedBlows(_first), 1u);
	_fights.SetAutoFighting(_first, true);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).fighter.control, fight::Control::Computer);
	EXPECT_TRUE(_fights.IsAutoFighting(_first));
	// A creature that never fought has nobody choosing its moves yet
	const auto newcomer = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_FALSE(_fights.IsAutoFighting(newcomer));
	_fights.SetAutoFighting(newcomer, true);
	EXPECT_TRUE(_fights.IsAutoFighting(newcomer));
}

TEST_F(CreatureFightSystemTest, ScriptsAskWhatAFighterIsDoing)
{
	auto& registry = Locator::entitiesRegistry::value();
	fight::Enter(registry.Get<CreatureFighting>(_first).fighter, fight::State::Stance);
	EXPECT_EQ(_fights.CurrentFightAction(_first), fight::FightAction::Stance);
	const auto newcomer = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	EXPECT_EQ(_fights.CurrentFightAction(newcomer), fight::FightAction::Other);
	registry.Assign<CreatureKnockedOut>(newcomer);
	EXPECT_EQ(_fights.CurrentFightAction(newcomer), fight::FightAction::Fainted);
}

TEST_F(CreatureFightSystemTest, AKnockOutThatNoBlowMadeGivesNoSightOfAFightWon)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::value());
	// A miracle or a loss of life knocks the loser out, not a blow. The faint alone makes a reaction, as to a death, on
	// the turn that it starts on the body.
	_fights.KnockOut(_second);
	EXPECT_TRUE(reactions.made.empty());
	_fights.ProcessTurn();
	ASSERT_EQ(reactions.made.size(), 1u);
	EXPECT_EQ(reactions.made.front().type, Reaction::ReactToDeath);
	EXPECT_EQ(reactions.made.front().initiator, _second);
	EXPECT_TRUE(reactions.made.front().playerless);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheWinnerIsMadeToShowOffAtTheLoser)
{
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	Locator::entitiesRegistry::value().Get<Creature>(_first).alignment = -0.5f;
	_fights.KnockOut(_second);
	ASSERT_EQ(minds.forced.size(), 1u);
	EXPECT_EQ(minds.forced[0].creature, _first);
	EXPECT_EQ(minds.forced[0].desire, creature_desires::Desire::ObeyPlayer);
	EXPECT_EQ(minds.forced[0].action, "ShowImpressiveAnimation");
	EXPECT_EQ(minds.forced[0].object, _second);
	EXPECT_EQ(minds.forced[0].activityObject, _second);
	EXPECT_FALSE(minds.pooedDirectly.has_value());
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, AnEvilWinnerIsMadeToPooOnTheLoser)
{
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	Locator::entitiesRegistry::value().Get<Creature>(_first).alignment = -0.51f;
	_fights.KnockOut(_second);
	ASSERT_EQ(minds.forced.size(), 1u);
	EXPECT_EQ(minds.forced[0].action, "Poo");
	EXPECT_EQ(minds.forced[0].object, _second);
	// The mind carries out the plan's own agenda: the fight never makes the creature poo by itself
	auto& registry = Locator::entitiesRegistry::value();
	for (int turn = 0; turn < 600 && registry.AllOf<CreatureFighting>(_first); ++turn)
	{
		_fights.Update(std::chrono::duration<float, std::milli>(100.0f));
		_fights.ProcessTurn();
	}
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(_first));
	EXPECT_FALSE(minds.pooedDirectly.has_value());
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, ScriptsSeeAFightOnlyWhileItsDuelLasts)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(_fights.IsDuelling(_first));
	registry.Get<CreatureFighting>(_first).stage = CreatureFighting::Stage::Taunt;
	EXPECT_FALSE(_fights.IsDuelling(_first));
	EXPECT_TRUE(_fights.IsFighting(_first));
	registry.Get<CreatureFighting>(_first).stage = CreatureFighting::Stage::Duel;
	_fights.KnockOut(_second);
	// The winner finishes its move in the fight, but its duel is over
	EXPECT_FALSE(_fights.IsDuelling(_first));
	EXPECT_FALSE(_fights.IsDuelling(_second));
}

TEST_F(CreatureFightSystemTest, AScriptTakesACreatureFromItsDuelForBoth)
{
	_fights.TakeForScript(_first);
	EXPECT_FALSE(_fights.IsDuelling(_first));
	EXPECT_FALSE(_fights.IsDuelling(_second));
	// Neither won: neither is knocked out
	EXPECT_FALSE(_fights.IsKnockedOut(_first));
	EXPECT_FALSE(_fights.IsKnockedOut(_second));
}

TEST_F(CreatureFightSystemTest, AScriptTakesAKnockedOutCreatureBeforeItIsTakenHome)
{
	auto& registry = Locator::entitiesRegistry::value();
	_fights.KnockOut(_second);
	ASSERT_TRUE(_fights.IsKnockedOut(_second));
	const auto where = registry.Get<const Transform>(_second).position;
	_fights.TakeForScript(_second);
	EXPECT_FALSE(_fights.IsKnockedOut(_second));
	// It is not taken home later
	for (int turn = 0; turn < 600; ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const Transform>(_second).position, where);
}

TEST_F(CreatureFightSystemTest, AForcedPlanGetsAKnockedOutCreatureUpWhereItLies)
{
	auto& registry = Locator::entitiesRegistry::value();
	_fights.Withdraw(_second);
	registry.Assign<CreatureNeeds>(_second);
	_fights.ForceFaint(_second);
	ASSERT_TRUE(_fights.IsKnockedOut(_second));
	EXPECT_EQ(registry.Get<const CreatureNeeds>(_second).rest, CreatureNeeds::Rest::Unconscious);
	const auto where = registry.Get<const Transform>(_second).position;
	_fights.GetUpForPlan(_second);
	EXPECT_FALSE(_fights.IsKnockedOut(_second));
	EXPECT_EQ(registry.Get<const CreatureNeeds>(_second).rest, CreatureNeeds::Rest::Awake);
	// It is not taken home later
	for (int turn = 0; turn < 600; ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const Transform>(_second).position, where);
}

TEST_F(CreatureFightSystemTest, AForcedPlanDoesNotRaiseACreatureKilledForGood)
{
	_fights.Withdraw(_second);
	_fights.KillPermanently(_second);
	ASSERT_TRUE(_fights.IsKnockedOut(_second));
	_fights.GetUpForPlan(_second);
	EXPECT_TRUE(_fights.IsKnockedOut(_second));
}

// A creature that got up from a faint for a forced plan does not faint again until the end of a faint or a turn with
// no plan lets it
TEST_F(CreatureFightSystemTest, ACreatureThatFaintedCannotFaintAgainUntilItIsFreeOfThatFaint)
{
	auto& registry = Locator::entitiesRegistry::value();
	_fights.Withdraw(_second);
	registry.Assign<CreatureNeeds>(_second);
	_fights.ForceFaint(_second);
	ASSERT_TRUE(_fights.IsKnockedOut(_second));
	EXPECT_TRUE(registry.Get<const CreatureNeeds>(_second).fainted);
	_fights.GetUpForPlan(_second);
	_fights.ForceFaint(_second);
	_fights.KnockOut(_second);
	EXPECT_FALSE(_fights.IsKnockedOut(_second));
	registry.Get<CreatureNeeds>(_second).fainted = false;
	_fights.KnockOut(_second);
	EXPECT_TRUE(_fights.IsKnockedOut(_second));
}

// A script that kills a creature for good kills it also just after it got up from a faint
TEST_F(CreatureFightSystemTest, AScriptKillsForGoodACreatureThatJustGotUpFromAFaint)
{
	auto& registry = Locator::entitiesRegistry::value();
	_fights.Withdraw(_second);
	registry.Assign<CreatureNeeds>(_second);
	_fights.ForceFaint(_second);
	_fights.GetUpForPlan(_second);
	_fights.KillPermanently(_second);
	EXPECT_TRUE(_fights.IsKnockedOut(_second));
}

TEST_F(CreatureFightSystemTest, AScriptDoesNotRaiseACreatureKilledForGood)
{
	_fights.Withdraw(_second);
	_fights.KillPermanently(_second);
	ASSERT_TRUE(_fights.IsKnockedOut(_second));
	_fights.TakeForScript(_second);
	EXPECT_TRUE(_fights.IsKnockedOut(_second));
}

TEST_F(CreatureFightSystemTest, AScriptReadsAndSetsTheFightHealthInAndOutOfAFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(_second).fighter.health = 0.6f;
	EXPECT_FLOAT_EQ(_fights.GetFightHealth(_second), 0.6f);
	// Set in a fight, the fighter takes it at once
	_fights.SetFightHealth(_second, 1.0f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(_second).fighter.health, 1.0f);
	registry.Get<CreatureFighting>(_second).fighter.health = 0.25f;
	// Out of the fight it stays as the fight left it
	_fights.Withdraw(_second);
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(_second));
	EXPECT_FLOAT_EQ(_fights.GetFightHealth(_second), 0.25f);
	// A creature that never fought has full fight health, and keeps whatever a script gives it
	const auto other = MakeCreature(registry, PlayerNames::PLAYER_THREE, {200.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(_fights.GetFightHealth(other), 1.0f);
	_fights.SetFightHealth(other, 1.5f);
	EXPECT_FLOAT_EQ(_fights.GetFightHealth(other), 1.5f);
}

TEST_F(CreatureFightSystemTest, AFightCalledOffEndsWithNoWinnerAndNoResponse)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::value());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
	}
	EXPECT_TRUE(_fights.QueueMove(_first, fight::AttackMove(fight::Band::High), false));
	_fights.CallOff(_first);
	for (const auto self : {_first, _second})
	{
		const auto& side = registry.Get<const CreatureFighting>(self);
		// Both wind down to stand, with nothing left to do and nothing to celebrate or sulk about
		EXPECT_TRUE(side.ended);
		EXPECT_TRUE(side.fighter.stopping);
		EXPECT_TRUE(side.played);
		EXPECT_EQ(side.stage, CreatureFighting::Stage::Respond);
		EXPECT_TRUE(side.fighter.queue.Empty());
	}
	// Nobody won it
	EXPECT_TRUE(reactions.made.empty());
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(_first).wins, 0u);
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(_second).wins, 0u);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, OnTheWayToAFightACreatureIsSettingItUp)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_FALSE(_fights.IsSettingUpFight(_first));
	registry.Get<CreatureFighting>(_first).stage = CreatureFighting::Stage::Approach;
	EXPECT_TRUE(_fights.IsSettingUpFight(_first));
	// Called off then, it simply leaves the fight
	_fights.CallOff(_first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(_first));
}

TEST_F(CreatureFightSystemTest, AFightThatStopsBeforeItsDuelEndsItsTranscript)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transcript = Locator::fightTranscript::emplace<ecs::systems::FightTranscriptSystem>();
	const auto fight = transcript.Begin(_first, _second, {});
	registry.Get<CreatureFighting>(_first).stage = CreatureFighting::Stage::Approach;
	registry.Get<CreatureFighting>(_second).stage = CreatureFighting::Stage::Approach;
	// The fight stops on the way. The transcript gets its end and the reason.
	_fights.CallOff(_first);
	const auto* found = transcript.GetRecorder().Find(fight);
	ASSERT_NE(found, nullptr);
	EXPECT_TRUE(found->ended.has_value());
	const auto over = transcript.LastOf(_first, "fight_over");
	EXPECT_TRUE(over.has_value());
	Locator::fightTranscript::reset();
}

TEST_F(CreatureFightSystemTest, AThingThatTheMindActsOnStartsNoFightInTheTurn)
{
	// The thing that the mind acts on is for its planner only. A tie of the leash to a creature starts no fight.
	auto& registry = Locator::entitiesRegistry::value();
	const auto third = MakeCreature(registry, PlayerNames::PLAYER_THREE, {100.0f, 0.0f, 0.0f});
	const auto fourth = MakeCreature(registry, PlayerNames::PLAYER_FOUR, {110.0f, 0.0f, 0.0f});
	registry.Assign<CreatureMindState>(third).leash.actOn = {entt::to_integral(fourth)};
	_fights.ProcessTurn();
	_fights.FightTurn(third);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(third));
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(fourth));
}

TEST_F(CreatureFightSystemTest, ACreatureRefusesAFightWithOneTooBigOrTooSmall)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto big = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	const auto small = MakeCreature(registry, PlayerNames::PLAYER_TWO, {130.0f, 0.0f, 0.0f});
	registry.Get<Creature>(big).size = 1.5f;
	registry.Get<Creature>(small).size = 0.6f;
	using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
	// Less than half the asker's height, it says no, either way round
	EXPECT_EQ(_fights.StartFight(big, small), Start::Refused);
	EXPECT_EQ(_fights.StartFight(small, big), Start::Refused);
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(big));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(small));
	// Only one of the two under a script's control: refused too
	registry.Get<Creature>(small).size = 1.0f;
	registry.Assign<ScriptControlled>(small);
	EXPECT_EQ(_fights.StartFight(big, small), Start::Refused);
}

// A creature with almost no life cannot start a fight. A script's order starts the fight all the same, and the hurt
// creature fights with less fight health. Thus the last fight of the story starts again after a lost bout.
TEST_F(CreatureFightSystemTest, AScriptStartsAFightForACreatureTooHurtToStartOne)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto hurt = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	const auto other = MakeCreature(registry, PlayerNames::PLAYER_TWO, {130.0f, 0.0f, 0.0f});
	registry.AssignOrReplace<CreatureNeeds>(hurt).needs.life = 0.05f;
	using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
	EXPECT_EQ(_fights.StartFight(hurt, other), Start::TooWeak);
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(hurt));
	EXPECT_EQ(_fights.StartForcedFight(hurt, other), Start::Started);
	ASSERT_TRUE(registry.AllOf<CreatureFighting>(hurt));
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(hurt).fighter.health, fight::FightHealthAtStart(0.05f));
	EXPECT_TRUE(registry.AllOf<CreatureFighting>(other));
}

// A creature that does a joint action with a third creature refuses a fight; its partner may still ask it
TEST_F(CreatureFightSystemTest, ACreatureTakenUpWithAThirdCreatureRefusesAFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto asker = MakeCreature(registry, PlayerNames::PLAYER_ONE, {100.0f, 0.0f, 0.0f});
	const auto asked = MakeCreature(registry, PlayerNames::PLAYER_TWO, {130.0f, 0.0f, 0.0f});
	const auto third = MakeCreature(registry, PlayerNames::PLAYER_TWO, {160.0f, 0.0f, 0.0f});
	using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
	registry.Assign<CreaturePartner>(asked, CreaturePartner {.partner = third});
	EXPECT_EQ(_fights.StartFight(asker, asked), Start::Refused);
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(asked));
	registry.AssignOrReplace<CreaturePartner>(asked, CreaturePartner {.partner = asker});
	EXPECT_EQ(_fights.StartFight(asker, asked), Start::Started);
}

TEST_F(CreatureFightSystemTest, AKnockOutKeepsTheWinnerAndTheLoserFromReactions)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<CreatureMindState>(_first).idle.serial = 4;
	registry.Assign<CreatureMindState>(_second).idle.serial = 9;
	Locator::creatureMindSystem::emplace<ForcedMinds>();
	_fights.KnockOut(_second);
	// Each keeps the number of the agenda that it had at the knock-out
	ASSERT_TRUE(registry.AllOf<CreatureAfterFight>(_first));
	ASSERT_TRUE(registry.AllOf<CreatureAfterFight>(_second));
	EXPECT_EQ(registry.Get<const CreatureAfterFight>(_first).agenda, 4u);
	EXPECT_EQ(registry.Get<const CreatureAfterFight>(_second).agenda, 9u);
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, AFightFacesOffAndTauntsBeforeItLooksForAnArena)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<CreatureFighting>(_first).stage, CreatureFighting::Stage::FaceOff);
	EXPECT_EQ(registry.Get<CreatureFighting>(_second).stage, CreatureFighting::Stage::FaceOff);
	EXPECT_TRUE(registry.Get<CreatureFighting>(_first).arenaEntity == entt::null);
	EXPECT_TRUE(_fights.IsSettingUpFight(_second));

	// The first creature asks, tells the other creature not to think and to turn, and turns itself on the fourth turn.
	// With no legs, nothing turns. On the next turn it tells the other creature to taunt with it.
	for (int turn = 0; turn < 4; ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<CreatureFighting>(_first).stage, CreatureFighting::Stage::FaceOff);
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureFighting>(_first).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(registry.Get<CreatureFighting>(_second).stage, CreatureFighting::Stage::Taunt);

	// This land has no room for an arena, so after the taunts the fight stops for both
	for (int turn = 0; turn < 4 && registry.AnyOf<CreatureFighting>(_first); ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_second));
}

namespace
{
/// The hands of the creatures. They record each creature that drops what it holds, and are busy for the creatures in
/// the busy list.
class RecordedHands final: public ecs::systems::CreatureObjectActionSystemInterface
{
public:
	void ProcessTurn() override {}
	void ActionTurn(entt::entity) override {}
	void AfterPoseTurn(entt::entity) override {}
	void UpdateHeld() override {}
	void ShowHeldAtTurnEnd() override {}
	bool PickUp(entt::entity, entt::entity) override { return false; }
	bool PutDown(entt::entity) override { return false; }
	bool Discard(entt::entity) override { return false; }
	bool DiscardWith(entt::entity, size_t) override { return false; }
	bool EatHeld(entt::entity) override { return false; }
	bool EatHeldOnly(entt::entity) override { return false; }
	bool Keep(entt::entity, size_t) override { return false; }
	bool Throw(entt::entity, const glm::vec3&) override { return false; }
	bool ThrowTaking(entt::entity, const glm::vec3&, float) override { return false; }
	bool ThrowAimed(entt::entity, const glm::vec3&, float) override { return false; }
	bool Destroy(entt::entity, entt::entity) override { return false; }
	bool Kick(entt::entity, entt::entity) override { return false; }
	bool PointAt(entt::entity, const glm::vec3&) override { return false; }
	bool Catch(entt::entity, entt::entity) override { return false; }
	void Cancel(entt::entity) override {}
	void StopForPlan(entt::entity creature, bool stopsBody) override { planStops.emplace_back(creature, stopsBody); }
	void Drop(entt::entity creature) override { dropped.push_back(creature); }
	[[nodiscard]] State GetState(entt::entity creature) const override
	{
		return std::ranges::find(busy, creature) != busy.end() ? State::Busy : State::Idle;
	}
	[[nodiscard]] bool IsCatching(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<float> GetProgress(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanPickUp(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanEat(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanEatHeld(entt::entity) const override { return false; }
	[[nodiscard]] bool CanDestroy(entt::entity) const override { return false; }

	std::vector<entt::entity> dropped;
	std::vector<entt::entity> busy;
	/// Each creature whose plan stopped the work of its hands, and whether the stop also stopped its body
	std::vector<std::pair<entt::entity, bool>> planStops;
};
} // namespace

// Each creature keeps what it holds as the fight starts. Before its taunt, it checks its hands: with a thing in the hand
// it starts to let go, and it tries the taunt from the next turn with no new check. The let-go keeps the body busy.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, EachCreatureLetsGoOfWhatItHoldsBeforeItsTaunt)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	auto& hands = static_cast<RecordedHands&>(Locator::creatureObjectActionSystem::emplace<RecordedHands>());
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_TRUE(hands.dropped.empty());
	EXPECT_EQ(minds.handsOf.size(), 0u);

	using creature_mind::HandsCheck;
	minds.handsOf[_first] = {{HandsCheck::Wait, HandsCheck::LetGo}, 0};
	minds.handsOf[_second] = {{}, 0};
	// Both taunt, so the other creature obeys already
	registry.Get<CreatureFighting>(_first).stage = CreatureFighting::Stage::Taunt;
	registry.Get<CreatureFighting>(_second).stage = CreatureFighting::Stage::Taunt;
	registry.Get<CreatureFighting>(_second).part = CreatureFighting::Part::Obeys;
	const auto& fighter = registry.Get<const CreatureFighting>(_first);
	game.ranges.clear();

	// The other creature has empty hands, so it taunts on the first turn. The first creature cannot start to let go.
	_fights.ProcessTurn();
	EXPECT_EQ(minds.handsOf[_first].second, 1u);
	EXPECT_EQ(minds.handsOf[_second].second, 1u);
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_second).played);
	EXPECT_FALSE(fighter.played);
	EXPECT_FALSE(fighter.handsChecked);
	EXPECT_EQ(game.ranges.size(), 1u);

	// It starts to let go, and does not try the taunt on this turn
	_fights.ProcessTurn();
	EXPECT_EQ(minds.handsOf[_first].second, 2u);
	EXPECT_TRUE(fighter.handsChecked);
	EXPECT_FALSE(fighter.played);
	EXPECT_EQ(game.ranges.size(), 1u);

	// While the let-go plays, each try draws a side and fails, with no new check of the hands
	hands.busy = {_first};
	_fights.ProcessTurn();
	_fights.ProcessTurn();
	EXPECT_EQ(minds.handsOf[_first].second, 2u);
	EXPECT_FALSE(fighter.played);
	EXPECT_EQ(game.ranges.size(), 3u);

	// The let-go ends and the taunt starts
	hands.busy.clear();
	_fights.ProcessTurn();
	EXPECT_EQ(minds.handsOf[_first].second, 2u);
	EXPECT_TRUE(fighter.played);
	EXPECT_EQ(game.ranges.size(), 4u);
	EXPECT_TRUE(hands.dropped.empty());
	Locator::gameRandom::reset();
	Locator::creatureMindSystem::reset();
	Locator::creatureObjectActionSystem::reset();
}

namespace
{
/// Legs that turn a creature to a heading at once, unless its body plays an action. They count the soft stops, the stops
/// at once and the asks to turn of each creature.
class TurningLegs final: public ecs::systems::CreatureLocomotionSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float) override {}
	void ShowTurnEnd(entt::entity) override {}
	MoveResult MoveTo(entt::entity, glm::vec2, Pace, float, float) override { return MoveResult::Busy; }
	MoveResult LeadTo(entt::entity, glm::vec2, float, float) override { return MoveResult::Busy; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Busy; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Busy; }
	MoveResult FleeFrom(entt::entity, glm::vec2, Pace) override { return MoveResult::Busy; }
	bool TurnToFace(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeading(entt::entity creature, GameHeading heading) override
	{
		++asked[creature];
		auto& registry = Locator::entitiesRegistry::value();
		if (creature_layers::IsPlaying(registry.Get<const CreatureAnimation>(creature).body))
		{
			return false;
		}
		auto& legs = registry.Get<CreatureLocomotion>(creature);
		legs.heading = heading;
		legs.started = true;
		return true;
	}
	bool TurnToFaceAtOnce(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeadingAtOnce(entt::entity, GameHeading) override { return false; }
	void Stop(entt::entity creature) override { ++stopped[creature]; }
	void StopAtOnce(entt::entity creature) override { ++stoppedAtOnce[creature]; }
	void ResetPace(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	void PlaceWithoutWalking(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return walking; }
	[[nodiscard]] float KeptFraction(entt::entity) const override { return 0.0f; }
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override
	{
		throw std::logic_error("the fights do not ask for the walkable land");
	}
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity, glm::ivec2) override { return {}; }

	/// Whether every creature walks and does not stop
	bool walking {false};
	std::map<entt::entity, size_t> asked;
	std::map<entt::entity, size_t> stopped;
	std::map<entt::entity, size_t> stoppedAtOnce;
};
} // namespace

// As a fight starts, an action that the body of a creature plays continues. The request does not wait for the body,
// but the turn to face does. The other creature turns from the third turn and the first creature from the fourth.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheFaceOffTurnWaitsForTheBodyThatPlaysAnAction)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	auto& legs = static_cast<TurningLegs&>(Locator::creatureLocomotionSystem::emplace<TurningLegs>());
	using Body = creature_layers::BodyAction;
	registry.Get<CreatureAnimation>(_first).body = Body {.kind = Body::Kind::Once, .animations = {5, 0, 0}};
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	// The first creature stops as a plan stops, and the action of the body plays on. The other creature continues.
	EXPECT_EQ(legs.stopped[_first], 1u);
	EXPECT_EQ(legs.stopped[_second], 0u);
	EXPECT_TRUE(legs.stoppedAtOnce.empty());
	EXPECT_TRUE(creature_layers::IsPlaying(registry.Get<const CreatureAnimation>(_first).body));

	// The first two turns are the request and the order not to think
	_fights.ProcessTurn();
	_fights.ProcessTurn();
	EXPECT_TRUE(legs.asked.empty());
	EXPECT_EQ(legs.stopped[_second], 0u);
	// The other creature turns from the third turn, the first creature from the fourth. Its body refuses the turn while
	// it plays the action.
	_fights.ProcessTurn();
	_fights.ProcessTurn();
	_fights.ProcessTurn();
	EXPECT_GE(legs.asked[_second], 1u);
	EXPECT_EQ(legs.stopped[_second], 1u);
	EXPECT_EQ(legs.asked[_first], 2u);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).faceTurn.phase, creature_turn_to_face::Turn::Phase::Start);

	// The action ends. The turn starts, it faces the other creature, holds for a turn, and is done. On the next turn it
	// tells the other creature to taunt.
	registry.Get<CreatureAnimation>(_first).body = {};
	int turns = 0;
	while (registry.Get<const CreatureFighting>(_first).stage == CreatureFighting::Stage::FaceOff && turns < 20)
	{
		_fights.ProcessTurn();
		++turns;
	}
	EXPECT_EQ(turns, 5);
	EXPECT_EQ(legs.asked[_first], 3u);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).faceTurn.phase, creature_turn_to_face::Turn::Phase::Done);
	Locator::creatureLocomotionSystem::reset();
}

// The other creature continues with what it did on the turn of the question. On the second turn, its mind stands
// still. On the third turn, it stops and obeys the first creature.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheAskedCreatureStandsStillOnTheSecondTurnAndObeysOnTheThird)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	auto& legs = static_cast<TurningLegs&>(Locator::creatureLocomotionSystem::emplace<TurningLegs>());
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	using Part = CreatureFighting::Part;
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).part, Part::Obeys);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).part, Part::Asked);

	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).part, Part::Asked);
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).part, Part::StandsStill);
	EXPECT_TRUE(minds.ordered.empty());
	EXPECT_EQ(legs.stopped[_second], 0u);

	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).part, Part::Obeys);
	ASSERT_EQ(minds.ordered.size(), 1u);
	EXPECT_EQ(minds.ordered.front().first, _second);
	EXPECT_EQ(minds.ordered.front().second, _first);
	EXPECT_EQ(legs.stopped[_second], 1u);
	EXPECT_NE(registry.Get<const CreatureFighting>(_second).faceTurn.phase, creature_turn_to_face::Turn::Phase::Idle);

	// The order to taunt stops the other creature again and gives it the plan to obey again
	for (int turn = 0; turn < 20 && registry.Get<const CreatureFighting>(_second).stage == CreatureFighting::Stage::FaceOff;
	     ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Taunt);
	ASSERT_EQ(minds.ordered.size(), 2u);
	EXPECT_EQ(minds.ordered.back().first, _second);
	EXPECT_EQ(legs.stopped[_second], 2u);
	EXPECT_EQ(legs.stopped[_first], 1u);
	Locator::creatureMindSystem::reset();
	Locator::creatureLocomotionSystem::reset();
}

namespace
{
/// How many times the creature pulled its angry face
size_t Angers(const ForcedMinds& minds, entt::entity creature)
{
	return static_cast<size_t>(std::ranges::count(minds.feelings, std::pair {creature, creature_face::Cue::Anger}));
}
} // namespace

// Only the creature that started the fight shows anger. In the face-off it shows anger as it asks, as it orders the
// other creature to turn, and as its own turn starts. The order to stand still and the taunt show no face.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheFirstCreatureShowsAngerAsEachStepOfTheFaceOffStarts)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	Locator::creatureLocomotionSystem::emplace<TurningLegs>();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(Angers(minds, _first), 0u);

	for (const size_t angers : {1u, 1u, 2u, 3u})
	{
		_fights.ProcessTurn();
		EXPECT_EQ(Angers(minds, _first), angers);
	}
	for (int turn = 0; turn < 20 && registry.Get<const CreatureFighting>(_first).stage == CreatureFighting::Stage::FaceOff;
	     ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(Angers(minds, _first), 3u);
	EXPECT_EQ(Angers(minds, _second), 0u);
	Locator::creatureMindSystem::reset();
	Locator::creatureLocomotionSystem::reset();
}

// In the arena, the first creature shows anger as its wait starts, as it orders the other creature to turn, as its own
// turn starts, as it orders the other creature to start its duel, and as its own duel starts
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheFirstCreatureShowsAngerAsEachStepInTheArenaStarts)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::creatureLocomotionSystem::emplace<TurningLegs>();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	using Stage = CreatureFighting::Stage;
	for (const auto self : {_first, _second})
	{
		registry.Get<CreatureFighting>(self).stage = Stage::Ready;
	}
	for (const size_t angers : {1u, 2u, 3u, 3u})
	{
		_fights.ProcessTurn();
		EXPECT_EQ(Angers(minds, _first), angers);
	}
	for (int turn = 0; turn < 20 && !registry.Get<const CreatureFighting>(_first).played; ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_EQ(Angers(minds, _first), 4u);
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, Stage::Duel);
	EXPECT_EQ(Angers(minds, _first), 5u);
	// The duel shows anger again every forty-two turns, as each step with a face does (test AStepPullsItsFaceAgain).
	// The test creatures have no bodies to make blows with, so the duel does not continue here.
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_first).angerShown);
	EXPECT_EQ(Angers(minds, _second), 0u);
	Locator::creatureMindSystem::reset();
	Locator::creatureLocomotionSystem::reset();
}

// The walk to the arena and the wait for the other creature have no time limit. A creature that walks for a long time
// does not stop the fight.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheWalkToTheArenaAndTheWaitForTheOtherCreatureHaveNoTimeLimit)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& legs = static_cast<TurningLegs&>(Locator::creatureLocomotionSystem::emplace<TurningLegs>());
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	using Stage = CreatureFighting::Stage;
	legs.walking = true;
	registry.Get<CreatureFighting>(_first).stage = Stage::Ready;
	auto& walker = registry.Get<CreatureFighting>(_second);
	walker.stage = Stage::Approach;
	walker.played = true;
	for (int turn = 0; turn < 1000; ++turn)
	{
		_fights.ProcessTurn();
	}
	ASSERT_TRUE(registry.AllOf<CreatureFighting>(_first));
	ASSERT_TRUE(registry.AllOf<CreatureFighting>(_second));
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, Stage::Ready);
	// The wait of the first creature shows anger as it starts and again every forty-two turns: 1000 turns give 24 faces
	EXPECT_EQ(Angers(minds, _first), 24u);
	EXPECT_EQ(Angers(minds, _second), 0u);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Approach);
	// The walk ends when the body stops
	legs.walking = false;
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Ready);
	Locator::creatureMindSystem::reset();
	Locator::creatureLocomotionSystem::reset();
}

// In the arena, the first creature tells the other creature to turn to face it on the turn after the other creature is
// ready, and it turns from the turn after that. On the turn after its own turn is done, it tells the other creature to
// start its duel. It starts its own duel on the next turn.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, InTheArenaBothTurnToFaceAndTheOtherCreatureStartsItsDuelFirst)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& legs = static_cast<TurningLegs&>(Locator::creatureLocomotionSystem::emplace<TurningLegs>());
	using Stage = CreatureFighting::Stage;
	using Phase = creature_turn_to_face::Turn::Phase;
	for (const auto self : {_first, _second})
	{
		registry.Get<CreatureFighting>(self).stage = Stage::Ready;
	}

	// The first creature finds the other creature ready
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, Stage::Square);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Ready);
	EXPECT_TRUE(legs.asked.empty());
	// It tells the other creature to stop and turn to face it
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Square);
	EXPECT_NE(registry.Get<const CreatureFighting>(_second).faceTurn.phase, Phase::Idle);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).faceTurn.phase, Phase::Idle);
	EXPECT_EQ(legs.stopped[_second], 1u);
	EXPECT_EQ(legs.stopped[_first], 0u);
	// It turns from the next turn
	_fights.ProcessTurn();
	EXPECT_EQ(legs.asked[_first], 1u);

	// It faces the other creature, holds for a turn, and is done. On the next turn it tells the other creature to
	// start its duel.
	int turns = 0;
	while (!registry.Get<const CreatureFighting>(_first).played && turns < 20)
	{
		_fights.ProcessTurn();
		++turns;
	}
	EXPECT_EQ(turns, 4);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, Stage::Square);
	EXPECT_EQ(legs.stopped[_second], 2u);
	// The other creature can start its duel before the first creature. It stays in its duel and waits for the first
	// creature. The first creature starts its duel on the next turn.
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, Stage::Duel);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Duel);
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, Stage::Duel);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Duel);
	Locator::creatureLocomotionSystem::reset();
}

// The other creature keeps its duel for the turns that the first creature needs to start its own duel
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFighterWaitsInItsDuelForTheOpponentThatIsAboutToStart)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::creatureLocomotionSystem::emplace<TurningLegs>();
	using Stage = CreatureFighting::Stage;
	auto& starter = registry.Get<CreatureFighting>(_first);
	starter.stage = Stage::Square;
	starter.played = true;
	// The body of the first creature plays an action, so it cannot start its duel
	using Body = creature_layers::BodyAction;
	registry.Get<CreatureAnimation>(_first).body = Body {.kind = Body::Kind::Once, .animations = {5, 0, 0}};
	for (int turn = 0; turn < 3; ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, Stage::Square);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Duel);
	registry.Get<CreatureAnimation>(_first).body = {};
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, Stage::Duel);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Duel);
	Locator::creatureLocomotionSystem::reset();
}

namespace
{
/// Keeps the kinds of the remarks asked for, and does nothing else
class RecordedGuidance final: public ecs::systems::GuidanceSystemInterface
{
public:
	void Reset() override {}
	void ProcessTurn() override {}
	bool PlayNow(guidance::Kind) override { return false; }
	bool RemarkNow(guidance::Kind) override { return false; }
	void RemarkIfNow(guidance::Kind kind) override { asked.push_back(kind); }
	void TownAttacked(entt::entity, PlayerNames, entt::entity) override {}
	void TownAttackCry(entt::entity, const guidance::TownAttackVoice&) override {}
	void BeliefVoice(glm::vec3, float, GuidanceAlignment) override {}
	void BuildingBroken(entt::entity, std::optional<PlayerNames>, float) override {}
	void AlignmentTurn(float) override {}
	void SayOneOff(guidance::OneOff) override {}
	void KilledVillager(entt::entity) override {}
	void CreatureWonFight(entt::entity creature) override { wonFight.push_back(creature); }
	void WorshipperDied() override {}
	void LosingVillager(entt::entity) override {}
	void LowOnPeople(entt::entity) override {}
	void TownChangedHands(entt::entity, PlayerNames, PlayerNames) override {}
	void LosingBeliefTurn() override {}
	void EndWaits() override {}
	[[nodiscard]] const std::vector<Remark>& GetRemarks() const override { return remarks; }
	[[nodiscard]] int32_t GetMoonWait() const override { return 0; }
	[[nodiscard]] const night_whisper::Whisperer& GetNightWhisper() const override { return whisper; }
	void StageNightWhisper(uint32_t) override {}

	std::vector<guidance::Kind> asked;
	night_whisper::Whisperer whisper;
	std::vector<entt::entity> wonFight;
	std::vector<Remark> remarks;
};
} // namespace

TEST_F(CreatureFightSystemTest, TheAdvisorsSpeakOfAFightOfThePlayersCreatureTooFarToShow)
{
	auto& guidance = static_cast<RecordedGuidance&>(Locator::guidanceSystem::emplace<RecordedGuidance>());
	_fights.Reset();
	auto& camera = Locator::camera::value();
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_FALSE(_fights.IsCameraOnFight());
	EXPECT_EQ(guidance.asked, std::vector {guidance::Kind::CreatureFight});
	// A fight near enough is shown, and the advisors say nothing
	guidance.asked.clear();
	camera.SetOrigin({0.0f, 30.0f, 40.0f});
	camera.SetFocus({0.0f, 0.0f, 0.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(_fights.IsCameraOnFight());
	EXPECT_TRUE(guidance.asked.empty());
	// A fight far away between creatures of other players is not spoken of
	_fights.Reset();
	Locator::entitiesRegistry::value().Get<Creature>(_first).owner = PlayerNames::PLAYER_THREE;
	camera.SetOrigin({0.0f, 30.0f, 2000.0f});
	camera.SetFocus({0.0f, 0.0f, 1900.0f});
	ShowTheFight({0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(guidance.asked.empty());
	Locator::guidanceSystem::reset();
}

TEST_F(CreatureFightSystemTest, AnOpponentWithNoLifeLeftLosesTheDuelToTheCreatureWhoseTurnItIs)
{
	auto& guidance = static_cast<RecordedGuidance&>(Locator::guidanceSystem::emplace<RecordedGuidance>());
	auto& reactions = static_cast<RecordedReactions&>(Locator::reactionSystem::emplace<RecordedReactions>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
	}
	// The second creature has no life left, but its fight health is not zero
	registry.Assign<CreatureNeeds>(_second).needs.life = 0.0f;
	// A creature that a script controls continues its duel
	registry.Assign<ScriptControlled>(_first);
	_fights.FightTurn(_first);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, CreatureFighting::Stage::Duel);
	registry.Remove<ScriptControlled>(_first);
	_fights.FightTurn(_first);
	const auto& won = registry.Get<const CreatureFighting>(_first);
	EXPECT_TRUE(won.ended);
	EXPECT_TRUE(won.fighter.stopping);
	EXPECT_EQ(won.stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(_first).wins, 1u);
	EXPECT_EQ(guidance.wonFight, std::vector {_first});
	ASSERT_EQ(reactions.made.size(), 1u);
	EXPECT_EQ(reactions.made.front().type, Reaction::ReactToFightWon);
	EXPECT_EQ(reactions.made.front().initiator, _first);
	EXPECT_EQ(reactions.made.front().player, PlayerNames::PLAYER_ONE);
	// The loser does not faint
	EXPECT_FALSE(_fights.IsKnockedOut(_second));
	Locator::reactionSystem::reset();
	Locator::guidanceSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheOrderToRespondAfterAWinByLifeStopsTheLoserWhichRespondsAlone)
{
	auto& body = static_cast<CountedGains&>(Locator::creaturePhysiologySystem::emplace<CountedGains>());
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
	}
	registry.Assign<CreatureNeeds>(_first).needs.life = 1.0f;
	registry.Assign<CreatureNeeds>(_second).needs.life = 0.0f;
	_fights.FightTurn(_first);
	ASSERT_EQ(registry.Get<const CreatureFighting>(_first).stage, CreatureFighting::Stage::Respond);
	// The loser stays in its duel against the winner, with no response and no pay from its life
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).partnerLeft, CreatureFighting::PartnerLeft::Won);
	_fights.FightTurn(_second);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Duel);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(_second).ended);
	// Some life comes back to the loser, so that each payment of its life shows
	registry.Get<CreatureNeeds>(_second).needs.life = 0.5f;
	// The body of the winner comes to rest: its duel step ends, and it plays no response
	fight::Enter(registry.Get<CreatureFighting>(_first).fighter, fight::State::Idle);
	_fights.FightTurn(_first);
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_first).restedAfterDuel);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(_first).played);
	EXPECT_TRUE(body.gains.empty());
	// On the next turn, its order to respond stops the loser. The plan of the winner fails, and it leaves the fight.
	_fights.FightTurn(_first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(_first));
	EXPECT_EQ(body.gains[_second], 1u);
	EXPECT_EQ(body.gains[_first], 2u);
	EXPECT_EQ(minds.abandoned, std::vector {_first});
	ASSERT_EQ(minds.ordered.size(), 1u);
	EXPECT_EQ(minds.ordered.front(), std::pair(_second, _first));
	// The loser leaves its duel two times in the stop. It stands in its stance, so its life pays two times.
	const auto& ordered = registry.Get<const CreatureFighting>(_second);
	EXPECT_EQ(ordered.stage, CreatureFighting::Stage::Respond);
	EXPECT_TRUE(ordered.ended);
	EXPECT_TRUE(ordered.fighter.stopping);
	EXPECT_TRUE(ordered.released);
	EXPECT_TRUE(ordered.mindFree);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(0.5f, 0.6f, 2));
	// It finishes its move and plays its response. With less life than the winner, it fears the winner.
	fight::Enter(registry.Get<CreatureFighting>(_second).fighter, fight::State::Idle);
	_fights.FightTurn(_second);
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_second).played);
	registry.Get<CreatureAnimation>(_second).body = {};
	_fights.FightTurn(_second);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(_second));
	ASSERT_EQ(minds.desired.size(), 1u);
	EXPECT_EQ(minds.desired.front(), std::pair(_second, creature_desires::Desire::Fear));
	Locator::creatureMindSystem::reset();
	Locator::creaturePhysiologySystem::reset();
}

TEST_F(CreatureFightSystemTest, ALoserWithMoreLifeThanTheWinnerWaitsAfterItsResponseUntilItsMindChoosesAPlan)
{
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 1.0f;
	}
	registry.Assign<CreatureNeeds>(_first).needs.life = 0.2f;
	registry.Assign<CreatureNeeds>(_second).needs.life = 0.0f;
	_fights.FightTurn(_first);
	registry.Get<CreatureNeeds>(_second).needs.life = 0.8f;
	fight::Enter(registry.Get<CreatureFighting>(_first).fighter, fight::State::Idle);
	_fights.FightTurn(_first);
	_fights.FightTurn(_first);
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(_first));
	fight::Enter(registry.Get<CreatureFighting>(_second).fighter, fight::State::Idle);
	_fights.FightTurn(_second);
	registry.Get<CreatureAnimation>(_second).body = {};
	// No fear: it waits with its free mind, and nothing ends its plan to obey
	for (uint32_t turn = 0; turn < 10; ++turn)
	{
		_fights.FightTurn(_second);
	}
	ASSERT_TRUE(registry.AllOf<CreatureFighting>(_second));
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_second).responded);
	EXPECT_TRUE(minds.desired.empty());
	EXPECT_TRUE(minds.obeyEnded.empty());
	// A plan that its mind chooses ends the fight. Its body is at rest, so it leaves at once.
	_fights.CallOff(_second);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(_second));
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheFightStepsWaitWhileTheHandHoldsTheCreatureAndTheFightStopsWhenTheHandLetsGo)
{
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	// The first creature responds to a fight that ended, and the second still duels
	{
		auto& responds = registry.Get<CreatureFighting>(_first);
		responds.stage = CreatureFighting::Stage::Respond;
		responds.ended = true;
	}
	fight::Enter(registry.Get<CreatureFighting>(_second).fighter, fight::State::Stance);
	registry.Get<CreatureFighting>(_second).fighter.health = 0.6f;
	registry.Assign<CreatureNeeds>(_second).needs.life = 1.0f;
	hand.held = _first;
	_fights.HandTookHold(_first);
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_first).held);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, CreatureFighting::Stage::Respond);
	// While held, its response does not start
	_fights.FightTurn(_first);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(_first).played);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stageSeconds, 0.0f);
	// Let go, it leaves the fight with no response. It stops its opponent too.
	hand.held.reset();
	_fights.HandLetGo(_first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(_first));
	const auto& other = registry.Get<const CreatureFighting>(_second);
	EXPECT_EQ(other.stage, CreatureFighting::Stage::Stop);
	EXPECT_TRUE(other.fighter.stopping);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterFight(1.0f, 0.6f));
	Locator::creatureHandSystem::reset();
}

TEST_F(CreatureFightSystemTest, ACreatureThatTheHandHoldsInADuelLeavesTheDuelAndItsOpponentFightsOnUntilTheHandLetsGo)
{
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
		registry.Assign<CreatureNeeds>(self).needs.life = 1.0f;
	}
	hand.held = _first;
	// The fights see the hold on the first turn of the hold
	_fights.FightTurn(_first);
	const auto& held = registry.Get<const CreatureFighting>(_first);
	EXPECT_EQ(held.stage, CreatureFighting::Stage::Stop);
	EXPECT_TRUE(held.ended);
	EXPECT_TRUE(held.fighter.stopping);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterFight(1.0f, 0.6f));
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).partnerLeft, CreatureFighting::PartnerLeft::Held);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Duel);
	// The held creature finishes its move, but it does not leave the fight while held
	fight::Enter(registry.Get<CreatureFighting>(_first).fighter, fight::State::Idle);
	_fights.FightTurn(_first);
	EXPECT_TRUE(registry.AllOf<CreatureFighting>(_first));
	// Let go, it leaves the fight, and its opponent stops with no response
	hand.held.reset();
	_fights.HandLetGo(_first);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(_first));
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Stop);
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_second).ended);
	Locator::creatureHandSystem::reset();
}

// After the hold, the plan asks the opponent again. The opponent agrees, and the fight starts when its move finished.
TEST_F(CreatureFightSystemTest, AFightAskedAgainAfterAHoldWaitsForTheMoveThatFinishes)
{
	using Result = ecs::systems::CreatureFightSystemInterface::StartResult;
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Assign<CreatureNeeds>(self).needs.life = 1.0f;
	}
	hand.held = _first;
	_fights.FightTurn(_first);
	// While the hand holds it, the fight is not only a move that finishes
	EXPECT_EQ(_fights.StartFight(_first, _second), Result::Busy);
	fight::Enter(registry.Get<CreatureFighting>(_first).fighter, fight::State::Idle);
	hand.held.reset();
	_fights.HandLetGo(_first);
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(_first));
	ASSERT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Stop);
	// The opponent still finishes its move: it agrees, but nothing starts
	EXPECT_EQ(_fights.StartFight(_first, _second), Result::Finishing);
	EXPECT_FALSE(registry.AllOf<CreatureFighting>(_first));
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Stop);
	// Its move finished, it leaves the fight, and the new fight starts
	fight::Enter(registry.Get<CreatureFighting>(_second).fighter, fight::State::Idle);
	_fights.ProcessTurn();
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(_second));
	EXPECT_EQ(_fights.StartFight(_first, _second), Result::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::FaceOff);
	Locator::creatureHandSystem::reset();
}

TEST_F(CreatureFightSystemTest, APlannedFightThatWaitsForAMoveAsksTheOtherCreatureOneTimeOnly)
{
	using Result = ecs::systems::CreatureFightSystemInterface::StartResult;
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Assign<CreatureNeeds>(self).needs.life = 1.0f;
	}
	hand.held = _first;
	_fights.FightTurn(_first);
	fight::Enter(registry.Get<CreatureFighting>(_first).fighter, fight::State::Idle);
	hand.held.reset();
	_fights.HandLetGo(_first);
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(_first));
	ASSERT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Stop);
	// The first question: the other creature agrees, and its move still finishes
	EXPECT_EQ(_fights.StartPlannedFight(_first, _second, false), Result::Finishing);
	EXPECT_EQ(minds.deathChecks, 1u);
	// Now only the other creature is under a script's control, so a new question would get a refusal. The step keeps
	// the answer that it got and does not ask again.
	registry.Assign<ScriptControlled>(_second);
	EXPECT_EQ(_fights.StartPlannedFight(_first, _second, true), Result::Finishing);
	EXPECT_EQ(_fights.StartPlannedFight(_first, _second, true), Result::Finishing);
	EXPECT_EQ(minds.deathChecks, 1u);
	// Its move finished, it leaves the fight, and the fight starts on the answer that it kept
	fight::Enter(registry.Get<CreatureFighting>(_second).fighter, fight::State::Idle);
	_fights.ProcessTurn();
	ASSERT_FALSE(registry.AllOf<CreatureFighting>(_second));
	EXPECT_EQ(_fights.StartPlannedFight(_first, _second, true), Result::Started);
	EXPECT_EQ(minds.deathChecks, 1u);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::FaceOff);
	Locator::creatureHandSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheFightMusicPlaysWhileTheHandDirectsTheFightOrNearItsArena)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(_fights.DirectsFight());
	EXPECT_TRUE(_fights.FightMusicHeard({0.0f, 30.0f, 2000.0f}));
	_fights.Reset();
	// The arena of the fight, seen from near and from far
	const auto arena = registry.Get<const CreatureFighting>(_first).arenaEntity;
	registry.Assign<Transform>(arena, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<CreatureArena>(arena, CreatureArena {.place = {0, 0},
	                                                     .radius = 40.0f,
	                                                     .temporary = true,
	                                                     .fightOn = true,
	                                                     .first = _first,
	                                                     .second = _second,
	                                                     .ring = nullptr});
	EXPECT_TRUE(_fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
	EXPECT_FALSE(_fights.FightMusicHeard({0.0f, 30.0f, 2000.0f}));
	// No fight is on in the arena, or the player's creature is not in it
	registry.Get<CreatureArena>(arena).fightOn = false;
	EXPECT_FALSE(_fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
	registry.Get<CreatureArena>(arena).fightOn = true;
	registry.Get<Creature>(_first).owner = PlayerNames::PLAYER_THREE;
	EXPECT_FALSE(_fights.FightMusicHeard({0.0f, 30.0f, 90.0f}));
}

TEST_F(CreatureFightSystemTest, AScriptFindsAnArenaNearForThingsThatAreNotCreaturesButMakesNoNewOneForThem)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto arena = registry.Create();
	registry.Assign<CreatureArena>(arena, CreatureArena {.place = {0, 0}, .radius = 40.0f});
	const auto rock = registry.Create();
	// The search gives the arena near the point as it is, also when one of the two things is not a creature
	const auto found = _fights.FindOrMakeArena({0.0f, 0.0f, 0.0f}, rock, _first, 10.0f);
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->arena, arena);
	EXPECT_FALSE(found->made);
	// With no arena near, only two creatures make a new one
	EXPECT_FALSE(_fights.FindOrMakeArena({1000.0f, 0.0f, 1000.0f}, rock, _first, 10.0f).has_value());
	EXPECT_FALSE(_fights.FindOrMakeArena({1000.0f, 0.0f, 1000.0f}, _first, rock, 10.0f).has_value());
}

TEST_F(CreatureFightSystemTest, AFightGestureIsTakenOnlyWhenTheLineToTheOpponentMeetsItsBody)
{
	// Neither creature has a body, so the line from the player's creature meets nothing
	EXPECT_FALSE(_fights.GestureSpecialMove());
	EXPECT_FALSE(_fights.GestureSpell(MagicType::Fireball));
	EXPECT_TRUE(QueueOf(_first).Empty());
	// Without a duel of the player's creature, no gesture is taken
	Locator::entitiesRegistry::value().Get<Creature>(_first).owner = PlayerNames::PLAYER_THREE;
	EXPECT_EQ(_fights.PlayersFighter(), std::nullopt);
	EXPECT_FALSE(_fights.GestureSpecialMove());
}

TEST_F(CreatureFightSystemTest, ThePlayerDirectsTheirOwnCreatureAndTheComputerAnyOther)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	// The other creature starts the fight: who starts it does not matter
	ASSERT_EQ(_fights.StartFight(_second, _first), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).fighter.control, fight::Control::Player);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).fighter.control, fight::Control::Computer);
	EXPECT_TRUE(_fights.IsAutoFighting(_first));
	EXPECT_TRUE(_fights.IsAutoFighting(_second));
}

TEST_F(CreatureFightSystemTest, EachHumanGodDirectsTheirOwnCreatureInANetworkGame)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_ONE] = {registry.Create(), registry.Create()};
	hands.hands[PlayerNames::PLAYER_TWO] = {registry.Create(), registry.Create()};
	// The god of this computer is the second player: the computer has no part in it
	hands.own = PlayerNames::PLAYER_TWO;
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).fighter.control, fight::Control::Player);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).fighter.control, fight::Control::Player);
}

TEST_F(CreatureFightSystemTest, ABlockTeachesTheFirstPlayerAloneAndEachHumanGodInANetworkGame)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_ONE] = {registry.Create(), registry.Create()};
	hands.hands[PlayerNames::PLAYER_TWO] = {registry.Create(), registry.Create()};
	auto& time = static_cast<FakeTime&>(Locator::time::emplace<FakeTime>());
	// Both creatures stand in their stance near the middle of the arena, where they take moves from the queue
	for (const auto creature : {_first, _second})
	{
		registry.Assign<CreatureFightRecord>(creature);
		registry.Get<CreatureFighting>(creature).fighter.state = fight::State::Stance;
	}
	registry.Get<Transform>(_first).position = {-1.0f, 0.0f, 0.0f};
	registry.Get<Transform>(_second).position = {1.0f, 0.0f, 0.0f};
	const auto tendencyOf = [&](entt::entity creature) { return registry.Get<const CreatureFightRecord>(creature).tendency; };
	// A game alone: the block of the second player does not teach its creature, the block of the first player does
	_fights.DoMove(PlayerNames::PLAYER_TWO, _first, fight::BlockMove(), false);
	EXPECT_EQ(tendencyOf(_second), 0.0f);
	_fights.DoMove(PlayerNames::PLAYER_ONE, _second, fight::BlockMove(), false);
	EXPECT_EQ(tendencyOf(_first), fight::LearnTendency(0.0f, fight::Move::Kind::Block));
	// A network game: the block of the second player teaches its creature, also on the computer of the first player
	time.networkGame = true;
	registry.Get<CreatureFighting>(_second).fighter = {};
	registry.Get<CreatureFighting>(_second).fighter.state = fight::State::Stance;
	_fights.DoMove(PlayerNames::PLAYER_TWO, _first, fight::BlockMove(), false);
	EXPECT_EQ(tendencyOf(_second), fight::LearnTendency(0.0f, fight::Move::Kind::Block));
}

TEST_F(CreatureFightSystemTest, AGodWithNoHandsLeavesItsCreatureToTheComputer)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_TWO] = {registry.Create(), registry.Create()};
	hands.own = PlayerNames::PLAYER_TWO;
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).fighter.control, fight::Control::Computer);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).fighter.control, fight::Control::Player);
}

TEST_F(CreatureFightSystemTest, AComputerGodLeavesItsCreatureToTheComputerAlsoWithHands)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_ONE] = {registry.Create(), registry.Create()};
	hands.hands[PlayerNames::PLAYER_THREE] = {registry.Create(), registry.Create()};
	auto& players = static_cast<FakePlayers&>(Locator::playerSystem::emplace<FakePlayers>());
	players.computers.insert(PlayerNames::PLAYER_THREE);
	registry.Get<Creature>(_second).owner = PlayerNames::PLAYER_THREE;
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	// The computer god's creature starts the fight with the creature of the player
	ASSERT_EQ(_fights.StartFight(_second, _first), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).fighter.control, fight::Control::Player);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).fighter.control, fight::Control::Computer);
}

TEST_F(CreatureFightSystemTest, TheHandOfThisComputerDirectsTheCreatureOfItsOwnGod)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	hands.hands[PlayerNames::PLAYER_ONE] = {registry.Create(), registry.Create()};
	hands.hands[PlayerNames::PLAYER_TWO] = {registry.Create(), registry.Create()};
	hands.own = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(_fights.PlayersFighter(), _second);
	EXPECT_EQ(_fights.HandTip(_second), fight::Tip::Block);
	EXPECT_EQ(_fights.HandTip(_first), fight::Tip::Attack);
	// With no queue of commands, the press goes to the creature of the god of this computer
	EXPECT_TRUE(PressGround({15.0f, 10.0f}, fight::Button::Action));
	_fights.Release(0, 0);
	EXPECT_EQ(QueueOf(_second).Size(), 1u);
	EXPECT_TRUE(QueueOf(_first).Empty());
}

TEST_F(CreatureFightSystemTest, TheFightCheatStopsTheDuelOfTheSendersCreature)
{
	Locator::reactionSystem::emplace<RecordedReactions>();
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
	}
	_fights.DoFightCheat(PlayerNames::PLAYER_TWO);
	// The duel stops with no winner
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Respond);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, CreatureFighting::Stage::Respond);
	Locator::reactionSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheFightCheatStartsAFightWithTheNearestCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	// A third creature farther away is not the one that it fights
	MakeCreature(registry, PlayerNames::PLAYER_THREE, {200.0f, 0.0f, 0.0f});
	_fights.DoFightCheat(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_fights.OpponentOf(_second), _first);
	EXPECT_EQ(_fights.OpponentOf(_first), _second);
	// A player with no creature does nothing
	_fights.DoFightCheat(PlayerNames::PLAYER_FOUR);
}

TEST_F(CreatureFightSystemTest, LifeLostInADuelTakesTheFightHealthAndCanKnockTheCreatureOut)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::creatureMindSystem::emplace<ForcedMinds>();
	registry.Get<CreatureFighting>(_first).fighter.health = 0.5f;
	_fights.LoseFightHealth(_first, 0.2f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(_first).fighter.health, 0.3f);
	EXPECT_FALSE(_fights.IsKnockedOut(_first));
	// With none left it is knocked out, and its opponent wins
	_fights.LoseFightHealth(_first, 0.5f);
	EXPECT_TRUE(_fights.IsKnockedOut(_first));
	EXPECT_EQ(registry.Get<const CreatureFightRecord>(_second).wins, 1u);
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, LifeLostOutOfADuelLeavesTheFightHealth)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<CreatureFighting>(_first).stage = CreatureFighting::Stage::Taunt;
	const auto before = registry.Get<const CreatureFighting>(_first).fighter.health;
	_fights.LoseFightHealth(_first, 0.2f);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureFighting>(_first).fighter.health, before);
}

// As the fight starts, the game's random numbers choose the taunt of each creature, the other creature's first. Each try
// to start the taunt draws a side first, and a body that plays an action refuses the try until it is free.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheGameChoosesTheTauntsAndEachTryDrawsASide)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	game.numbers = {1, 0};
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	EXPECT_EQ(game.ranges, (std::vector<uint32_t> {2, 2}));
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_second).rude);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(_first).rude);

	// Both taunt, so the other creature obeys already
	registry.Get<CreatureFighting>(_first).stage = CreatureFighting::Stage::Taunt;
	registry.Get<CreatureFighting>(_second).stage = CreatureFighting::Stage::Taunt;
	registry.Get<CreatureFighting>(_second).part = CreatureFighting::Part::Obeys;
	auto& body = registry.Get<CreatureAnimation>(_first).body;
	body.kind = creature_layers::BodyAction::Kind::Once;
	for (int turn = 0; turn < 3; ++turn)
	{
		_fights.ProcessTurn();
	}
	// The busy creature draws on each turn and does not taunt; the other draws once, taunts and goes on
	EXPECT_FALSE(registry.Get<const CreatureFighting>(_first).played);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, CreatureFighting::Stage::Taunt);
	EXPECT_NE(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Taunt);
	EXPECT_EQ(game.ranges.size(), 2u + 3u + 1u);
	Locator::gameRandom::reset();
}

// A faint starts on the body on the next turn of the knock-out. It cuts what the body plays and lies on the side that
// one draw of the game's random numbers chooses then.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintCutsTheBodyAndDrawsItsSide)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	game.numbers = {1};
	registry.Get<CreatureAnimation>(_second).body.kind = creature_layers::BodyAction::Kind::Once;
	_fights.ForceFaint(_second);
	EXPECT_TRUE(game.ranges.empty());
	EXPECT_EQ(registry.Get<const CreatureAnimation>(_second).body.kind, creature_layers::BodyAction::Kind::Once);
	_fights.ProcessTurn();
	const auto& body = registry.Get<const CreatureAnimation>(_second).body;
	EXPECT_EQ(body.kind, creature_layers::BodyAction::Kind::Sequence);
	EXPECT_TRUE(body.mirrored);
	EXPECT_EQ(game.ranges, (std::vector<uint32_t> {2}));
	Locator::gameRandom::reset();
}

// A faint that does not come from the mind first checks the hands. With a thing in the hand, the creature starts to let
// go, and the faint starts on the next turn with no new check. While the body or the hands are busy, the faint waits.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintWaitsForTheLetGoOfTheThingInTheHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	using creature_mind::HandsCheck;
	minds.hands = {HandsCheck::Wait, HandsCheck::Wait, HandsCheck::LetGo};
	_fights.ForceFaint(_second);
	const auto faints = [&] {
		return registry.Get<const CreatureAnimation>(_second).body.kind == creature_layers::BodyAction::Kind::Sequence;
	};
	// Two turns busy, then the let-go starts. The other creature, which stays in the fight, can draw the side of its
	// response in these turns.
	for (size_t turn = 1; turn <= 3; ++turn)
	{
		_fights.ProcessTurn();
		EXPECT_FALSE(faints());
		EXPECT_EQ(minds.handChecks, turn);
	}
	EXPECT_TRUE(registry.Get<const CreatureKnockedOut>(_second).handsChecked);
	// The faint starts with no new check and draws its side
	const auto drawn = game.ranges.size();
	_fights.ProcessTurn();
	EXPECT_TRUE(faints());
	EXPECT_EQ(minds.handChecks, 3u);
	ASSERT_EQ(game.ranges.size(), drawn + 1);
	EXPECT_EQ(game.ranges.back(), 2u);
	EXPECT_FALSE(registry.Get<const CreatureKnockedOut>(_second).fainting);
	Locator::gameRandom::reset();
	Locator::creatureMindSystem::reset();
}

// The knocked-out creature counts the time that it lies only from when it lies still: not while the faint waits for the
// hands, and not while the faint plays before it lies still.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AKnockedOutCreatureCountsItsTimeFromWhenItLiesStill)
{
	auto& registry = Locator::entitiesRegistry::value();
	_fights.ForceFaint(_second);
	_fights.ProcessTurn();
	_fights.ProcessTurn();
	const auto& knockedOut = registry.Get<const CreatureKnockedOut>(_second);
	EXPECT_LT(knockedOut.seconds, 0.0f);
	// It lies still: from now its time counts on each turn
	registry.Get<CreatureAnimation>(_second).body.phase = creature_layers::BodyAction::Phase::Loop;
	_fights.ProcessTurn();
	const auto first = knockedOut.seconds;
	_fights.ProcessTurn();
	EXPECT_GT(knockedOut.seconds, first);
	EXPECT_EQ(knockedOut.stage, CreatureKnockedOut::Stage::Lying);
}

// The faint stops a walk at once as it starts on the body. An action of the hands can start a walk between the
// knock-out and the faint: if the walk stayed, the creature would get up to follow it and never be taken home.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintStopsAWalkThatStartedAfterTheKnockOut)
{
	auto& legs = static_cast<StoppingLegs&>(Locator::creatureLocomotionSystem::emplace<StoppingLegs>());
	_fights.ForceFaint(_second);
	// The knock-out stops the walk that the creature had
	ASSERT_EQ(legs.stopped, (std::vector<entt::entity> {_second}));
	// The faint starts on the next turn and stops the walk again
	_fights.ProcessTurn();
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const CreatureAnimation>(_second).body.kind,
	          creature_layers::BodyAction::Kind::Sequence);
	EXPECT_EQ(legs.stopped, (std::vector<entt::entity> {_second, _second}));
	Locator::creatureLocomotionSystem::reset();
}

namespace
{
/// Every animation lasts one second; the fakes pose nothing
class SecondLongAnimations final: public ecs::systems::CreatureAnimationSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(std::chrono::duration<float, std::milli>) override {}
	void PoseTurn(entt::entity) override {}
	void PoseTurns() override {}
	void HoldTurnPose(entt::entity) override {}
	void PoseCopy(ecs::Registry&, entt::entity, float, const ecs::Registry&) override {}
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity, size_t, float, uint32_t, bool) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> MiddleOfBonesInAnimation(entt::entity, size_t, float,
	                                                                std::span<const uint32_t>) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity, size_t) override { return 1000.0f; }
	[[nodiscard]] std::optional<glm::vec3> AnimationTravel(entt::entity, size_t) override { return std::nullopt; }
	[[nodiscard]] std::optional<creature_kiss::KissPoints> KissPointsOf(entt::entity) override { return std::nullopt; }
	void KickSway(entt::entity, glm::vec3, glm::vec3) override {}
	void PlaceHandGlows(Registry&, entt::entity, int32_t, int32_t) override {}
	void SetCreedPower(entt::entity, creature_hand_glow::Place, float, float) override {}
	void LoadCreedPowers(entt::entity,
	                     std::span<const creature_hand_glow::SavedPower, creature_hand_glow::k_PlaceCount>) override
	{
	}
	void PlayCreedEvent(entt::entity, int32_t) override {}
	[[nodiscard]] std::optional<glm::vec3> MiddleOfGroinAndLips(const Registry&, entt::entity) override { return std::nullopt; }
	void AddAlignmentMotesOf(const Registry&, entt::entity) override {}
};

/// Both fighters are in a state, and only scripts would move them
void PutInStates(Registry& registry, entt::entity first, fight::State firstState, entt::entity second)
{
	for (const auto& [self, state] : {std::pair(first, firstState), std::pair(second, fight::State::Stance)})
	{
		auto& fighter = registry.Get<CreatureFighting>(self).fighter;
		fight::Enter(fighter, state);
		fighter.control = fight::Control::None;
	}
}
} // namespace

// The start ends when its time and the span of the turn reach its end, before it moves on. The stance that comes after
// it moves on in the same turn.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheStartEndsByTheSpanAndTheStanceMovesOnInTheSameTurn)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, _first, fight::State::Start, _second);
	auto& fighter = registry.Get<CreatureFighting>(_first).fighter;
	// A creature of size 1: a span of 117 ms in a turn of 100 ms
	const auto span = fight::TestSpanMs(100.0f, ShownSize(registry.Get<const Creature>(_first)), false);
	fighter.timeMs = static_cast<float>(1000 - span - 1);
	_fights.ProcessTurn();
	// The start plays on by the span
	EXPECT_EQ(fighter.state, fight::State::Start);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(1000 - 1));
	fighter.timeMs = static_cast<float>(1000 - span);
	_fights.ProcessTurn();
	EXPECT_EQ(fighter.state, fight::State::Stance);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(fight::TurnAdvance(fight::PlaybackSpeed(fight::animations::k_Stance), 100)));
	Locator::creatureAnimationSystem::reset();
}

// A fighter that wins as it turns on the spot in its start ends the turn, plays the start to its end and finishes. Its
// fight stage is not the duel any more.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AWinnerThatTurnsInItsStartPlaysTheStartAndFinishes)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, _first, fight::State::Start, _second);
	auto& fighting = registry.Get<CreatureFighting>(_first);
	fighting.stage = CreatureFighting::Stage::Respond;
	fighting.fighter.turning = true;
	fighting.fighter.stopping = true;
	_fights.ProcessTurn();
	// The turn ended: the start plays from its beginning
	EXPECT_FALSE(fighting.fighter.turning);
	EXPECT_EQ(fighting.fighter.state, fight::State::Start);
	EXPECT_GT(fighting.fighter.timeMs, 0.0f);
	fighting.fighter.timeMs = 1000.0f;
	_fights.ProcessTurn();
	EXPECT_EQ(fighting.fighter.state, fight::State::Finish);
	Locator::creatureAnimationSystem::reset();
}

// The casting pose ends at the start of the turn after it began. The end of the cast moves on in the same turn.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheEndOfACastMovesOnInTheTurnThatThePoseEnds)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, _first, fight::State::CastStart, _second);
	auto& fighter = registry.Get<CreatureFighting>(_first).fighter;
	const auto advance = fight::TurnAdvance(fight::PlaybackSpeed(fight::animations::k_StartCast), 100);
	fighter.timeMs = static_cast<float>(1000 - advance);
	_fights.ProcessTurn();
	// The start of the cast ends; the pose begins and moves on
	EXPECT_EQ(fighter.state, fight::State::Cast);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(advance));
	_fights.ProcessTurn();
	EXPECT_EQ(fighter.state, fight::State::CastEnd);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(advance));
	Locator::creatureAnimationSystem::reset();
}

// A fight called off finishes from the stance at once, and the finish moves on in the same turn. The finish plays at
// the fight's speed and ends when its time and the span reach its end.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheFinishMovesOnAtOnceAndEndsByTheSpan)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, _first, fight::State::Stance, _second);
	auto& fighter = registry.Get<CreatureFighting>(_first).fighter;
	fighter.stopping = true;
	_fights.ProcessTurn();
	const auto advance = fight::TurnAdvance(fight::PlaybackSpeed(fight::animations::k_Finish), 100);
	EXPECT_EQ(fighter.state, fight::State::Finish);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(advance));
	const auto span = fight::TestSpanMs(100.0f, ShownSize(registry.Get<const Creature>(_first)), false);
	fighter.timeMs = static_cast<float>(1000 - span);
	_fights.ProcessTurn();
	EXPECT_EQ(fighter.state, fight::State::Idle);
	Locator::creatureAnimationSystem::reset();
}

// A frozen creature fights with fewer milliseconds in each turn: its animations move on by its own turn's time
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFrozenFighterMovesOnByItsOwnShorterTurn)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, _first, fight::State::Stance, _second);
	registry.Get<CreatureAnimation>(_first).playbackScale = 0.5f;
	auto& fighter = registry.Get<CreatureFighting>(_first).fighter;
	auto& other = registry.Get<CreatureFighting>(_second).fighter;
	fighter.timeMs = 0.0f;
	other.timeMs = 0.0f;
	_fights.ProcessTurn();
	const auto speed = fight::PlaybackSpeed(fight::animations::k_Stance);
	EXPECT_EQ(fighter.timeMs, static_cast<float>(fight::TurnAdvance(speed, 50)));
	EXPECT_EQ(other.timeMs, static_cast<float>(fight::TurnAdvance(speed, 100)));
	// Frozen through, it does not move on at all
	registry.Get<CreatureAnimation>(_first).playbackScale = 0.0f;
	const auto before = fighter.timeMs;
	_fights.ProcessTurn();
	EXPECT_EQ(fighter.timeMs, before);
	Locator::creatureAnimationSystem::reset();
}

// The start of a fight stops the plan of the first creature, and each order stops the plan of the other creature. Each
// stop also stops the body: an action that the body started plays on, and a point plays its end.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheStartAndEachOrderStopTheHandsAsAPlanStopDoes)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	auto& hands = static_cast<RecordedHands&>(Locator::creatureObjectActionSystem::emplace<RecordedHands>());
	Locator::creatureLocomotionSystem::emplace<TurningLegs>();
	Locator::creatureMindSystem::emplace<ForcedMinds>();
	ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
	ASSERT_EQ(hands.planStops.size(), 1u);
	EXPECT_EQ(hands.planStops.front(), std::make_pair(_first, true));

	// The first order comes on the third turn of the face-off
	for (int turn = 0; turn < 3; ++turn)
	{
		_fights.ProcessTurn();
	}
	ASSERT_EQ(hands.planStops.size(), 2u);
	EXPECT_EQ(hands.planStops.back(), std::make_pair(_second, true));
	Locator::creatureMindSystem::reset();
	Locator::creatureLocomotionSystem::reset();
	Locator::creatureObjectActionSystem::reset();
}

// When the first creature stops the fight before its first order, what the other creature does stops as a failure.
// When the other creature stops the fight itself, or obeys already, the first creature does not stop it.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AnEarlyEndByTheFirstCreatureStopsWhatTheAskedCreatureDoes)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& legs = static_cast<TurningLegs&>(Locator::creatureLocomotionSystem::emplace<TurningLegs>());
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	const auto start = [&]() {
		registry.Remove<CreatureFighting>(_first);
		registry.Remove<CreatureFighting>(_second);
		minds.obeyEnded.clear();
		ASSERT_EQ(_fights.StartFight(_first, _second), ecs::systems::CreatureFightSystemInterface::StartResult::Started);
		_fights.ProcessTurn();
	};

	start();
	_fights.CallOff(_first);
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_second));
	ASSERT_EQ(minds.abandoned.size(), 1u);
	EXPECT_EQ(minds.abandoned.front(), _second);
	EXPECT_EQ(legs.stopped[_second], 1u);

	// The second turn tells the other creature to stand still. A spell or the hand stops the fight of the first
	// creature.
	start();
	_fights.ProcessTurn();
	_fights.AbortFight(_first);
	ASSERT_EQ(minds.abandoned.size(), 2u);
	EXPECT_EQ(minds.abandoned.back(), _second);

	// The other creature stops the fight by its own new plan. The plan of the first creature fails.
	start();
	_fights.CallOff(_second);
	ASSERT_EQ(minds.abandoned.size(), 3u);
	EXPECT_EQ(minds.abandoned.back(), _first);

	// The other creature obeys from the third turn
	start();
	_fights.ProcessTurn();
	_fights.ProcessTurn();
	ASSERT_EQ(registry.Get<const CreatureFighting>(_second).part, CreatureFighting::Part::Obeys);
	EXPECT_TRUE(minds.obeyEnded.empty());
	_fights.CallOff(_first);
	EXPECT_EQ(minds.abandoned.size(), 3u);
	// The plan of the other creature to obey fails
	ASSERT_EQ(minds.obeyEnded.size(), 1u);
	EXPECT_EQ(minds.obeyEnded.front(), std::pair(_second, _first));
	Locator::creatureMindSystem::reset();
	Locator::creatureLocomotionSystem::reset();
}

// A creature that the hand of another creature holds does not start its duel. It starts it on the first turn after the
// other creature lets go of it.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, ACreatureHeldByAnotherCreatureDoesNotStartItsDuel)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::creatureLocomotionSystem::emplace<TurningLegs>();
	using Stage = CreatureFighting::Stage;
	auto& other = registry.Get<CreatureFighting>(_second);
	other.stage = Stage::Square;
	other.played = true;
	registry.Assign<HeldByCreature>(_second, HeldByCreature {.creature = _first});
	for (int turn = 0; turn < 3; ++turn)
	{
		_fights.ProcessTurn();
		EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Square);
	}
	registry.Remove<HeldByCreature>(_second);
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, Stage::Duel);
	Locator::creatureLocomotionSystem::reset();
}

// Each creature thinks and then does its own fight step, one creature after the other in the order of the list of the
// living. After the first creature in the list, the asked creature thinks on the turn of the request and stands still
// from the next turn. Before the first creature, it thinks also on the next turn, and stands still from the turn after
// that.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheAskedCreatureThinksAsItsPlaceInTheListOfTheLivingLets)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::creatureLocomotionSystem::emplace<TurningLegs>();
	Locator::creatureMindSystem::emplace<ForcedMinds>();
	const auto thinks = [&registry](entt::entity creature) {
		const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
		return !creature_fight::ObeysInFight({
		    .fighting = fighting != nullptr,
		    .startedIt = fighting != nullptr && fighting->madeArena,
		    .onlyAsked = fighting != nullptr && fighting->part == CreatureFighting::Part::Asked,
		});
	};
	// In each turn, each creature in the list thinks, then does its fight step. The first creature asks for the fight
	// in its own turn of the first turn.
	const auto play = [&](bool askedFirst) {
		registry.Remove<CreatureFighting>(_first);
		registry.Remove<CreatureFighting>(_second);
		const std::vector<entt::entity> list = askedFirst ? std::vector {_second, _first} : std::vector {_first, _second};
		std::vector<bool> thought;
		for (int turn = 0; turn < 4; ++turn)
		{
			for (const auto creature : list)
			{
				if (creature == _second)
				{
					thought.push_back(thinks(_second));
				}
				else if (turn == 0)
				{
					EXPECT_EQ(_fights.StartFight(_first, _second),
					          ecs::systems::CreatureFightSystemInterface::StartResult::Started);
				}
				_fights.FightTurn(creature);
			}
		}
		return thought;
	};
	EXPECT_EQ(play(false), (std::vector {true, false, false, false}));
	EXPECT_EQ(play(true), (std::vector {true, true, false, false}));
	Locator::creatureMindSystem::reset();
	Locator::creatureLocomotionSystem::reset();
}

namespace
{
/// Both creatures are after their duel and responded, each with its body free and the life given. The second creature
/// obeys the first.
void AfterTheDuel(Registry& registry, entt::entity first, float firstLife, entt::entity second, float secondLife)
{
	for (const auto& [self, life] : {std::pair(first, firstLife), std::pair(second, secondLife)})
	{
		auto& fighting = registry.Get<CreatureFighting>(self);
		fighting.stage = CreatureFighting::Stage::Respond;
		fighting.part = CreatureFighting::Part::Obeys;
		fighting.played = true;
		fighting.showedResponse = true;
		fighting.ended = true;
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = life;
	}
}

/// The body of the creature plays a clip one time, as a response does
void PlayResponse(Registry& registry, entt::entity creature)
{
	using Body = creature_layers::BodyAction;
	registry.Get<CreatureAnimation>(creature).body = Body {.kind = Body::Kind::Once, .animations = {5, 0, 0}};
}
} // namespace

// The first creature has more life and ends its response first. Its plan ends at the start of the second turn after its
// response. The end of its plan stops the other creature at once, with no fear. The plan of the other creature to obey
// ends as a failure.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheEndOfTheFirstCreatureStopsTheResponseOfTheOtherCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	AfterTheDuel(registry, _first, 1.0f, _second, 0.5f);
	PlayResponse(registry, _second);
	for (uint32_t turn = 0; turn < fight::k_PlanEndTurnsAfterResponse; ++turn)
	{
		_fights.ProcessTurn();
		EXPECT_TRUE(registry.AllOf<CreatureFighting>(_first));
		ASSERT_TRUE(registry.AllOf<CreatureFighting>(_second));
		// On the turn after its response, the first creature frees the mind of the other creature
		EXPECT_EQ(registry.Get<const CreatureFighting>(_second).mindFree, turn >= 1);
	}
	EXPECT_TRUE(minds.obeyEnded.empty());
	_fights.ProcessTurn();
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_second));
	EXPECT_TRUE(minds.desired.empty());
	ASSERT_EQ(minds.obeyEnded.size(), 1u);
	EXPECT_EQ(minds.obeyEnded.front(), std::pair(_second, _first));
	// The first creature ends its own plan: nothing gives it up
	EXPECT_TRUE(minds.abandoned.empty());
	Locator::creatureMindSystem::reset();
}

// The other creature has more life and ends its response first. It waits, and obeys, until the first creature ends its
// response with the fear. The fear of the first creature then stops the other creature.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheOtherCreatureWaitsForTheEndOfTheFirstCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	AfterTheDuel(registry, _first, 0.5f, _second, 1.0f);
	PlayResponse(registry, _first);
	for (int turn = 0; turn < 3; ++turn)
	{
		_fights.ProcessTurn();
	}
	ASSERT_TRUE(registry.AllOf<CreatureFighting>(_second));
	EXPECT_TRUE(registry.Get<const CreatureFighting>(_second).responded);
	EXPECT_TRUE(minds.obeyEnded.empty());
	// While it waits, its life falls to the life of the first creature. It does not look at the lives again, thus it
	// does not fear.
	registry.Get<CreatureNeeds>(_second).needs.life = 0.5f;
	registry.Get<CreatureAnimation>(_first).body = {};
	_fights.ProcessTurn();
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_second));
	ASSERT_EQ(minds.desired.size(), 1u);
	EXPECT_EQ(minds.desired.front(), std::pair(_first, creature_desires::Desire::Fear));
	ASSERT_EQ(minds.obeyEnded.size(), 1u);
	EXPECT_EQ(minds.obeyEnded.front(), std::pair(_second, _first));
	Locator::creatureMindSystem::reset();
}

// The other creature fears the first creature. Its fear stops the first creature, and the plan of the first creature
// fails.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheFearOfTheOtherCreatureStopsThePlanOfTheFirstCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	AfterTheDuel(registry, _first, 1.0f, _second, 0.5f);
	PlayResponse(registry, _first);
	_fights.ProcessTurn();
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_second));
	ASSERT_EQ(minds.desired.size(), 1u);
	EXPECT_EQ(minds.desired.front(), std::pair(_second, creature_desires::Desire::Fear));
	ASSERT_EQ(minds.abandoned.size(), 1u);
	EXPECT_EQ(minds.abandoned.front(), _first);
	ASSERT_EQ(minds.obeyEnded.size(), 1u);
	EXPECT_EQ(minds.obeyEnded.front(), std::pair(_second, _first));
	Locator::creatureMindSystem::reset();
}

// After the duel, a new plan of the other creature stops its fight. It also stops the first creature, and the plan of
// the first creature fails.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, ANewPlanAfterTheDuelStopsBothCreatures)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	AfterTheDuel(registry, _first, 1.0f, _second, 0.5f);
	PlayResponse(registry, _first);
	PlayResponse(registry, _second);
	EXPECT_FALSE(_fights.IsDuelling(_second));
	_fights.CallOff(_second);
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_second));
	ASSERT_EQ(minds.abandoned.size(), 1u);
	EXPECT_EQ(minds.abandoned.front(), _first);
	ASSERT_EQ(minds.obeyEnded.size(), 1u);
	EXPECT_EQ(minds.obeyEnded.front(), std::pair(_second, _first));
	// Neither fears: the new plan comes first
	EXPECT_TRUE(minds.desired.empty());
	Locator::creatureMindSystem::reset();
}

// Only the creature that started a fight is the first creature of the fight
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, OnlyTheCreatureThatStartedTheFightLeadsIt)
{
	EXPECT_TRUE(_fights.LeadsFight(_first));
	EXPECT_FALSE(_fights.LeadsFight(_second));
	Locator::entitiesRegistry::value().Remove<CreatureFighting>(_first);
	EXPECT_FALSE(_fights.LeadsFight(_first));
}

// A call-off in the duel stops both creatures. The first creature then no longer leads the fight while it winds down, so
// the order of a leash tie is taken.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AStopEndsTheLeadOfTheCreatureThatStartedTheFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	_fights.CallOff(_first);
	ASSERT_TRUE(registry.AllOf<CreatureFighting>(_first));
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).stage, CreatureFighting::Stage::Respond);
	EXPECT_FALSE(_fights.LeadsFight(_first));
	EXPECT_FALSE(_fights.LeadsFight(_second));
}

// A call-off on the other creature stops its partner too, so the first creature also no longer leads the fight
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AStopOfThePartnerEndsTheLeadOfTheFirstCreature)
{
	_fights.CallOff(_second);
	EXPECT_FALSE(_fights.LeadsFight(_first));
}

// After a duel that ended with no stop, the first creature still leads the fight
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheFirstCreatureLeadsTheFightAfterTheDuelUntilItStops)
{
	auto& registry = Locator::entitiesRegistry::value();
	AfterTheDuel(registry, _first, 1.0f, _second, 0.5f);
	EXPECT_TRUE(_fights.LeadsFight(_first));
	EXPECT_FALSE(_fights.IsDuelling(_first));
}

// A fighter whose opponent has its order to start its duel has an opponent: its start goes on to its stance, and not to
// the finish
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, TheStartGoesToTheStanceAgainstAnOpponentAboutToStartItsDuel)
{
	Locator::creatureAnimationSystem::emplace<SecondLongAnimations>();
	auto& registry = Locator::entitiesRegistry::value();
	PutInStates(registry, _first, fight::State::Start, _second);
	auto& other = registry.Get<CreatureFighting>(_second);
	other.stage = CreatureFighting::Stage::Square;
	other.played = true;
	fight::Enter(other.fighter, fight::State::Idle);
	// The body of the other creature plays an action, so it cannot start its duel
	PlayResponse(registry, _second);
	auto& fighter = registry.Get<CreatureFighting>(_first).fighter;
	const auto span = fight::TestSpanMs(100.0f, ShownSize(registry.Get<const Creature>(_first)), false);
	fighter.timeMs = static_cast<float>(1000 - span);
	_fights.ProcessTurn();
	EXPECT_EQ(registry.Get<const CreatureFighting>(_second).stage, CreatureFighting::Stage::Square);
	EXPECT_EQ(fighter.state, fight::State::Stance);
	Locator::creatureAnimationSystem::reset();
}

// A fight called off in the duel stops the plan of the partner as a failure. Neither creature then fears the other.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFightCalledOffInTheDuelFailsThePartnerAndNobodyFears)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& minds = static_cast<ForcedMinds&>(Locator::creatureMindSystem::emplace<ForcedMinds>());
	registry.Get<CreatureFighting>(_second).part = CreatureFighting::Part::Obeys;
	registry.AssignOrReplace<CreatureNeeds>(_first).needs.life = 1.0f;
	registry.AssignOrReplace<CreatureNeeds>(_second).needs.life = 0.5f;
	_fights.CallOff(_first);
	ASSERT_EQ(minds.obeyEnded.size(), 1u);
	EXPECT_EQ(minds.obeyEnded.front(), std::pair(_second, _first));
	for (int turn = 0; turn < 3; ++turn)
	{
		_fights.ProcessTurn();
	}
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_first));
	EXPECT_FALSE(registry.AnyOf<CreatureFighting>(_second));
	EXPECT_TRUE(minds.desired.empty());
	EXPECT_TRUE(minds.abandoned.empty());
	Locator::creatureMindSystem::reset();
}

TEST_F(CreatureFightSystemTest, AKnockOutInTheDuelGivesBothCreaturesTheStrengthOfTheFightTwoTimes)
{
	auto& body = static_cast<CountedGains&>(Locator::creaturePhysiologySystem::emplace<CountedGains>());
	// The plan that the win forces makes the winner give up its action, and the two minds stop each other
	_fights.KnockOut(_second);
	EXPECT_EQ(body.gains[_first], 2u);
	EXPECT_EQ(body.gains[_second], 2u);
	Locator::creaturePhysiologySystem::reset();
}

TEST_F(CreatureFightSystemTest, AFightCalledOffInTheDuelGivesBothCreaturesTheStrengthOfTheFightTwoTimes)
{
	auto& body = static_cast<CountedGains&>(Locator::creaturePhysiologySystem::emplace<CountedGains>());
	_fights.CallOff(_first);
	EXPECT_EQ(body.gains[_first], 2u);
	EXPECT_EQ(body.gains[_second], 2u);
	// Out of the duel, a second stop gives nothing
	_fights.CallOff(_second);
	EXPECT_EQ(body.gains[_first], 2u);
	EXPECT_EQ(body.gains[_second], 2u);
	Locator::creaturePhysiologySystem::reset();
}

TEST_F(CreatureFightSystemTest, AFightCalledOffBeforeTheDuelGivesNoStrength)
{
	auto& body = static_cast<CountedGains&>(Locator::creaturePhysiologySystem::emplace<CountedGains>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		registry.Get<CreatureFighting>(self).stage = CreatureFighting::Stage::Square;
	}
	_fights.CallOff(_first);
	EXPECT_TRUE(body.gains.empty());
	Locator::creaturePhysiologySystem::reset();
}

TEST_F(CreatureFightSystemTest, ACallOffInTheDuelMakesTheLifeOfEachCreaturePayAtEachEndOfItsBody)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = 1.0f;
	}
	// The creature that gives up its action ends its fight two times, its partner three times
	_fights.CallOff(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 2));
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 3));
	EXPECT_FALSE(registry.Get<const CreatureFighting>(_first).partnerInMind);
	EXPECT_FALSE(registry.Get<const CreatureFighting>(_second).partnerInMind);
	// A second call-off gives no more payments
	_fights.CallOff(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 2));
	// A stop again while the body is still in the stance pays again. A body that finishes does not pay.
	_fights.EndBodyFightAgain(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 3));
	fight::Enter(registry.Get<CreatureFighting>(_second).fighter, fight::State::Finish);
	_fights.EndBodyFightAgain(_second);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 3));
}

TEST_F(CreatureFightSystemTest, AStopForANewPlanAfterTheFightMakesTheLifePayAgainWhileTheBodyPlaysAMove)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = 1.0f;
	}
	_fights.CallOff(_first);
	ASSERT_TRUE(registry.Get<const CreatureFighting>(_first).ended);
	// A plain call-off of the fight that is over pays nothing
	_fights.CallOff(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 2));
	// A new plan ends the fight of the body again: the life pays one time more, and the other creature does not pay
	_fights.CallOffForPlan(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 3));
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 3));
	// The other creature pays again for its own new plan while its body makes a move
	fight::Enter(registry.Get<CreatureFighting>(_second).fighter, fight::State::Action);
	_fights.CallOffForPlan(_second);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 4));
	// A body that finishes its fight does not pay
	fight::Enter(registry.Get<CreatureFighting>(_first).fighter, fight::State::Finish);
	_fights.CallOffForPlan(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 3));
}

TEST_F(CreatureFightSystemTest, AStopForANewPlanInTheDuelPaysAsACallOffDoes)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = 1.0f;
	}
	_fights.CallOffForPlan(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 2));
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 3));
}

TEST_F(CreatureFightSystemTest, AKnockOutBelowNoFightHealthTakesMoreThanAQuarterAtEachPayment)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = 1.0f;
	}
	registry.Get<CreatureFighting>(_first).fighter.health = 0.9f;
	registry.Get<CreatureFighting>(_second).fighter.health = -0.2f;
	_fights.KnockOut(_second);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, -0.2f, 4));
	EXPECT_LT(registry.Get<const CreatureNeeds>(_second).needs.life, 0.0f);
	EXPECT_FLOAT_EQ(_fights.GetFightHealth(_second), -0.2f);
}

TEST_F(CreatureFightSystemTest, AnIndestructibleLoserKeepsALittleLifeAfterAKnockOut)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = 1.0f;
	}
	registry.Get<CreatureFighting>(_first).fighter.health = 0.9f;
	registry.Get<CreatureFighting>(_second).fighter.health = -0.2f;
	registry.Assign<Indestructible>(_second);
	_fights.KnockOut(_second);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, -0.2f, 4, true));
	EXPECT_GT(registry.Get<const CreatureNeeds>(_second).needs.life, 0.0f);
}

TEST_F(CreatureFightSystemTest, AKnockOutMakesTheLifeOfTheWinnerPayTwoTimesAndTheLifeOfTheLoserFourTimes)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = 1.0f;
	}
	registry.Get<CreatureFighting>(_first).fighter.health = 0.9f;
	registry.Get<CreatureFighting>(_second).fighter.health = 0.9f;
	// The forced plan of the winner stops both creatures, then the faint of the loser stops the loser again
	_fights.KnockOut(_second);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.9f, 2));
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, 0.9f, 4));
}

TEST_F(CreatureFightSystemTest, AWalkBackEndsTheFightOfTheBodyBeforeTheCreatureGivesUpItsAction)
{
	auto& body = static_cast<CountedGains&>(Locator::creaturePhysiologySystem::emplace<CountedGains>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = 1.0f;
	}
	_fights.CallOffForWalkBack(_first);
	EXPECT_EQ(body.gains[_first], 1u);
	EXPECT_EQ(body.gains[_second], 1u);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 3));
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 1));
	Locator::creaturePhysiologySystem::reset();
}

TEST_F(CreatureFightSystemTest, AReleaseSoonAfterAHoldMakesTheLifeOfTheHeldCreaturePayAgain)
{
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
		registry.Get<CreatureFighting>(self).fighter.health = 0.6f;
		registry.AssignOrReplace<CreatureNeeds>(self).needs.life = 1.0f;
	}
	hand.held = _first;
	_fights.FightTurn(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 1));
	// Let go while its body still makes a move of the fight: it ends its fight three times more, its opponent one time
	fight::Enter(registry.Get<CreatureFighting>(_first).fighter, fight::State::Action);
	hand.held.reset();
	_fights.HandLetGo(_first);
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_first).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 4));
	EXPECT_FLOAT_EQ(registry.Get<const CreatureNeeds>(_second).needs.life, fight::LifeAfterPayments(1.0f, 0.6f, 1));
	Locator::creatureHandSystem::reset();
}

TEST_F(CreatureFightSystemTest, AHoldInTheDuelGivesBothCreaturesTheStrengthOfTheFightOneTimeWhenTheHandLetsGo)
{
	auto& body = static_cast<CountedGains&>(Locator::creaturePhysiologySystem::emplace<CountedGains>());
	auto& hand = static_cast<HoldingHand&>(Locator::creatureHandSystem::emplace<HoldingHand>());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto self : {_first, _second})
	{
		fight::Enter(registry.Get<CreatureFighting>(self).fighter, fight::State::Stance);
	}
	// The hold takes the held creature out of its duel with no gain
	hand.held = _first;
	_fights.FightTurn(_first);
	EXPECT_TRUE(body.gains.empty());
	// Let go, its plan gives up its action. Only the duel of the opponent gives the gains.
	hand.held.reset();
	_fights.HandLetGo(_first);
	EXPECT_EQ(body.gains[_first], 1u);
	EXPECT_EQ(body.gains[_second], 1u);
	Locator::creatureHandSystem::reset();
	Locator::creaturePhysiologySystem::reset();
}

// A faint that a script orders keeps the leash on. Any other faint takes it off. Both knock the creature out.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFightSystemTest, AFaintThatAScriptOrdersKeepsTheLeash)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& leash = static_cast<HeldLeash&>(Locator::leashSystem::emplace<HeldLeash>());
	_fights.FaintForScript(_second);
	EXPECT_TRUE(registry.AllOf<CreatureKnockedOut>(_second));
	EXPECT_TRUE(leash.takenOff.empty());
	_fights.ForceFaint(_first);
	EXPECT_TRUE(registry.AllOf<CreatureKnockedOut>(_first));
	EXPECT_EQ(leash.takenOff, (std::vector<entt::entity> {_first}));
	Locator::leashSystem::reset();
}

TEST_F(CreatureFightSystemTest, APressOnTheGroundMakesTheHandWaitAndAWaitingHandMakesNoMove)
{
	auto& hands = static_cast<FakeHands&>(Locator::handSystem::emplace<FakeHands>());
	auto& registry = Locator::entitiesRegistry::value();
	// A press on the ground sends the step and starts the wait of the hand
	EXPECT_TRUE(PressGround({-15.0f, 10.0f}, fight::Button::Action));
	_fights.Release(0, 0);
	EXPECT_EQ(hands.sends, 1);
	EXPECT_EQ(QueueOf(_first).Size(), 1u);
	const auto sparkles = registry.Get<const CreatureFighting>(_first).marks.Size();
	// Until the turn does the step, the fight takes the press but makes no move, no sparkle and no new wait
	hands.waits = true;
	EXPECT_TRUE(PressGround({-15.0f, -10.0f}, fight::Button::Action));
	EXPECT_FALSE(_fights.HoldsBodyPress());
	EXPECT_FALSE(_fights.IsPressed());
	_fights.Release(0, 0);
	EXPECT_EQ(hands.sends, 1);
	EXPECT_EQ(QueueOf(_first).Size(), 1u);
	EXPECT_EQ(registry.Get<const CreatureFighting>(_first).marks.Size(), sparkles);
	// After the turn that did the step, a press sends again
	hands.waits = false;
	EXPECT_TRUE(PressGround({-15.0f, -10.0f}, fight::Button::Action));
	EXPECT_EQ(hands.sends, 2);
	EXPECT_EQ(QueueOf(_first).Size(), 2u);
}

// The other creature of a fight waits for its next order when it turned to face the first creature, when it is ready in
// the arena, and when it turned in the arena but has no order to start its duel yet
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureFightLook, TheOtherCreatureWaitsForAnOrderOnlyWhenItDidTheLastOrder)
{
	using Stage = CreatureFighting::Stage;
	using Phase = creature_turn_to_face::Turn::Phase;
	CreatureFighting fighting {.stage = Stage::FaceOff};
	fighting.faceTurn.phase = Phase::Start;
	EXPECT_FALSE(WaitsForNextOrder(fighting));
	fighting.faceTurn.phase = Phase::Done;
	EXPECT_TRUE(WaitsForNextOrder(fighting));
	fighting.stage = Stage::Taunt;
	EXPECT_FALSE(WaitsForNextOrder(fighting));
	fighting.stage = Stage::Approach;
	EXPECT_FALSE(WaitsForNextOrder(fighting));
	fighting.stage = Stage::Ready;
	EXPECT_TRUE(WaitsForNextOrder(fighting));
	fighting.stage = Stage::Square;
	EXPECT_TRUE(WaitsForNextOrder(fighting));
	fighting.played = true;
	EXPECT_FALSE(WaitsForNextOrder(fighting));
	fighting.faceTurn.phase = Phase::Idle;
	fighting.played = false;
	EXPECT_FALSE(WaitsForNextOrder(fighting));
}

// Before the duel, the head of the first creature follows the looks of the steps of the fight one turn late. The look
// comes from the fight as the last turn left it.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureFightLook, TheFirstCreatureLooksAsTheStepOfTheLastTurnTellsIt)
{
	using Stage = CreatureFighting::Stage;
	using Phase = creature_turn_to_face::Turn::Phase;
	CreatureFighting fighting {.stage = Stage::FaceOff, .madeArena = true};
	// Before its first fight turn it keeps the look that it had
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::Before);
	// The request looks at the other creature, the order to stand still has no look, the order to turn looks ahead
	const std::vector<LeadInLook> faceOff {LeadInLook::Opponent, LeadInLook::About, LeadInLook::Ahead, LeadInLook::Opponent,
	                                       LeadInLook::Opponent};
	for (uint32_t turn = 1; turn <= faceOff.size(); ++turn)
	{
		fighting.faceOffTurns = turn;
		fighting.faceTurn.phase = turn >= fight::k_FaceOffOwnTurn ? Phase::Start : Phase::Idle;
		EXPECT_EQ(FirstCreatureLook(fighting), faceOff[turn - 1]) << "face-off turn " << turn;
	}
	// The turn after its own turn ended, the order to taunt has no look
	fighting.faceTurn.phase = Phase::Done;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::About);
	fighting.stage = Stage::Taunt;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::About);
	fighting.stage = Stage::Point;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::About);
	// The order to walk to the arena and the walk look at the other creature, and the wait looks at it
	fighting.stage = Stage::Approach;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::Partner);
	fighting.stage = Stage::Ready;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::Opponent);
	// The wait keeps its look for one more turn, then the order to turn looks ahead and its own turn at the other
	fighting.stage = Stage::Square;
	fighting.faceOffTurns = 0;
	fighting.faceTurn = {};
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::Opponent);
	fighting.faceOffTurns = fight::k_SquareOrderTurn;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::Ahead);
	fighting.faceOffTurns = fight::k_SquareOwnTurn;
	fighting.faceTurn.phase = Phase::Start;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::Opponent);
	// The order to start the duel has no look
	fighting.faceTurn.phase = Phase::Done;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::About);
	fighting.played = true;
	EXPECT_EQ(FirstCreatureLook(fighting), LeadInLook::About);
}

namespace
{
/// A creature out cold that is about to rest to get better, with its faint already on its body
CreatureKnockedOut& AboutToRest(Registry& registry, entt::entity creature)
{
	registry.Remove<CreatureFighting>(creature);
	auto& needs = registry.Assign<CreatureNeeds>(creature);
	needs.needs.life = 0.3f;
	needs.needs.exhaustion = 0.5f;
	return registry.AssignOrReplace<CreatureKnockedOut>(creature, CreatureKnockedOut {
	                                                                  .stage = CreatureKnockedOut::Stage::Resting,
	                                                                  .fainting = false,
	                                                                  .rest = true,
	                                                              });
}
} // namespace

TEST_F(CreatureFightSystemTest, TheRestAfterAFaintStartsTheSleepPoseAndHealsFromTheNextTurn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	game.numbers = {1};
	AboutToRest(registry, _first);
	registry.Remove<CreatureFighting>(_second);
	// Taken home, the body stands. On the first turn of the rest it starts the sleep pose on the drawn side.
	_fights.FightTurn(_first);
	const auto& body = registry.Get<const CreatureAnimation>(_first).body;
	EXPECT_EQ(body.kind, creature_layers::BodyAction::Kind::Sequence);
	EXPECT_EQ(body.animations, (std::array<size_t, 3> {28, 29, 30}));
	EXPECT_TRUE(body.mirrored);
	EXPECT_EQ(game.ranges, (std::vector<uint32_t> {2}));
	// The body heals at the start of a turn, before this step. Thus the rest that the start of the pose gives heals
	// from the next turn, not in the turn that the pose starts.
	EXPECT_TRUE(registry.Get<const CreatureKnockedOut>(_first).restPosed);
	EXPECT_EQ(registry.Get<const CreatureNeeds>(_first).rest, CreatureNeeds::Rest::Resting);
	// Each later turn of the pose rests and draws no side
	_fights.FightTurn(_first);
	EXPECT_EQ(registry.Get<const CreatureNeeds>(_first).rest, CreatureNeeds::Rest::Resting);
	EXPECT_EQ(game.ranges.size(), 1u);
	Locator::gameRandom::reset();
}

TEST_F(CreatureFightSystemTest, TheRestAfterAFaintTriesThePoseEachTurnWhileTheBodyIsBusyAndCountsThoseTurns)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& game = static_cast<ListedGameRandom&>(Locator::gameRandom::emplace<ListedGameRandom>());
	AboutToRest(registry, _first);
	registry.Remove<CreatureFighting>(_second);
	// It lies where it fell, as a creature that nobody takes home: the body is busy
	auto& animation = registry.Get<CreatureAnimation>(_first);
	animation.body = {.kind = creature_layers::BodyAction::Kind::Sequence,
	                  .phase = creature_layers::BodyAction::Phase::Loop,
	                  .animations = {105, 105, 107},
	                  .holdLoop = true};
	for (int turn = 0; turn < 5; ++turn)
	{
		_fights.FightTurn(_first);
	}
	// Each try draws a side, and the pose does not start. It does not heal.
	EXPECT_EQ(game.ranges, (std::vector<uint32_t>(5, 2)));
	EXPECT_FALSE(registry.Get<const CreatureKnockedOut>(_first).restPosed);
	EXPECT_EQ(registry.Get<const CreatureNeeds>(_first).rest, CreatureNeeds::Rest::Unconscious);
	EXPECT_EQ(animation.body.animations[0], 105u);
	// Once the body is free, the pose starts, and the turns of the tries count as turns of the rest
	animation.body = {};
	_fights.FightTurn(_first);
	EXPECT_TRUE(registry.Get<const CreatureKnockedOut>(_first).restPosed);
	auto& needs = registry.Get<CreatureNeeds>(_first);
	needs.needs.life = 1.0f;
	needs.needs.exhaustion = 0.0f;
	needs.fainted = true;
	// Better, it gets up on the 31st turn of the rest, from the end of the sleep pose
	for (int turn = 6; turn < 30; ++turn)
	{
		_fights.FightTurn(_first);
	}
	EXPECT_EQ(registry.Get<const CreatureKnockedOut>(_first).stage, CreatureKnockedOut::Stage::Resting);
	_fights.FightTurn(_first);
	EXPECT_EQ(registry.Get<const CreatureKnockedOut>(_first).stage, CreatureKnockedOut::Stage::GettingUp);
	EXPECT_TRUE(animation.body.endWanted);
	EXPECT_EQ(needs.rest, CreatureNeeds::Rest::Awake);
	// After a rest, the knock-out ends only when the body is free. The mark of the faint stays until a turn with no plan.
	_fights.FightTurn(_first);
	EXPECT_TRUE(registry.AllOf<CreatureKnockedOut>(_first));
	EXPECT_EQ(registry.Get<const CreatureKnockedOut>(_first).steps, CreatureKnockedOut::Steps::Running);
	animation.body = {};
	_fights.FightTurn(_first);
	EXPECT_FALSE(registry.AllOf<CreatureKnockedOut>(_first));
	EXPECT_TRUE(needs.fainted);
	Locator::gameRandom::reset();
}

// A creature that nobody takes home and that does not rest lies until the turn after the wait for the spells, as the
// game ends the empty last stage of the wait then. On that turn the knock-out ends, and the body starts to get up. The
// mind is free while the body gets up, and the mark of the faint stays until a turn with no plan.
TEST_F(CreatureFightSystemTest, AKnockOutWithNoRestEndsOnTheTurnAfterTheWaitForTheSpellsAndTheBodyGetsUpThen)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	auto& needs = registry.Assign<CreatureNeeds>(_first);
	needs.needs.life = 1.0f;
	needs.fainted = true;
	registry.AssignOrReplace<CreatureKnockedOut>(_first, CreatureKnockedOut {
	                                                         .stage = CreatureKnockedOut::Stage::WaitingForSpells,
	                                                         .fainting = false,
	                                                         .rest = false,
	                                                     });
	auto& animation = registry.Get<CreatureAnimation>(_first);
	animation.body = {.kind = creature_layers::BodyAction::Kind::Sequence,
	                  .phase = creature_layers::BodyAction::Phase::Loop,
	                  .animations = {105, 105, 107},
	                  .holdLoop = true};
	// No spell is on it: the wait ends, and the body still lies
	_fights.FightTurn(_first);
	ASSERT_TRUE(registry.AllOf<CreatureKnockedOut>(_first));
	EXPECT_EQ(registry.Get<const CreatureKnockedOut>(_first).stage, CreatureKnockedOut::Stage::WaitingForSpells);
	EXPECT_EQ(registry.Get<const CreatureKnockedOut>(_first).steps, CreatureKnockedOut::Steps::EndNextTurn);
	EXPECT_FALSE(animation.body.endWanted);
	EXPECT_EQ(registry.Get<const CreatureNeeds>(_first).rest, CreatureNeeds::Rest::Unconscious);
	// On the next turn the knock-out ends, and the body starts to get up
	_fights.FightTurn(_first);
	EXPECT_FALSE(registry.AllOf<CreatureKnockedOut>(_first));
	EXPECT_TRUE(creature_layers::IsPlaying(animation.body));
	EXPECT_TRUE(animation.body.endWanted);
	EXPECT_EQ(registry.Get<const CreatureNeeds>(_first).rest, CreatureNeeds::Rest::Awake);
	EXPECT_TRUE(registry.Get<const CreatureNeeds>(_first).fainted);
}

TEST_F(CreatureFightSystemTest, TheMoveHomeAfterAFaintCutsTheFaintAndTheBodyStands)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureFighting>(_first);
	registry.Remove<CreatureFighting>(_second);
	auto& animation = registry.Get<CreatureAnimation>(_first);
	animation.body = {.kind = creature_layers::BodyAction::Kind::Sequence,
	                  .phase = creature_layers::BodyAction::Phase::Loop,
	                  .animations = {105, 105, 107},
	                  .holdLoop = true};
	registry.Assign<CreatureKnockedOut>(_first, CreatureKnockedOut {.stage = CreatureKnockedOut::Stage::FadingOut,
	                                                                .fainting = false,
	                                                                .home = glm::vec3(40.0f, 0.0f, 40.0f),
	                                                                .fizzTurns = 1});
	_fights.FightTurn(_first);
	EXPECT_EQ(registry.Get<const CreatureKnockedOut>(_first).stage, CreatureKnockedOut::Stage::FadingIn);
	EXPECT_EQ(registry.Get<const Transform>(_first).position.x, 40.0f);
	EXPECT_EQ(animation.body.kind, creature_layers::BodyAction::Kind::Stand);
}

namespace
{
/// The middle of the screen shows land only to an eye at one place
class CentrePicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> LandUnderPixel(glm::vec3 eye, glm::vec3 /*nearPoint*/, Sea /*sea*/) const override
	{
		if (landSeenFrom.has_value() && glm::distance(eye, *landSeenFrom) < 1.0f)
		{
			return glm::vec3(0.0f);
		}
		return std::nullopt;
	}

	std::optional<glm::vec3> landSeenFrom;
};

constexpr glm::vec3 k_EyeInArena {0.0f, 30.0f, 20.0f};
/// A place of the temple's camera, far from the camera of the world
constexpr glm::vec3 k_TempleEye {0.0f, 10.0f, 600.0f};

/// The camera of the world looks at the arena from within it
void LookFromWithinTheArena(Camera& camera)
{
	camera.SetOrigin(k_EyeInArena);
	camera.SetFocus({0.0f, 0.0f, 0.0f});
}

/// Frames that last 0.6 s each of real time. The game time does not move, as in the temple or while the game pauses.
void LookFor(ecs::systems::CreatureFightSystem& fights, int frames,
             std::chrono::milliseconds frameTime = std::chrono::milliseconds(600))
{
	auto& time = static_cast<FakeTime&>(Locator::time::emplace<FakeTime>());
	time.realTime = frameTime;
	for (int frame = 0; frame < frames; ++frame)
	{
		fights.NoteLook();
		fights.Update(std::chrono::duration<float, std::milli>(0.0f));
	}
}
} // namespace

TEST_F(CreatureFightSystemTest, TheLookAtAFightCountsOnlyWhileTheMiddleOfTheScreenShowsLand)
{
	auto* picking = new CentrePicking();
	Locator::pickingSystem::reset<MockPickingSystem>(picking);
	_fights.Reset();
	LookFromWithinTheArena(Locator::camera::value());
	// The middle of the screen shows the sky: the look does not count
	LookFor(_fights, 4);
	EXPECT_FALSE(_fights.IsCameraOnFight());
	// The middle of the screen shows land: after more than a second the fight view starts
	picking->landSeenFrom = k_EyeInArena;
	LookFor(_fights, 1);
	EXPECT_FALSE(_fights.IsCameraOnFight());
	LookFor(_fights, 1);
	EXPECT_TRUE(_fights.IsCameraOnFight());
	Locator::pickingSystem::reset();
}

TEST_F(CreatureFightSystemTest, InTheTempleTheLineToTheMiddleOfTheScreenStartsFromTheViewOfTheTemple)
{
	auto* picking = new CentrePicking();
	Locator::pickingSystem::reset<MockPickingSystem>(picking);
	_fights.Reset();
	LookFromWithinTheArena(Locator::camera::value());
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	auto behind = GoIntoTemple(temple);
	LookFromWithinTheArena(*behind);
	Locator::camera::value().SetOrigin(k_TempleEye).SetFocus(k_TempleEye + glm::vec3(0.0f, 0.0f, 10.0f));
	// Only the camera of the world behind the temple sees land in the middle of the screen: the look does not count
	picking->landSeenFrom = k_EyeInArena;
	LookFor(_fights, 4);
	EXPECT_FALSE(_fights.IsCameraOnFight());
	// The view of the temple sees land: the look counts and the fight view starts behind the temple
	picking->landSeenFrom = k_TempleEye;
	LookFor(_fights, 2);
	EXPECT_TRUE(_fights.IsCameraOnFight());
	EXPECT_NE(dynamic_cast<const FightCameraModel*>(&behind->GetModel()), nullptr);
	ComeOutOfTemple(temple, behind);
	Locator::temple::reset();
	Locator::pickingSystem::reset();
}

TEST_F(CreatureFightSystemTest, TheLookUsesTheCameraFromBeforeItMovedInTheFrame)
{
	auto* picking = new CentrePicking();
	Locator::pickingSystem::reset<MockPickingSystem>(picking);
	picking->landSeenFrom = k_EyeInArena;
	_fights.Reset();
	auto& camera = Locator::camera::value();
	auto& time = static_cast<FakeTime&>(Locator::time::emplace<FakeTime>());
	time.realTime = std::chrono::milliseconds(600);
	// Before it moves in each frame the camera is far from the arena, and then it moves into the arena
	for (int frame = 0; frame < 4; ++frame)
	{
		camera.SetOrigin({0.0f, 30.0f, 2000.0f}).SetFocus({0.0f, 0.0f, 1900.0f});
		_fights.NoteLook();
		LookFromWithinTheArena(camera);
		_fights.Update(std::chrono::duration<float, std::milli>(600.0f));
	}
	EXPECT_FALSE(_fights.IsCameraOnFight());
	// An update with no look kept before it does not count
	for (int frame = 0; frame < 4; ++frame)
	{
		_fights.Update(std::chrono::duration<float, std::milli>(600.0f));
	}
	EXPECT_FALSE(_fights.IsCameraOnFight());
}

TEST_F(CreatureFightSystemTest, TheLookCountsWholeMillisecondsOfRealTimeAndStartsTheViewAfterMoreThan1000)
{
	auto* picking = new CentrePicking();
	Locator::pickingSystem::reset<MockPickingSystem>(picking);
	picking->landSeenFrom = k_EyeInArena;
	_fights.Reset();
	LookFromWithinTheArena(Locator::camera::value());
	// Two frames of 500 ms make 1000 ms: this is not more than 1000, so the view does not start
	LookFor(_fights, 2, std::chrono::milliseconds(500));
	EXPECT_FALSE(_fights.IsCameraOnFight());
	// One more millisecond starts the view, also with no game time
	LookFor(_fights, 1, std::chrono::milliseconds(1));
	EXPECT_TRUE(_fights.IsCameraOnFight());
	Locator::pickingSystem::reset();
}
