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

#include <chrono>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Common/InputSteps.h"
#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"
#include "Magic/CastInput.h"
#include "Magic/HandMotion.h"
#include "Magic/HeldCastRules.h"
#include "Magic/PrayerRules.h"
#include "Magic/SpellRules.h"
#include "Particles/ParticleSpellLink.h"

namespace openblack::magic
{
struct EffectSource;
} // namespace openblack::magic

namespace openblack::player_commands
{
struct SeedGesture;
struct HandCast;
} // namespace openblack::player_commands

namespace openblack::ecs::systems
{
class ObjectDrawListSystemInterface;

/// The running miracles, the seeds the player holds and the miracle dispensers.
///
/// A miracle is an entity with a components::Spell: cast at a point or on an object by a caster who pays for it, it runs
/// its particle effect, acts on the events the effect sends (a fireball's flight, a bolt striking, food landing) and
/// pays its upkeep each turn until its time or its prayer power runs out; it goes once its effect, and whatever its kind
/// made, has gone. A dispenser floats a one-shot bubble above itself; tapping the bubble with the hand (the left button)
/// puts its miracle in the hand, fully charged and ready. A seed summoned from the player's worship is charged from
/// their prayer power and is ready after a moment. The action button (the right) casts the seed as it says: held
/// (lightning, water, food, wood: applied every turn while the button is down), thrown on letting go (the fireball and
/// the flocks; the storms and shields at the circle drawn) or placed on pressing (heal, forest, teleport, the beam
/// explosion, the creature spells on a creature). The hand must be in the player's influence. The gestures drawn reach
/// it through the gesture events: a circle, a power-up, a scribble or a shake, which drops the seed.
class MagicSystemInterface: public SavedStateInterface
{
public:
	/// What a frame tells the miracles about the player's hand
	struct HandFrame
	{
		/// Where the hand is
		glm::vec3 handPosition {0.0f};
		/// The land or object under the cursor, none over the sky
		std::optional<glm::vec3> point;
		/// The line of sight through the cursor
		glm::vec3 rayOrigin {0.0f};
		glm::vec3 rayDirection {0.0f, 0.0f, 1.0f};
		/// Which way the camera looks
		glm::vec3 cameraForward {0.0f, 0.0f, 1.0f};
		/// Whether the hand is over the world, not over a window or in the temple
		bool overWorld {true};
	};

	/// What the debug window shows of a running miracle
	struct SpellInfo
	{
		entt::entity entity;
		MagicType magicType;
		PlayerNames player;
		float age;
		float duration;
		float chants;
		float initialChants;
		float strength;
		float upkeep;
		bool closing;
		bool fromHand;
		uint32_t effect;
		glm::vec3 position;
	};

	/// What the debug window shows of a dispenser
	struct DispenserInfo
	{
		entt::entity entity;
		MagicType magicType;
		glm::vec3 position;
		bool hasOrb;
		uint32_t tick;
		uint32_t period;
		bool active;
	};

	/// What became of the last thing the hand tried, for the debug window
	enum class HandResult : uint8_t
	{
		None,
		TookMiracle,
		Readied,
		Cast,
		CastHeld,
		NotReady,
		CantCastThere,
		Released,
		Discarded,
		/// A storm or shield let go of with no circle drawn
		NoCircle,
		PoweredUp,
		/// The miracle went into the player's own dispenser as a one-shot globe
		Stored,
	};

	/// What the debug window shows of the hand's casting
	struct HandCastState
	{
		magic::CastInput::State state {magic::CastInput::State::Idle};
		bool holding {false};
		bool ready {false};
		/// Seconds until the seed is ready
		float readyIn {0.0f};
		magic::SeedOrigin origin {magic::SeedOrigin::Bubble};
		int powerUp {-1};
		float chantStore {0.0f};
		float storedChants {-1.0f};
		/// The hand's smoothed movement, and the speed a fireball would be thrown at
		glm::vec3 velocity {0.0f};
		float throwSpeed {0.0f};
		/// The circle remembered for a storm or shield, and the seconds it has left
		std::optional<glm::vec3> circleCentre;
		float circleRadius {0.0f};
		float circleSecondsLeft {0.0f};
		magic::PourPose pour;
		/// Whether the hand's point is somewhere the held seed may be cast
		bool pointValid {false};
	};

	virtual ~MagicSystemInterface() = default;

	// Casting

	/// A miracle cast by a player at a point, as the hand, a script or a debug tool casts it: the entity, or none when
	/// it could not start. The process info is what its effect starts with: the hand's place, the camera, the throw.
	virtual entt::entity CastAtPoint(MagicType type, PlayerNames player, glm::vec3 point, const magic::SpellCastData& cast,
	                                 const particles::ProcessInfo& info) = 0;
	/// A miracle cast on an object: at its feet, its effect given the object to act on
	virtual entt::entity CastOnObject(MagicType type, PlayerNames player, entt::entity target, const magic::SpellCastData& cast,
	                                  const particles::ProcessInfo& info) = 0;
	/// The miracle stops: its effect dies away and it goes once that has gone
	virtual void CloseDown(entt::entity spell) = 0;
	/// A creature casts a miracle at an object, as its casting pose's loop begins: as big as the object, its effect
	/// flowing from the creature's hands, paid for with the creature's body, cast at the object's feet or on the object
	/// as the miracle's seed is. Whether it could: not when the creature is too tired to pay for it, nor when the
	/// miracle can't be cast at the object. Past those it counts as cast even if no miracle comes of it.
	virtual bool CastByCreature(entt::entity /*creature*/, MagicType /*type*/, entt::entity /*target*/) { return false; }
	/// A creature casts a miracle at a place, as its casting pose's loop begins. The miracle has a fixed size, or for a
	/// fire seed a size from the height of the creature. The other rules are the same as for a cast at an object.
	/// Return true if the creature cast it.
	virtual bool CastByCreatureAt(entt::entity /*creature*/, MagicType /*type*/, glm::vec3 /*place*/) { return false; }
	/// The creature lets go of the miracle it cast: one that lasts only while it is held stops
	virtual void ReleaseCreatureCast(entt::entity /*creature*/) {}
	/// The beams from the hands of a creature that casts from above stop. The miracle that it casts continues. The hold
	/// of the creature by a hand does this.
	virtual void EndCreatureCastBeams(entt::entity /*creature*/) {}
	/// An effect at a point that no miracle is behind, such as a lightning strike a script calls down: everything it
	/// reaches takes it, as from the player
	virtual void ApplyEffectAt(glm::vec3 /*point*/, const magic::EffectValues& /*values*/, PlayerNames /*player*/) {}
	/// An effect on one object that no miracle is behind, such as the crush of a blow, from a player: what its defence
	/// lets through, as any effect does. Whether the object has life to take it.
	virtual bool ApplyEffectToObject(entt::entity /*object*/, const magic::EffectValues& /*values*/, PlayerNames /*player*/)
	{
		return false;
	}
	/// The same from a source that says what applied the effect, and whether any player is behind it
	virtual bool ApplyEffectToObject(entt::entity /*object*/, const magic::EffectValues& /*values*/,
	                                 const magic::EffectSource& /*source*/)
	{
		return false;
	}
	/// Whether a player may cast a magic type at a point
	[[nodiscard]] virtual bool CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) = 0;
	/// Something the miracle made acts for it, as its particles would: whether the miracle acted
	virtual bool SpellEvent(entt::entity spell, const particles::SpellEventInfo& event) = 0;
	/// The miracle pays prayer power, or is given it back for a negative cost
	virtual void PayForSpell(entt::entity spell, float cost) = 0;

	// Dispensers and one-shot miracles

	/// A dispenser of a magic type standing at a point, turned about the vertical; it floats its first bubble once its
	/// period has passed
	virtual entt::entity CreateDispenser(glm::vec3 position, MagicType type, float yAngleRadians) = 0;
	/// A dispenser a script or a land's script makes: a building of its kind standing on the ground at a point, turned
	/// and scaled, holding no miracle and turned off until a script gives it one and turns it on, with its building's
	/// period. It belongs to the town given. With no town, it belongs to the first town that the first player gained.
	/// None for a building that does not exist.
	virtual entt::entity CreateScriptDispenser(glm::vec3 /*position*/, AbodeInfo /*building*/, float /*yAngleRadians*/,
	                                           float /*scale*/, entt::entity /*town*/)
	{
		return entt::null;
	}
	/// A dispenser that a town's plan or a scaffold puts up: as a script's dispenser, with a part of it built. A
	/// dispenser with less than all of it built goes up with a site that its town opens later.
	virtual entt::entity CreateTownDispenser(glm::vec3 /*position*/, AbodeInfo /*building*/, float /*yAngleRadians*/,
	                                         float /*scale*/, entt::entity /*town*/, float /*built*/)
	{
		return entt::null;
	}
	/// A script gives a dispenser a miracle and the turns between its bubbles; with no turns it is turned off
	virtual void SetDispenserMagic(entt::entity /*dispenser*/, MagicType /*type*/, uint32_t /*turns*/) {}
	/// For the testbed: a dispenser that has counted its period, floating its bubble now if it has none
	virtual void ChargeDispenser(entt::entity /*dispenser*/) {}
	/// The time from a dispenser's bubble being taken to the next, in seconds
	virtual void SetDispenserPeriod(entt::entity dispenser, float seconds) = 0;
	/// A script turns a dispenser on or off. Turned on, it floats a new bubble at once, even over one it already has,
	/// and starts counting again.
	virtual void SetDispenserActive(entt::entity /*dispenser*/, bool /*active*/) {}
	/// A script sets the turns from a dispenser's bubble being taken to the next; none leaves it as it was
	virtual void SetDispenserTurns(entt::entity /*dispenser*/, uint32_t /*turns*/) {}
	/// A one-shot bubble of a seed at a point, floating there
	virtual entt::entity CreateOneOffSeed(glm::vec3 position, SpellSeedType seed, int powerUp, float multiplier) = 0;
	/// A one-shot bubble of whatever seed casts a magic type, at the power-up level that casts it
	virtual entt::entity CreateOneOffSeedFor(glm::vec3 position, MagicType type) = 0;
	/// Takes a dispenser away with its bubble and its swirl, or a bubble with the seed spinning in it; false for anything
	/// else, which is left alone
	virtual bool Remove(entt::entity entity) = 0;
	/// A seed straight into the player's hand, fully charged and ready, as from a bubble, if the hand is free: the seed,
	/// or none
	virtual entt::entity GiveSeedToHand(PlayerNames player, SpellSeedType seed, int powerUp, float multiplier) = 0;
	/// The seed of a one-shot miracle (a globe, or a miracle that a script puts into the hand) goes into the player's
	/// hand, as GiveSeedToHand does. The best icon of the player for the seed makes it. Thus the seed can power up, and
	/// it gives its prayer power back to the site of the icon. With no such icon, the seed has no icon. The seed, or none.
	virtual entt::entity GiveOneShotSeedToHand(PlayerNames player, SpellSeedType seed, int powerUp, float multiplier)
	{
		return GiveSeedToHand(player, seed, powerUp, multiplier);
	}
	/// A seed summoned from the player's worship into their hand, if it is free: its cost to create is charged from the
	/// player's prayer power, as much as they have, and it is ready after the usual delay. The seed, or none.
	virtual entt::entity SummonSeed(PlayerNames player, SpellSeedType seed, int powerUp) = 0;
	/// A seed summoned from an icon of a worship site into its player's hand, if it is free: the site charges it with its
	/// cost to create, as much as it has, and tops up what it casts. The seed, or none.
	virtual entt::entity SummonSeedAtSite(entt::entity site, SpellSeedType seed, int powerUp) = 0;
	/// A miracle charged at an icon of a worship site goes into the hand of the player that tapped or charged the icon, if
	/// it is free, holding the prayer power the icon stored for it; the site tops up what it casts. On the Gods'
	/// Playground that hand can be of a player that does not own the site. The seed, or none.
	virtual entt::entity TakeIconSeed(entt::entity /*site*/, PlayerNames /*holder*/, SpellSeedType /*seed*/, int /*powerUp*/,
	                                  float /*chants*/)
	{
		return entt::null;
	}
	/// An icon that charged a seed held in a hand sets the power-up level of that seed. When the seed holds more than the
	/// miracle at that level costs, the seed gives the rest back to its worship site.
	virtual void SetIconSeedPowerUp(entt::entity /*seed*/, int /*powerUp*/) {}
	/// The hand of a player takes back its own seed that shows over the miracle that it cast. The seed takes what the
	/// miracle has left, and the miracle closes down. With enough left for another cast, the seed goes into the free
	/// hand as the held miracle, ready at once. Else the seed goes. Whether the hand keeps the seed.
	virtual bool TakeSeedBack(PlayerNames /*player*/, entt::entity /*seed*/) { return false; }
	/// The tooltip text of the miracle in the hand, once after the hand takes a seed or the seed gets a new power-up
	/// level. The hand shows this text at once, over the other tooltips. None when there is no new text.
	[[nodiscard]] virtual std::optional<uint32_t> TakeHeldMiracleToolTip() { return std::nullopt; }
	/// The hand drops the miracle it holds, as a scribble or a shake does: what was started is called off and what the
	/// seed still holds goes back to the player's worship
	virtual void DiscardHeldSeed() = 0;
	/// The hand of the player drops the miracle that it holds, as DiscardHeldSeed does for the player of this computer.
	/// The result is the same on each computer, so a tool can use it before the same turn on each computer.
	virtual void DiscardHeldSeedOf(PlayerNames player) = 0;
	/// A seed goes with the icon that made it: out of the hand if it is held, and a miracle still running from it closes
	/// down. What it holds is lost.
	virtual void DeleteSeed(entt::entity seed) = 0;

	// The hand

	/// Once a frame, before any press or release. The hand's movement and its spin are measured at the steps of the
	/// input that the frame takes (see input_steps), the hand at its place between the last frame and this frame.
	virtual void UpdateHand(const HandFrame& frame, const input_steps::Steps& steps) = 0;
	/// A fireball tapped by the empty hand within the player's influence. The hand sends the tap as a command of the
	/// player, and every computer catches the ball at the turn: another player's goes into the hand, the player's own
	/// slips through. Whether the hand sent the tap.
	virtual bool TapFireBall(entt::entity ball) = 0;
	/// A one-shot globe tapped by the empty hand: its miracle goes into the hand, charged in full, and the globe pops.
	/// Whether the hand took the miracle.
	virtual bool TapGlobe(entt::entity globe, PlayerNames player) = 0;
	/// The action button went down with an empty hand: a fireball under the hand, within the player's influence, takes
	/// the press. Whether one did, so nothing behind it is taken.
	virtual bool PressFireBall(uint32_t nowMs, uint32_t turn) = 0;
	/// Once a frame while the action button is down: a fireball pressed for as long as a tap may last is taken, which
	/// catches it as a tap would
	virtual void HoldFireBall(uint32_t nowMs, uint32_t turn) = 0;
	/// The action button came up: let go quickly, the fireball pressed is tapped
	virtual void ReleaseFireBall(uint32_t nowMs, uint32_t turn) = 0;
	/// At the turn: the tap or the take of a fireball that the hand sent (player_commands::HandTap, HandTake). Another
	/// player's ball is caught into the empty hand; the player's own slips through.
	virtual void DoCatchFireBall(PlayerNames /*player*/, entt::entity /*ball*/) {}
	/// At the turn: the use of the miracle in the hand on a thing that the hand sent (player_commands::HandApply). A ready
	/// fire seed takes in a fireball. Whether the miracle in the hand took the command.
	virtual bool DoApplySeed(PlayerNames /*player*/, entt::entity /*target*/) { return false; }
	/// At the turn: a gesture on the miracle in the hand that the hand sent (player_commands::SeedGesture)
	virtual void DoSeedGesture(PlayerNames /*player*/, const player_commands::SeedGesture& /*gesture*/) {}
	/// At the turn: the cast of the miracle in the hand that the hand sent (player_commands::HandCast)
	virtual void DoCast(PlayerNames /*player*/, const player_commands::HandCast& /*cast*/) {}
	/// At the turn: the let go of a miracle locked on in the hand that the hand sent (player_commands::HandCastUnlock)
	virtual void DoCastUnlock(PlayerNames /*player*/, entt::entity /*seed*/) {}
	/// The hand's own work of each turn, before the turn: a miracle locked on in the hand sends its cast again
	virtual void InterfaceTurn() {}
	/// The one-shot bubble under the hand now, if any
	[[nodiscard]] virtual std::optional<entt::entity> OrbUnderHand() const = 0;
	/// The action button went down with a miracle in the hand: it is armed, locked on or cast. Whether the miracles took
	/// the press, so the creatures don't.
	virtual bool PressAction() = 0;
	/// The action button came up: an armed miracle is cast, a locked one let go
	virtual void ReleaseAction() = 0;
	/// Whether the hand of this computer holds a miracle, so it doesn't take hold of creatures or things
	[[nodiscard]] virtual bool IsHandBusy() const = 0;
	/// Whether the hand of a player holds a miracle. The turns read this, because it is the same on every computer.
	[[nodiscard]] virtual bool IsHandBusyOf(PlayerNames player) const = 0;
	/// How a pour of food or wood lifts and tips the hand now, a fraction of the way from the last turn to the next
	[[nodiscard]] virtual magic::PourPose GetHandPour(float fraction) const = 0;
	[[nodiscard]] virtual HandCastState GetHandCastState() const = 0;
	/// The miracle seed in the hand of this computer
	[[nodiscard]] virtual std::optional<entt::entity> GetHeldSeed() const = 0;
	/// The miracle seed in the hand of a player. The turns read this, because it is the same on every computer.
	[[nodiscard]] virtual std::optional<entt::entity> GetHeldSeedOf(PlayerNames player) const = 0;
	[[nodiscard]] virtual HandResult GetLastHandResult() const = 0;
	/// Once a frame, once the hand is posed: where the held miracle's in-hand effect sits, and how large the hand is
	/// drawn, which is how large the effect is
	virtual void PlaceHandEffect(glm::vec3 /*point*/, float /*handScale*/) {}
	/// Whether the point under the hand is in the influence of the player whose seed it holds (or influence is ignored)
	[[nodiscard]] virtual bool IsHandInInfluence() const { return true; }
	/// The miracle held in the hand and where it may be cast, the pointer over a thing or over the bare land; none
	/// without a miracle in the hand
	[[nodiscard]] virtual std::optional<magic::HeldCastAtPointer> HeldCastAt(std::optional<entt::entity> /*pointed*/) const
	{
		return std::nullopt;
	}

	// What a miracle's objects in the world do for it

	/// An event a miracle's object sends it, as its particles do, such as a blow on the physical shield's dome; whether it
	/// acted
	virtual bool SendSpellEvent(entt::entity spell, const particles::SpellEventInfo& event) = 0;
	/// A miracle is made to pay prayer power, its caster asked for the whole shortfall, as a blow on a shield is; its
	/// strength after
	virtual float ForcePayForSpell(entt::entity spell, float cost) = 0;
	/// A miracle's strength now, 0 once it has gone
	[[nodiscard]] virtual float SpellStrength(entt::entity spell) = 0;

	// Turns and frames

	/// Once a game turn, before the particles: the dispensers, then every miracle's upkeep, effect and events
	virtual void ProcessTurn() = 0;
	/// Once a frame: the held miracle follows the hand, the bubbles' seeds spin
	virtual void Update(float seconds) = 0;
	/// Once a frame, after the frame makes the land's draw list: the swirls of the dispensers that the frame draws show
	/// and move on, and the other swirls do not show. They step no more often than 60 times a second, by the game time
	/// since the last step.
	virtual void UpdateDispenserFrame(const ObjectDrawListSystemInterface& /*drawList*/,
	                                  std::chrono::milliseconds /*frameRealTime*/, std::chrono::milliseconds /*frameGameTime*/,
	                                  bool /*landDrawn*/)
	{
	}
	/// A new land: every miracle goes
	virtual void Reset() = 0;

	// For the debug window and the testbed

	/// For the testbed and the debug window, until worship sets them: a player's power multiplier for a tribe
	virtual void SetTribalPower(PlayerNames /*player*/, Tribe /*tribe*/, float /*power*/) {}
	/// A player's power multiplier for a tribe, 1 until something raises it
	[[nodiscard]] virtual float GetTribalPower(PlayerNames /*player*/, Tribe /*tribe*/) const { return 1.0f; }
	/// A cheat for the debug window: the miracles may be cast outside the player's influence
	virtual void SetIgnoreInfluence(bool ignore) = 0;
	/// For the testbed's scenarios: the hand is where the scenario puts it, not where the mouse is, until none is given,
	/// so that a scenario can cast through the hand as a player does
	virtual void DriveHand(std::optional<HandFrame> frame) = 0;
	/// Where a scenario puts the hand, if it does: the hand is drawn there
	[[nodiscard]] virtual std::optional<HandFrame> GetDrivenHand() const = 0;
	[[nodiscard]] virtual bool IsIgnoringInfluence() const = 0;
	[[nodiscard]] virtual std::vector<SpellInfo> GetSpells() const = 0;
	/// A miracle of the player of this computer follows the hand at each turn, so the hand report goes at each turn
	[[nodiscard]] virtual bool SpellNeedsHandReportEachTurn() const { return false; }
	/// The newest miracle of a kind, closing down or not, whose last event was strictly within a radius of a point
	[[nodiscard]] virtual std::optional<entt::entity> SpellAt(MagicType /*type*/, glm::vec3 /*point*/, float /*radius*/) const
	{
		return std::nullopt;
	}
	/// The rain puts out a fire at a point: the first storm miracle whose size covers it, without such a reaction going,
	/// has the people come to watch
	virtual void RainOnFire(const glm::vec3& point) = 0;
	[[nodiscard]] virtual std::vector<DispenserInfo> GetDispensers() const = 0;
};

} // namespace openblack::ecs::systems
