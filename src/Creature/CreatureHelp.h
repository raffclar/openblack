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

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

/// The good advisor's messages about the player's creature: what it learnt, its troubles, what it wants, why it failed,
/// and short prompts on how to look after it. Each kind of message has its own text range, its own way of being said,
/// and its own rules for waiting its turn. A message the creature can't be told about now may wait on a stack and be
/// told later; one told is not told again for a minute.
namespace openblack::creature_help
{

/// The kinds of message, in the order the game keeps them: a waiting message of an earlier kind is told first
enum class Kind : uint8_t
{
	LessonIncreaseSource,
	LessonDecreaseSource,
	LessonIncreaseDesire,
	LessonDecreaseDesire,
	LessonIncreaseOpinion,
	LessonDecreaseOpinion,
	LessonNormalAction,
	LessonMagicAction,
	NearlyLearntNormalAction,
	NearlyLearntMagicAction,
	CantLearnNormalActionYet,
	CantLearnMagicActionYet,
	/// The stage messages: each starts the lesson script of a stage of growing up (the first does nothing)
	StageInitial,
	StageLearnToEat,
	StagePunishment,
	StageLeashPullAndPickUp,
	StageLeashAttachToHouse,
	StageMeetGuide,
	StageFriendsWithGuide,
	StageGuideExplainsHistory,
	StageLearnSpell,
	StageLearnToImpressTown,
	StageLearnToFight,
	StageLearnToHelpTown,
	StageLeashGoodAndEvil,
	StageFullyMature,
	ReceivedSpell,
	FinishedReceivingSpell,
	Tutorial,
	CurrentSource,
	PhysicalProblem,
	MentalProblem,
	PromptHasntLearntNormalAction,
	TownAttitudeToCreature,
	CannotLearnActionInThisStage,
	ShowStageCriteria,
	SomethingInterestingOffScreen,
	MiscellaneousStacked,
	MiscellaneousUnstacked,
	MessedUpAction,
	FailureExplanation,
	CurrentDesire,
	_Count
};
constexpr size_t k_KindCount = static_cast<size_t>(Kind::_Count);

/// How a message is said
enum class Telling : uint8_t
{
	Nothing,
	/// The good advisor comes out, looks at the creature as it says the text, and goes home
	PointAtCreature,
	/// The text alone
	JustText,
	/// The text is made but nothing says it (as the game does for the mental problems)
	Unsaid,
	/// The stage's lesson script starts, and the creature is in a development script from then on
	StageScript,
};

struct KindInfo
{
	/// The lowest creature help level that tells it
	uint8_t level;
	Telling telling;
	/// The first text of its range; the message's parameter is added to it
	uint16_t firstText;
	/// Told only while a camera of the creature's player looks at the creature
	bool needsCreatureInView;
	/// Waits on the stack when it can't be told now; otherwise it is lost
	bool stacks;
	/// Only one of the kind waits: a newer one takes the waiting one's place
	bool oneWaiting;
};

/// The kinds, in Kind's order
[[nodiscard]] const KindInfo& Info(Kind kind);
[[nodiscard]] constexpr bool IsStageMessage(Kind kind)
{
	return kind >= Kind::StageInitial && kind <= Kind::StageFullyMature;
}
/// The lesson script a stage message starts, none for the first. The last starts its script only on the first land.
[[nodiscard]] std::optional<std::string_view> StageScript(Kind kind, uint32_t land);

/// The text told when the creature can't hold what the player offers it, in place of the miscellaneous range's eighth
/// text
constexpr uint32_t k_TooBigToHandleText = 4322;
/// The miscellaneous message of a thing too big for the creature to handle
constexpr uint32_t k_TooBigToHandle = 7;
/// Why it failed: it couldn't get where it was sent, and the leash pulled it away from what it was doing
constexpr uint32_t k_Inaccessible = 3;
constexpr uint32_t k_PulledAway = 7;

/// The text of a message, none when there is nothing to say: the ordinary skills' messages say nothing of skill 0. The
/// miracles' learnt and nearly learnt texts are the miracles' own, given by the caller.
[[nodiscard]] std::optional<uint32_t> Text(Kind kind, uint32_t parameter, std::optional<uint32_t> miracleText = std::nullopt);

/// A message as it is raised
struct Message
{
	Kind kind {Kind::CurrentDesire};
	uint32_t parameter {0};
	/// What it is about, and where, which a waiting message keeps
	entt::entity thing {entt::null};
	glm::vec3 position {0.0f};
	/// Told even while the hand is on the creature, the creature is busy dying or asleep, or it was told lately
	bool forced {false};
};

/// What the game is doing as a message is raised
struct Situation
{
	/// The creature help option, which the scripts can also turn off
	bool helpOn {true};
	bool insideTemple {false};
	/// The creature is the local player's
	bool playersCreature {true};
	/// Its stage of growing up
	uint32_t stage {13};
	/// Fainted, dying or asleep
	bool outOrAsleep {false};
	/// A script holds the dialogue
	bool dialogueTaken {false};
	/// The player's hand is on the creature
	bool handOnCreature {false};
	/// Another message about a creature was raised this game turn
	bool raisedThisTurn {false};
	bool scriptControlled {false};
	bool inDevelopmentScript {false};
	/// A camera of its player looks at it
	bool inView {true};
};

enum class Outcome : uint8_t
{
	Tell,
	Stack,
	Drop,
};

/// Only a creature this grown up gives any message but the stage messages
constexpr uint32_t k_HelpStage = 13;

/// What becomes of a message, given whether one like it was told lately
[[nodiscard]] Outcome Decide(const Message& message, const Situation& situation, bool toldLately);

/// A message the creature remembers, by its kind and parameter
struct Entry
{
	uint32_t parameter {0};
	entt::entity thing {entt::null};
	glm::vec3 position {0.0f};
	/// The game turn it was last told, 0 for never
	uint32_t toldTurn {0};
	bool waiting {false};
};

/// The creature's prompts on how to look after it
constexpr size_t k_TutorialCount = 12;

/// What a creature remembers of its messages
struct Memory
{
	std::array<std::vector<Entry>, k_KindCount> entries {};
	/// When it was last told of a problem with its body, and of a prompt
	uint32_t physicalProblemTurn {0};
	uint32_t tutorialTurn {0};
	/// The prompts the player has shown they know, and when each was last told
	std::array<bool, k_TutorialCount> tutorialDone {};
	std::array<uint32_t, k_TutorialCount> tutorialToldTurn {};
	/// How steadily the hand has been over it, 0 to 1
	float handOver {0.0f};
};

/// Whole seconds between two game turns
[[nodiscard]] uint32_t WholeSeconds(uint32_t now, uint32_t then, uint32_t turnsPerSecond);

/// Whether the message of this kind and parameter was told less than a minute ago (five seconds for what it wants)
[[nodiscard]] bool ToldLately(const Memory& memory, Kind kind, uint32_t parameter, uint32_t turn, uint32_t turnsPerSecond);
/// The message is told now
void MarkTold(Memory& memory, Kind kind, uint32_t parameter, uint32_t turn);
/// The message waits its turn: unless one like it was told in the last minute, it takes the place of the kind's waiting
/// message of the same parameter (or its only one), else it waits after the others
void Stack(Memory& memory, const Message& message, uint32_t turn, uint32_t turnsPerSecond);

/// The waiting message to raise again this turn: the kinds in order, then their messages in order
struct Waiting
{
	Kind kind;
	size_t index;
};
[[nodiscard]] std::optional<Waiting> FirstWaiting(const Memory& memory);
/// The waiting message was raised again: whatever became of it, it is done waiting and counts as told now
void Settle(Memory& memory, Waiting waiting, uint32_t turn);

/// How the creature's body is doing, 0 to 1, and how its species' help reads it
struct Body
{
	float energy {1.0f};
	float life {1.0f};
	float exhaustion {0.0f};
	float dehydration {0.0f};
};
struct BodyThresholds
{
	/// Seconds between messages about its body
	float seconds {120.0f};
	float lowEnergy {0.5f};
	float veryLowEnergy {0.2f};
	float lowLife {0.5f};
	float veryLowLife {0.2f};
	float tired {0.5f};
	float veryTired {0.8f};
	float thirsty {0.5f};
	float veryThirsty {0.8f};
};
/// The problems with its body the help tells of, by their parameter
enum class BodyProblem : uint8_t
{
	LowEnergy,
	VeryLowEnergy,
	Hurt,
	BadlyHurt,
	Tired,
	VeryTired,
	Thirsty,
	VeryThirsty,
};
/// The body problem to tell of now, if its time has come: the first that holds of low energy, very low energy, hurt,
/// badly hurt, tired, very tired, thirsty and very thirsty, tested in that order (so a body low enough for the "very"
/// message always gets the plain one first, as the game has it)
[[nodiscard]] std::optional<BodyProblem> BodyProblemNow(const Body& body, const BodyThresholds& thresholds,
                                                        uint32_t secondsSinceLast);

/// Fainting, the creature says why first
[[nodiscard]] std::optional<BodyProblem> FaintingProblem(const Body& body);

/// The hand-over reading after a creature turn, with the hand over it or not
[[nodiscard]] float NextHandOver(float handOver, bool over);
/// The hand has been over it steadily enough to be told what it wants
constexpr float k_HandOverToTell = 0.25f;

/// The prompts' conditions this turn: whether the creature is old enough, what it most wants, and what it carries
struct TutorialConditions
{
	uint32_t stage {13};
	bool wantsToBuildHome {false};
	bool wantsWater {false};
	bool wantsToPoo {false};
	bool carrying {false};
};
[[nodiscard]] std::array<bool, k_TutorialCount> TutorialApplies(const TutorialConditions& conditions);
/// The prompt to tell, if its time has come: of those not done whose conditions hold, the one told longest ago (the
/// first of equals), when that was more than the seconds given ago; a never-told first prompt is always due
[[nodiscard]] std::optional<size_t> TutorialNow(const Memory& memory, std::span<const bool, k_TutorialCount> applies,
                                                uint32_t turn, uint32_t turnsPerSecond, float seconds);
/// The prompts a leash order shows the player knows
enum class Prompt : uint8_t
{
	Leash,
	Run,
	HandOver,
	Unleash,
	MoreRocks,
	PickUp,
	DropRocksAtHome,
	DropAtFeet,
	StrokeToEat,
	SlapOrStroke,
	Drink,
	Poo,
};

/// Whether a camera at the origin, looking at the focus, has the point in view: closer than 400 and within the help's
/// cone about where it looks
[[nodiscard]] bool LooksAt(glm::vec3 cameraOrigin, glm::vec3 cameraFocus, glm::vec3 point);

/// The most wanted of the creature's active desires, by its number: the first of those of greatest value above 0, else
/// desire 0
[[nodiscard]] size_t DominantDesire(std::span<const float> values, std::span<const bool> active);

} // namespace openblack::creature_help
