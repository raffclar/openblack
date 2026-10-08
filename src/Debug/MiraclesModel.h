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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureSpells.h"
#include "ECS/Effects/Reactions.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Magic/Core/SpellCastData.h"
#include "Particles/SpellLink.h"

// What the Miracles debug window works out before it acts: which seeds and power-up levels there are, the miracle a
// choice casts and what the cast hands it, the one-shot orb or miracle dispenser it makes, the thing nearest a click,
// the infinite prayer power switch that gives every worship site back what it had once turned off, and the rows of the
// villagers' reactions. Pure functions and small value types, tested with hand-made tables and fakes.

namespace openblack::debug::miracles
{

/// The power-up level of a seed's plain miracle; its power-ups are levels 0, 1 and 2
inline constexpr int k_BasePowerUpLevel = -1;

/// What the window casts: a seed at one of its power-up levels
struct Choice
{
	SpellSeedType seed {SpellSeedType::None};
	int powerUpLevel {k_BasePowerUpLevel};
};

/// The seeds that cast a miracle at their plain level, in the tables' order
[[nodiscard]] std::vector<SpellSeedType> CastableSeeds(const InfoConstants& info);

/// The power-up levels a seed has, from its plain miracle up
[[nodiscard]] std::vector<int> PowerUpLevels(const GSpellSeedInfo& seed);

/// The level a choice keeps when its seed changes: the same one if the new seed has it, else the plain miracle
[[nodiscard]] int KeepLevel(const GSpellSeedInfo& seed, int powerUpLevel);

/// The miracle a choice casts, none for no seed
[[nodiscard]] MagicType MagicTypeOf(const InfoConstants& info, Choice choice);

/// How a power-up level reads in the window
[[nodiscard]] std::string LevelName(int powerUpLevel);

/// A cast from the window starts this far above the point it lands on, and is thrown straight down onto it
inline constexpr float k_CastHeight = 15.0f;
inline constexpr float k_CastThrowSpeed = 10.0f;

/// What a cast from the window hands the miracles
struct CastPlan
{
	MagicType type {MagicType::None};
	magic::SpellCastData cast;
	psys::ProcessInfo process;
};

/// The cast of a choice at a point as a seed charged to `multiplier` casts it from the hand: its effect's prayer power
/// and the player's timer, both times the multiplier, at the size its tables give a cast that costs the normal amount
/// (1 for a miracle without a radius, and always 1 for the fire seed)
[[nodiscard]] CastPlan PlanCast(const InfoConstants& info, Choice choice, float multiplier, glm::vec3 point,
                                glm::vec3 cameraForward);

/// What the window casts through: today the game's spells, later another miracles system behind the same calls
class CasterInterface
{
public:
	virtual ~CasterInterface() = default;

	/// The new spell, or entt::null when it could not start
	virtual entt::entity CastAtPoint(const CastPlan& plan, PlayerNames player, glm::vec3 point) = 0;
	/// A cast on a thing: on the thing itself when its miracle is cast on things, else at its feet
	virtual entt::entity CastOnObject(const CastPlan& plan, PlayerNames player, entt::entity target) = 0;
	/// Whether the hand could cast the miracle at the point
	[[nodiscard]] virtual bool CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) const = 0;
};

/// Where a cast goes: a point on the land, or a thing
using Target = std::variant<glm::vec3, entt::entity>;

/// What became of a cast
struct CastResult
{
	entt::entity spell {entt::null};
	std::string message;
};

/// Casts the plan at its target through the caster; a plan without a miracle casts nothing
[[nodiscard]] CastResult Cast(CasterInterface& caster, const CastPlan& plan, std::string_view name, PlayerNames player,
                              const Target& target);

/// What the window makes of the chosen miracle, as the game's scripts make them
enum class DispenserKind : uint8_t
{
	/// A one-shot orb on the land: the seed at its power-up level, taken once
	OneShot,
	/// The same seed straight into the player's hand, fully charged
	OneShotInHand,
	/// A miracle dispenser: a building that makes a one-shot orb of its miracle, and another one each period after
	/// the last was taken
	Permanent,
};

/// What a dispenser from the window is made with
struct DispenserPlan
{
	DispenserKind kind {DispenserKind::OneShot};
	Choice choice;
	/// The building of a permanent dispenser
	AbodeInfo abode {AbodeInfo::NorseSpellDispenser};
	/// How many turns a permanent dispenser waits before its next orb; 0 leaves it inactive
	uint32_t periodTurns {0};
};

/// What the window makes dispensers through: today the game's one-shot orbs and dispensers, later another miracles
/// system behind the same calls. The points are on the land.
class DispenserCreatorInterface
{
public:
	virtual ~DispenserCreatorInterface() = default;

	/// The new orb, or entt::null
	virtual entt::entity CreateOneShot(SpellSeedType seed, int powerUpLevel, glm::vec3 point) = 0;
	/// The seed in the hand, or entt::null when the hand could not take it
	virtual entt::entity CreateOneShotInHand(SpellSeedType seed, int powerUpLevel, PlayerNames player) = 0;
	/// The new dispenser with its miracle and period, or entt::null
	virtual entt::entity CreatePermanent(AbodeInfo abode, MagicType magic, uint32_t periodTurns, glm::vec3 point) = 0;
};

/// The buildings that are miracle dispensers, in the tables' order
[[nodiscard]] std::vector<AbodeInfo> DispenserAbodes(const InfoConstants& info);

/// The dispenser the window starts with: the Norse one the first land gives, else the first there is
[[nodiscard]] std::optional<AbodeInfo> DefaultDispenserAbode(std::span<const AbodeInfo> abodes);

/// A dispenser building's own period in turns, as a new one has it
[[nodiscard]] uint32_t DefaultPeriod(const InfoConstants& info, AbodeInfo abode);

/// Where the hand is: the right hand, which holds the miracles, else the left one
[[nodiscard]] std::optional<glm::vec3> HandPoint(std::optional<glm::vec3> left, std::optional<glm::vec3> right);

/// Makes the plan's dispenser at a point through the creator; the in-hand seed ignores the point. A plan without a
/// miracle makes nothing.
[[nodiscard]] CastResult CreateDispenser(DispenserCreatorInterface& creator, const InfoConstants& info,
                                         const DispenserPlan& plan, std::string_view name, PlayerNames player, glm::vec3 point);

/// A thing a click may pick, and where it stands
struct ThingAt
{
	entt::entity entity {entt::null};
	glm::vec3 position {0.0f};
};

/// A click picks the nearest thing within this distance of the land it hits, in metres
inline constexpr float k_PickDistance = 10.0f;

/// The thing nearest the point within the distance, measured across the land; the first one on a tie
[[nodiscard]] std::optional<entt::entity> NearestThing(std::span<const ThingAt> things, glm::vec3 point,
                                                       float maxDistance = k_PickDistance);

/// A worship site's prayer power cheats
struct PrayerCheats
{
	bool infinite {false};
	bool freeMaintenance {false};

	bool operator==(const PrayerCheats&) const = default;
};

/// Infinite prayer power at every worship site while it is on. Each site keeps what it had the first time the switch
/// reaches it, and gets that back when the switch goes off, so the game is as it was without the switch.
class InfinitePrayer
{
public:
	[[nodiscard]] bool IsOn() const { return _on; }

	/// Turns the switch on; nothing changes until Apply reaches the sites
	void TurnOn() { _on = true; }

	/// While on, once a frame for each site: its cheats on, what it had kept the first time
	void Apply(entt::entity site, PrayerCheats& cheats);

	/// Turns the switch off: each site reached gets back its own cheats through `restore`, which skips a site that is
	/// gone
	void TurnOff(const std::function<void(entt::entity site, PrayerCheats cheats)>& restore);

	/// How many sites the switch holds
	[[nodiscard]] size_t HeldSites() const { return _kept.size(); }

private:
	struct Kept
	{
		entt::entity site;
		PrayerCheats cheats;
	};

	bool _on {false};
	std::vector<Kept> _kept;
};

/// How a reaction type reads in the window
[[nodiscard]] std::string_view ReactionName(Reaction type);

/// One reaction as the window lists it
struct ReactionRow
{
	uint32_t id {0};
	std::string_view name;
	PlayerNames player {PlayerNames::NEUTRAL};
	float radius {0.0f};
	/// How many villagers follow it
	uint32_t followers {0};
	/// Its initiator is a miracle
	bool fromMiracle {false};
};

/// The rows of the reactions, in their order: only the ones a miracle started unless `all`. `isMiracle` tells whether
/// an initiator is a miracle, `followers` how many villagers follow a reaction.
[[nodiscard]] std::vector<ReactionRow> ReactionRows(std::span<const ecs::effects::reactions::Reaction> reactions, bool all,
                                                    const std::function<bool(entt::entity initiator)>& isMiracle,
                                                    const std::function<uint32_t(uint32_t reaction)>& followers);

/// A running miracle's age, out of its length when it has one
[[nodiscard]] std::string SpellAge(float age, float duration);
/// What a running miracle is doing
[[nodiscard]] std::string_view SpellState(bool closedDown);
/// What a dispenser is doing: its orb waits to be taken, it makes one, or it makes none
[[nodiscard]] std::string_view DispenserState(bool hasOrb, bool active);

[[nodiscard]] std::string_view CreatureSpellName(creature_spells::Spell spell);
[[nodiscard]] std::string_view CreatureSpellPhaseName(creature_spells::Phase phase);
/// The miracles on a creature, as one line: each one not off with its phase and the turns it has left
[[nodiscard]] std::string CreatureSpellsLine(uint32_t creature, const creature_spells::Spells& spells);

} // namespace openblack::debug::miracles
