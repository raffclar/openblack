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
#include <cstdint>

#include <optional>
#include <random>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "Commands/PlayerCommands.h"
#include "Common/GameRandom.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/PosedModel.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack
{
class FightCameraModel;
} // namespace openblack

namespace openblack::ecs::systems
{

class CreatureFightSystem final: public CreatureFightSystemInterface
{
public:
	void ProcessTurn() override;
	void FightTurn(entt::entity creature) override;
	void BloodTurn(entt::entity creature) override;
	void MovesTurn(entt::entity creature) override;
	void StaminaTurn(entt::entity creature) override;
	void Reset() override;
	void PrepareLand() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;

	StartResult StartFight(entt::entity creature, entt::entity opponent) override;
	StartResult StartPlannedFight(entt::entity creature, entt::entity opponent, bool agreed) override;
	void Withdraw(entt::entity creature) override;
	void AbortFight(entt::entity creature) override;
	void CallOff(entt::entity creature) override;
	void HandTookHold(entt::entity creature) override;
	void HandLetGo(entt::entity creature) override;
	[[nodiscard]] bool IsSettingUpFight(entt::entity creature) const override;
	[[nodiscard]] bool IsFighting(entt::entity creature) const override;
	[[nodiscard]] bool IsDuelling(entt::entity creature) const override;
	void TakeForScript(entt::entity creature) override;
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity creature) const override;

	bool QueueMove(entt::entity creature, const creature_fight::Move& move, bool replace) override;
	void ReleaseCharge(entt::entity creature, float heldMs) override;
	void SetAutoFighting(entt::entity creature, bool autoFight) override;
	[[nodiscard]] bool IsAutoFighting(entt::entity creature) const override;
	[[nodiscard]] creature_fight::FightAction CurrentFightAction(entt::entity creature) const override;
	[[nodiscard]] uint32_t QueuedBlows(entt::entity creature) const override;
	[[nodiscard]] float GetFightHealth(entt::entity creature) const override;
	[[nodiscard]] float ForwardExtent(entt::entity creature) const override;
	void SetFightHealth(entt::entity creature, float health) override;
	[[nodiscard]] std::optional<FoundArena> FindOrMakeArena(const glm::vec3& point, entt::entity creature, entt::entity other,
	                                                        float within) override;

	[[nodiscard]] std::optional<entt::entity> PlayersFighter() const override;
	[[nodiscard]] bool DirectsFight() const override;
	void FlewTo(const glm::vec3& place) override;
	bool Press(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, creature_fight::Button button, uint32_t milliseconds,
	           uint32_t turn) override;
	void Release(uint32_t milliseconds, uint32_t turn) override;
	void DoGroundPress(PlayerNames player, entt::entity creature, const glm::vec3& point, bool replace) override;
	void DoMove(PlayerNames player, entt::entity target, const creature_fight::Move& move, bool replace) override;
	void DoCharge(PlayerNames player, float heldMs) override;
	[[nodiscard]] bool IsPressed() const override { return _pressed.has_value(); }
	[[nodiscard]] std::optional<creature_fight::Tip> HandTip(std::optional<entt::entity> under) const override;
	bool GestureSpecialMove() override;
	bool GestureSpell(MagicType type) override;
	void DrawMarks(float realMs, const glm::vec3& eye) override;
	std::optional<glm::vec3> TakeHandSparkle() override { return std::exchange(_handSparkle, std::nullopt); }

	[[nodiscard]] bool IsBlocking(entt::entity creature) const override;
	void Recoil(entt::entity creature) override;
	void KnockOut(entt::entity creature) override;
	void ForceFaint(entt::entity creature) override;
	void KillPermanently(entt::entity creature) override;
	void Resurrect(entt::entity creature) override;
	[[nodiscard]] bool IsKnockedOut(entt::entity creature) const override;

	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override;

	void SetCameraWatches(bool enabled) override { _cameraWatches = enabled; }
	[[nodiscard]] bool GetCameraWatches() const override { return _cameraWatches; }
	[[nodiscard]] bool IsCameraOnFight() const final { return _view.has_value(); }
	[[nodiscard]] bool IsCameraOnFightInArena() const final;
	void SetFightExit(bool allowed) override { _fightExit = allowed; }
	[[nodiscard]] bool GetFightExit() const override { return _fightExit; }
	[[nodiscard]] bool FightMusicHeard(const glm::vec3& camera) const override;
	void LoseFightHealth(entt::entity creature, float damage) override;

private:
	/// The start of a fight. When ask is false, the other creature agreed before and is not asked again.
	StartResult Start(entt::entity creature, entt::entity opponent, bool ask);
	/// The turn's parts: fights started by the leash, the stages before and after the
	/// duel, the duel's moves, and the creatures knocked out
	void StartFightsFromMinds();
	void StartFightFromMind(entt::entity creature);
	void ProcessStages();
	void StageTurn(entt::entity creature);
	/// A fighter's animation, its move in the duel, its blow and its pose for the turn
	void StepMoves(entt::entity creature);
	/// A fighter's turn in the duel: it faces its opponent, keeps in the arena, moves apart from its opponent and
	/// chooses or makes its next move
	void DuelTurn(entt::entity creature);
	/// The action that moved on in a fighter's turn, and where its animation was before it moved on
	struct ActionStep
	{
		size_t animation;
		int32_t fromMs;
	};
	/// A fighter's animation moves on by one game turn. An action (or a reel in a block) whose animation reaches its end
	/// in the turn ends first: the fighter goes back to its stance (or its block), and a move that the player put in the
	/// queue starts at once. An action first
	/// moves the creature as far as its animation's root moves in the turn. Other states go to their next state when the
	/// animation ends. Returns the action that moved on, if one did.
	std::optional<ActionStep> StepAnimation(entt::entity creature);
	void ProcessKnockedOut();
	void KnockedOutTurn(entt::entity creature);
	/// Makes the move at the front of a fighter's queue, if it can
	void CheckQueue(entt::entity creature);
	/// A blow at a band: struck, or a step taken towards where it would land
	/// The spell a fighter casts as its cast's start ends: an attacking one at the opponent, a defending one on itself
	void CastFightSpell(entt::entity creature, MagicType type, entt::entity opponent);
	void AttemptBlow(entt::entity creature, creature_fight::Band band, float speed);
	/// A fighter's blow in its turn: tested along its swing in the turn, then the two bodies move apart when they overlap.
	/// Only a blow that moved on in the fighter's step of the turn is tested.
	void BlowTurn(entt::entity creature, std::optional<ActionStep> step);
	/// A blow tested from where its animation was at the last turn: it marks its swing and, where it meets the
	/// opponent's body, lands there
	void TestBlow(entt::entity creature, int32_t fromMs);
	/// A blow that met the opponent's body at a point, with its vector there
	void LandBlow(entt::entity creature, const glm::vec3& point, const glm::vec3& vector);
	/// The wound a blow leaves where the line along its vector through the point first meets the skin, and the drip of
	/// a wound that bleeds
	void WoundSkin(entt::entity victim, const glm::vec3& point, const glm::vec3& vector, uint8_t kind, uint8_t column);
	/// Each game turn the drips of blood run down the skins
	void ProcessDrips();
	void DripTurn(entt::entity creature);
	/// One creature beat the other, by a blow or otherwise: the loser faints, and the winner is made to have a poo on
	/// the loser or to show off
	void Win(entt::entity winner, entt::entity loser, bool byBlow);
	/// The opponent of a creature has no life left: the creature wins and leaves the duel. The loser does not faint,
	/// and nobody forces a plan on the winner.
	void WinByOpponentsLife(entt::entity winner, entt::entity loser);
	/// The fight ends for a creature: its life pays for it and it learns from it
	/// The creature's fight is over: won or lost, or called off with neither (when its mind isn't told it fought)
	/// Its life pays for the fight only when paysLife is true.
	void EndFightFor(entt::entity creature, std::optional<bool> won, bool paysLife = true);
	/// The fight of the creature stops with no response, as when its partner tells it to stop. In the steps before the
	/// duel it leaves the fight at once. Else it finishes its move and then leaves the fight.
	void StopFight(entt::entity creature, std::optional<bool> won, std::string_view why);
	void BeginDuel(entt::entity creature);
	void MeasureBlows(entt::entity creature);
	/// Faints and lies out cold, to be taken home later
	void Faint(entt::entity creature);
	/// The faint starts on the body: its side, the faint itself, and the reaction of those near it
	void StartFaint(entt::entity creature);
	/// The camera's fight view: started by looking at an arena with a fight on, it follows the fight and lingers a little
	/// after it, unless the player zooms out of it
	void UpdateView(float seconds);
	/// How names the way into the view for the transcript: "look" or "double_click". The camera's place and focus for
	/// the test of distance are the ones given, or else the camera's now.
	void TryStartView(entt::entity first, entt::entity second, const creature_fight::Arena& arena, std::string_view how,
	                  std::optional<std::pair<glm::vec3, glm::vec3>> from = std::nullopt);
	/// Whether the player has dragged the land so far from the arena of the fight that the view ends
	[[nodiscard]] bool DraggedAway(const FightCameraModel& model) const;
	/// Why names the end of the view for the transcript
	void EndView(std::string_view why);
	/// Leaves the fight for good, its mind taking over again
	void Leave(entt::entity creature);
	/// Where the line from the player's fighter to its opponent meets the opponent's skin, as gestures need
	[[nodiscard]] std::optional<ecs::posed_model::SkinHit> SeesOpponent(entt::entity creature) const;
	/// A move called by a gesture, queued with its sparkle when the fighter sees its opponent
	bool GestureMove(const creature_fight::Move& move);
	/// A fight command of the player goes to the next turn. With no queue of commands (the tests), the world does it at
	/// once.
	void Send(const player_commands::Action& action);

	/// The transcript of the fights, when the inspector keeps one. A record for each state that a fighter is in at the end
	/// of a frame or a game turn, when it is new.
	void NoteActions();
	void NoteAction(entt::entity creature);
	void NoteStage(entt::entity creature, components::CreatureFighting::Stage from, components::CreatureFighting::Stage to);
	/// The fight of the creature is over, for a reason
	void NoteFightOver(entt::entity creature, std::string_view reason);

	/// For the transcript: why the next fight starts, and who adds the next moves to a queue
	std::string_view _startReason {"request"};
	std::string_view _queueSource {"script"};
	/// For the transcript: the state and the animation that each fighter was last in, and the record that causes the next
	/// change of each fighter
	struct Seen
	{
		creature_fight::State state;
		size_t animation;
	};
	std::unordered_map<entt::entity, Seen> _seen;
	std::unordered_map<entt::entity, uint64_t> _actionCause;

	bool _cameraWatches {true};
	/// The player's creature a press is charging a blow for, and when it was pressed
	struct Pressed
	{
		entt::entity creature;
		uint32_t milliseconds;
		uint32_t turn;
	};
	std::optional<Pressed> _pressed;
	/// The fight the camera watches: the creature that made the arena and the other, and once the fight is over how
	/// long the view lingers
	struct Watched
	{
		entt::entity first;
		entt::entity second;
		std::optional<float> lingerSeconds;
	};
	std::optional<Watched> _view;
	/// How long the camera has looked at an arena from within it
	float _lookSeconds {0.0f};
	/// Where the camera is now, as the screen shows it, and the land in the middle of the screen. When the middle of the
	/// screen shows no land, the land it last showed, or the point that the camera looks at before it showed any land.
	[[nodiscard]] std::pair<glm::vec3, glm::vec3> ViewNow();
	std::optional<glm::vec3> _screenCentre;
	/// The place on the land that a double click sent the world camera to, until the next update of the view, with the
	/// camera's place and the land in the middle of the screen before the flight
	struct Flight
	{
		glm::vec2 place;
		glm::vec3 eye;
		glm::vec3 focus;
	};
	std::optional<Flight> _flightPlace;
	/// Whether the player may leave the fight view, and it ends by itself (scripts may forbid it)
	bool _fightExit {true};
	/// A press on a body is held: the sparkle added last charges
	bool _markHeld {false};
	/// The point on the opponent's body that the hand goes to, from the last drawing
	std::optional<glm::vec3> _handSparkle;
	/// The sparkles flicker at most 60 times a second
	creature_fight::marks::Pace _marksPace;
	/// The game's random numbers: the fight draws on the synced seed, and the blood and the marks on the local seed
	GameRandomSource _random;
};

} // namespace openblack::ecs::systems
