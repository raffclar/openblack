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

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureReactionRules.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct MindFileData;
}
namespace openblack::creature_mind_tables
{
struct Tables;
}

namespace openblack::ecs::systems
{

/// The creatures' minds: once a game turn their desires grow and fade, and each decides what its body does while it
/// has nothing better to do, and what it looks at (see components::CreatureMindState). Creatures can also be told to
/// do things, which the debug tools use.
class CreatureMindSystemInterface
{
public:
	virtual ~CreatureMindSystemInterface() = default;

	virtual void ProcessTurn() = 0;
	/// Once a game turn after ProcessTurn: each creature plans two of its desires and changes what it does for a plan
	/// pressing enough
	virtual void PlanTurn() = 0;
	/// Once a game turn after PlanTurn: what the leash taught, and copying the player, step on
	virtual void LearnTurn() = 0;

	/// Takes up a mind file's learning, desires and stage of growing up at the next turn
	virtual void LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind) = 0;
	/// The creature's mind as a mind file of the current version, over the file it was loaded from, if it has a mind
	[[nodiscard]] virtual std::optional<creaturemind::MindFileData> SaveMind(entt::entity creature) const = 0;
	/// Swaps the minds of two creatures at once, as a script does: everything the mind file keeps (what each has learnt
	/// and how it feels, its physique and tattoos) moves to the other body, while each keeps its name, size and alignment
	/// for now; the creature the first belonged to now owns the second and the other way round, and both carry out the
	/// swap, during which each body takes on the size and alignment the other had. Returns whether it could.
	virtual bool SwapMinds(entt::entity /*from*/, entt::entity /*to*/) { return false; }
	/// Forgets everything learnt: the desires start again as the species' do, with no opinions or examples
	virtual void ClearLearning(entt::entity creature) = 0;
	/// Creatures that can see the point watch an ordinary skill practised (by its row in the game's table), or a miracle
	/// cast
	virtual void SeeSkill(const glm::vec3& point, size_t skill) = 0;
	virtual void SeeMiracle(const glm::vec3& point, size_t miracle) = 0;
	/// A player did one of the deeds creatures copy (by its row in the game's table) at a point, maybe to something.
	/// Only that player's own creature, the one it leads on the leash, may copy it; with no player given, any creature
	/// may (for the debug tools).
	virtual void PlayerDid(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	                       std::optional<PlayerNames> player) = 0;
	/// A player did a deed with a miracle (by its magic type). A creature that has not seen the miracle often enough
	/// counts a sighting of it and can then stop before it copies the deed.
	virtual void PlayerCastDeed(size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	                            std::optional<PlayerNames> player, MagicType /*magic*/)
	{
		PlayerDid(deed, point, object, player);
	}
	/// The game's tables the minds use, once the game's data is loaded
	[[nodiscard]] virtual const creature_mind_tables::Tables* GetTables() = 0;
	/// What a creature believes about a thing now, with the attributes that its decision trees test. None for a thing
	/// that is of no kind that a creature keeps beliefs about.
	[[nodiscard]] virtual std::optional<creature_tree::Belief> BeliefAbout(entt::entity /*creature*/,
	                                                                       entt::entity /*thing*/) const
	{
		return std::nullopt;
	}

	/// Whether a cell of the land, by its column and row, is clear for a creature to go to: land it can walk on with
	/// nothing in the way (nor anything living, when asked)
	[[nodiscard]] virtual bool IsCellClearFor(entt::entity /*creature*/, int32_t /*x*/, int32_t /*z*/,
	                                          bool /*livingBlocks*/) const
	{
		return false;
	}
	/// The middle of the nearest clear area so wide about a point that the creature may go to, none without one
	[[nodiscard]] virtual std::optional<glm::vec2> FindClearAreaFor(entt::entity /*creature*/, glm::vec2 /*point*/,
	                                                                float /*width*/, bool /*livingBlocks*/) const
	{
		return std::nullopt;
	}

	/// Plays an action once, unless the creature's body already plays one
	virtual bool PlayAction(entt::entity creature, size_t animation) = 0;
	/// Plays a gesture on top of the body, unless one already plays
	virtual bool PlayGesture(entt::entity creature, size_t animation) = 0;
	/// Draws a gesture in the air with the hand, by the gesture's number, unless it draws one already
	virtual bool DrawGesture(entt::entity /*creature*/, uint32_t /*gesture*/) { return false; }
	/// Pulls a face for a few seconds, as told to by hand
	virtual void PullFace(entt::entity creature, size_t animation) = 0;
	/// Pulls the face a feeling or what it is doing calls for, as the creature's mind would; returns the face pulled
	virtual std::optional<creature_face::Request> ShowFeeling(entt::entity creature, creature_face::Cue cue) = 0;
	/// Sits down for a while, unless the body plays an action
	virtual bool SitDown(entt::entity creature) = 0;
	/// Gets up from sitting
	virtual void StandUp(entt::entity creature) = 0;
	/// The player's hand let go of the creature having stroked or slapped it, from -1 (slapped) to 1 (stroked): it
	/// stops what it was doing if slapped, warms or cools to the player, and shows its pleasure or sorrow next. Feedback
	/// too slight to count only has it look at the player.
	virtual void ReceiveFeedback(entt::entity creature, float feedback) = 0;
	/// The hand let go of the creature, and sends the feedback to the mind. The feedback arrives after the first turn
	/// in which the creature is free.
	virtual void SendReleaseFeedback(entt::entity creature, float feedback) { ReceiveFeedback(creature, feedback); }
	/// The player's hand took hold of the creature: its mind stands still while held, it turns to face the player, and
	/// once its body is free it shows what it wants
	virtual void HandTookHold(entt::entity /*creature*/) {}
	/// Feedback that only moves how the creature feels about its player (as being hit by what its player threw does):
	/// it takes the player to want compassion or anger, and warms or cools to the player, from -1 to 1
	virtual void UpdateAttitudeFromFeedback(entt::entity /*creature*/, float /*feedback*/) {}
	/// The average of the feedback the player has given all creatures: each new feedback counts for more than the
	/// average before it. It is one for all creatures.
	[[nodiscard]] virtual float GetAverageFeedback() const { return 0.0f; }
	/// The running average of the player's feedback starts again from 0, as a custom music track starts
	virtual void ClearAverageFeedback() {}
	/// A custom music track starts: the creature reacts to the mood that it felt before about the track (0 for a new
	/// track, above 0 for a track it likes, below 0 for a track it does not like)
	virtual void ReactToMusic(entt::entity /*creature*/, float /*mood*/) {}
	/// The player told the creature what to do on the leash: the player has dealt with it now
	virtual void NoteLeashOrder(entt::entity /*creature*/) {}
	/// Every source of a type in the creature's desires goes up or down by an amount
	virtual void ChangeDesireSource(entt::entity /*creature*/, uint32_t /*type*/, float /*amount*/) {}
	/// Plays an action at once, as stroking and slapping make it, unless the body is less than a share of the way through
	/// what it plays; a face can be pulled with it, which is pulled even when the action can't play. The action plays to
	/// its end and leaves the mind's plan as it was. Returns whether it played.
	virtual bool ForceAction(entt::entity creature, size_t animation, bool mirrored, std::optional<creature_face::Request> face,
	                         float interruptsAfter) = 0;

	/// Sees to a need now, in place of whatever it was doing: sleeps until rested, eats something (the nearest food when
	/// none is given), goes and drinks at the nearest water, has a poo, is sick, or faints. Returns whether it could.
	virtual bool Sleep(entt::entity creature) = 0;
	virtual bool Eat(entt::entity creature, std::optional<entt::entity> food) = 0;
	virtual bool Drink(entt::entity creature) = 0;
	virtual bool Poo(entt::entity creature) = 0;
	virtual bool Puke(entt::entity creature) = 0;
	virtual bool Faint(entt::entity creature) = 0;
	/// Wakes it, or ends whatever it is sitting, lying or squatting through
	virtual void Wake(entt::entity creature) = 0;

	/// A fight the creature was in ended, won or lost: fighting satisfies the desire it is for, and its body pays for it
	virtual void FoughtFight(entt::entity creature, bool won) = 0;
	/// A fight started between the two creatures. The fight has the minds of both until it ends. When the first creature
	/// did not plan the fight itself, it is made to fight. The second creature stops what it does and obeys the first.
	virtual void FightStarted(entt::entity /*creature*/, entt::entity /*opponent*/, bool /*planned*/) {}
	/// What the creature does stops as a failure. The desire becomes fully dominant, and the creature plans to satisfy it
	/// through the thing. Returns whether the creature has a plan for that desire.
	virtual bool ForceDesireOn(entt::entity /*creature*/, creature_desires::Desire /*desire*/, entt::entity /*thing*/)
	{
		return false;
	}
	/// Something stops what the creature is doing, which it gives up as a failure, as a spell freezing it does
	virtual void AbandonAction(entt::entity /*creature*/) {}
	/// The creature is made to catch a thing flying at it: what it was doing fails, and it plays with the thing by catching
	/// it, wanting to play less for a while
	virtual void ForceCatch(entt::entity /*creature*/, entt::entity /*object*/) {}
	/// A plan forced on a creature, as a reaction forces one: the desire it serves, the thing the desire is served
	/// through (none for the desire itself), the action by its name in the game's table, the thing acted on, and the
	/// thing used, if any
	struct ForcedActivity
	{
		creature_desires::Desire desire {creature_desires::Desire::Fear};
		std::optional<entt::entity> activityObject;
		std::string_view action;
		entt::entity actionObject {entt::null};
		std::optional<entt::entity> instrument;
		/// The plan counts as an order: when the creature forgets the plan, the plan can teach approval
		bool ordered {false};
		/// The creature comes across the thing that it acts on before it makes the plan
		bool comeAcross {false};
		/// When this has a value, the plan is not forced and has this priority. The creature then changes to a plan that
		/// is more than twice as pressing, it looks at its emergencies, and the plan counts for its desire at its end.
		std::optional<float> unforcedPriority;
		/// The plan is marked as forced until the creature stops it. Then the creature takes up no reaction and no
		/// emergency, and the plan does not count for its desire at its end. The plans of reactions, of copying the
		/// player and of watching a villager are not marked, but they also have the highest priority.
		bool forced {true};
	};
	/// What the creature was doing stops as a failure and it carries out the plan instead: whether it could. A plan
	/// carried out is remembered for the player's feedback (unless nothing says why it wants it), and the desires the
	/// plan's desire opposes are held back.
	virtual bool ForceActivity(entt::entity /*creature*/, const ForcedActivity& /*plan*/) { return false; }
	/// The player's hand gives the creature a thing: it drops what it was doing, turns to the camera at a point on the
	/// ground looking curious, and reaches to take the thing from the hand, with its left hand when asked; then its
	/// desires choose what to do with it. Whether it took the giving.
	virtual bool TakeFromHand(entt::entity /*creature*/, entt::entity /*object*/, bool /*leftHand*/, glm::vec2 /*camera*/)
	{
		return false;
	}
	/// Whether the creature is still about taking a thing from the hand: turning to the player or reaching for it
	[[nodiscard]] virtual bool IsTakingFromHand(entt::entity /*creature*/) const { return false; }
	/// Whether the creature lies dead, knocked out or doing one of the actions of lying dead for now or for good
	[[nodiscard]] virtual bool IsLyingDead(entt::entity /*creature*/) { return false; }
	/// Whether another creature seems friendly to the creature: the creature is not angry with it or afraid of it, and the
	/// other creature is not angry with the creature
	[[nodiscard]] virtual bool SeemsFriendly(entt::entity /*creature*/, entt::entity /*other*/) { return false; }
	/// A fire reaches the creature: it goes to put out a fire on something it cares for enough, once it has seen the
	/// water miracle often enough to cast it, and runs from the burning thing otherwise
	virtual void ReactToFire(entt::entity /*creature*/, entt::entity /*burning*/) {}
	/// A nasty miracle struck near the creature: it is frightened by it, and runs from where it struck, or goes to look
	/// at it when it isn't afraid or is on the learning leash; it learns the miracle when it may
	virtual void ReactToNastyMagic(entt::entity /*creature*/, const glm::vec3& /*point*/, std::optional<size_t> /*learn*/) {}
	/// After it reacts to a nasty miracle, a creature on the leash that knows the miracle casts it. It casts at the first
	/// thing near the hand, when its reaction ends.
	virtual void CastOnLeashAfterReaction(entt::entity /*creature*/, size_t /*miracle*/) {}
	/// A nice miracle of its own player's was cast near the creature: it goes to look at it, and learns it
	virtual void ReactToNiceMagic(entt::entity /*creature*/, const glm::vec3& /*point*/, std::optional<size_t> /*learn*/) {}
	/// The creature starts to react to a thing. It keeps the plan that it does now, before the reaction changes the plan.
	virtual void StartReacting(entt::entity /*creature*/) {}
	/// The creature takes up a reaction to a thing: it comes across the thing, drops what it was doing for the plan the
	/// reaction makes (done to the thing or to itself), learns the skill it watches, and moves a source of its desires,
	/// as the reaction's kind says
	virtual void TakeUpReaction(entt::entity /*creature*/, entt::entity /*thing*/, const creature_reaction::Setup& /*setup*/) {}
	/// The creature weighs looking another creature over: it comes across it, and when it never has looked it over, or
	/// not for more than the wait in turns, it is due a look, and notes the turn as when it last looked. Whether it is due
	[[nodiscard]] virtual bool NoticeCreatureToInspect(entt::entity /*creature*/, entt::entity /*other*/, uint32_t /*turn*/,
	                                                   uint32_t /*waitTurns*/)
	{
		return false;
	}
	/// The creature tries a miracle at an object, as its casting pose's loop begins or in a fight: in a fight it pays
	/// stamina first; a try short of the sightings it needs fizzles; a miracle it can't cast frustrates it. Whether it
	/// was cast.
	virtual bool TryMiracle(entt::entity /*creature*/, MagicType /*type*/, entt::entity /*target*/) { return false; }
	/// For the debug tools: the creature knows a miracle (by its magic type) as if it had learnt it, and is told to cast
	/// one at a thing, going about it as it would by itself
	virtual void KnowMiracle(entt::entity /*creature*/, size_t /*miracle*/) {}
	/// A script makes the creature know an ordinary skill or a miracle (by its row in the game's tables), or forget it.
	/// Taught a miracle, it has seen it as often as its species needs to.
	virtual void SetKnowsAction(entt::entity /*creature*/, CreatureActionLearningType /*type*/, uint32_t /*action*/,
	                            bool /*knows*/)
	{
	}
	virtual bool TellCast(entt::entity /*creature*/, MagicType /*type*/, entt::entity /*target*/) { return false; }

	/// A plan the creature is made to carry out, as an order given with the leash makes it: an action of the game's table
	/// for a desire, on a thing or at a point, with its own agenda unless one is given
	struct ForcedPlan
	{
		creature_desires::Desire desire {creature_desires::Desire::ObeyPlayer};
		std::string_view action;
		std::optional<entt::entity> object;
		std::optional<glm::vec2> point;
		std::optional<std::vector<creature_mind::Step>> agenda;
		/// The thing used, if any
		std::optional<entt::entity> instrument;
		/// What the desire is about, if anything: what the thing acted on belongs to, say
		std::optional<entt::entity> activityObject;
		/// What the creature does, for a plan that gives its agenda; told when not given
		std::optional<creature_mind::Activity> activity;
		/// The plan counts as an order: by an order on the leash, by a script, by a thing that the player gives, by what
		/// the creature does after a stroke or a slap
		bool ordered {false};
		/// The plan is marked as forced until the creature stops it: it then takes up no reaction and no emergency, and
		/// the plan does not count for its desire at its end. A script's order and the meal after a stroke are not
		/// marked.
		bool forced {true};
		/// The priority of the plan against the plans that the creature finds for itself. With none, nothing that it wants
		/// is more pressing.
		std::optional<float> priority;
	};
	/// The creature has eaten a magic mushroom: once it is done with what it is doing it becomes high, unless it is
	/// about to already
	virtual void AteMagicMushroom(entt::entity /*creature*/) {}
	/// The creature gives up what it was doing for the plan, which nothing it wants replaces until it is done. Returns
	/// whether it could be carried out.
	virtual bool ForcePlan(entt::entity /*creature*/, const ForcedPlan& /*plan*/) { return false; }
	/// The creature's desires choose something to do to a thing, as its planner would with that thing the only one
	/// about, and it is made to do it. The desire it acts on, if its desires found anything.
	virtual std::optional<creature_desires::Desire> ForcePlanOn(entt::entity /*creature*/, entt::entity /*object*/)
	{
		return std::nullopt;
	}
	/// A script has the creature carry out an agenda, as it plays what a script gave it, turns it to face a point or
	/// sends it somewhere: the script takes control of it, and it stops what it was doing as a failure and obeys. Under
	/// a script's control it chooses nothing for itself, standing about once the agenda is over, until the script lets
	/// it go. Returns whether it could.
	virtual bool CarryOutForScript(entt::entity /*creature*/, std::vector<creature_mind::Step> /*agenda*/) { return false; }
	/// A script has the creature keep its head turned to a thing or a point until it lets it go (nothing lets it go); it
	/// lets go by itself once the thing is gone or the script lets the creature go
	virtual void SetScriptFocus(entt::entity /*creature*/, std::optional<creature_mind::Gaze> /*focus*/) {}
	/// A script forces an action of the game's table on the creature, by its row, done to a thing and with another, as
	/// it stages a scene: the script takes control of it, and it carries the action out as an order from its player.
	/// Returns whether the action could be carried out.
	virtual bool ScriptDoAction(entt::entity /*creature*/, uint32_t /*action*/, entt::entity /*target*/,
	                            std::optional<entt::entity> /*with*/)
	{
		return false;
	}
	/// Whether the creature has played what it was doing, as scripts ask: its agenda is over or it is only idle
	[[nodiscard]] virtual bool HasPlayed(entt::entity /*creature*/) const { return true; }
	/// A script asked and the creature has played: it stops the remaining steps of its agenda
	virtual void ClearAfterPlayed(entt::entity /*creature*/) {}

	/// How strongly the player last rewarded or punished the creature, and setting it back to 0
	[[nodiscard]] virtual float GetInteractionMagnitude(entt::entity /*creature*/) const { return 0.0f; }
	virtual void ClearInteractionMagnitude(entt::entity /*creature*/) {}
	/// How many times the creature has carried out an action (by the game's numbering) to its end
	[[nodiscard]] virtual uint32_t GetActionCount(entt::entity /*creature*/, uint32_t /*action*/) const { return 0; }
	/// A script makes a desire the creature's only one for some seconds, or takes it away again
	virtual void SetOnlyDesire(entt::entity /*creature*/, creature_desires::Desire /*desire*/, float /*seconds*/) {}
	virtual void ClearOnlyDesire(entt::entity /*creature*/) {}
	/// A desire is made the creature's dominant one for some seconds (as a leash does every turn it is worn): wanted as
	/// much as it can be, the others as little as any can be and held down, its bodily needs too only when asked
	virtual void MakeDesireDominant(entt::entity /*creature*/, creature_desires::Desire /*desire*/, float /*seconds*/,
	                                bool /*needsToo*/)
	{
	}
	/// The desire made dominant ends at once and every desire held down is let go (as picking the learning leash does)
	virtual void EndDominantDesire(entt::entity /*creature*/) {}
	/// The desire the creature wants most now, if its mind has desires yet
	[[nodiscard]] virtual std::optional<creature_desires::Desire> DominantDesire(entt::entity /*creature*/) const
	{
		return std::nullopt;
	}
	/// A script changes the creature's desires: how much it wants them, whether they are active and how much it can
	/// want them at most. A creature whose mind hasn't thought yet has them set up first, the ones its stage of growing
	/// up brings already active, so that what the script sets stands.
	virtual void ChangeDesires(entt::entity /*creature*/, const std::function<void(creature_desires::Desires&)>& /*change*/) {}
	/// A script sets how pressing what the creature carries out now is, which a plan of its own must beat to replace it
	virtual void SetAgendaPriority(entt::entity /*creature*/, float /*priority*/) {}
	/// The creature is told to point out the lesson highlight about it, if there is one; whether one was found
	virtual bool PointOutHighlight(entt::entity /*creature*/) { return false; }
	/// The story moves the creature to a stage of growing up: its desires become the ones its stages so far bring, none
	/// stays held back, and it drops the plan it was carrying out
	virtual void MoveToStage(entt::entity /*creature*/, uint32_t /*stage*/) {}
	/// A script makes the creature stop what it does at once, as a failure. The action that its body plays ends at two
	/// times the usual speed, and the creature stops where it is.
	virtual void ForceFinish(entt::entity /*creature*/) {}
};

} // namespace openblack::ecs::systems
