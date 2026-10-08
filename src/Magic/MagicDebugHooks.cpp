/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Environment-variable test hooks of the miracles (documented in docs/bw1-notes/openblack-internals.md):
//   OPENBLACK_TEST_SPELL="<MAGIC>,x,z[,radius[,duration[,player[,curl]]]]"  a cast as the script's SPELL_AT_POS
//   OPENBLACK_TEST_SEED="<SEED>[,pu]"                                 a charged seed into the hand (one-shot path)
//   OPENBLACK_TEST_ONESHOT="<SEED>,x,z[,pu[,tap]]"                    a one-shot orb on the land (tap: into the hand)
// <MAGIC> is a MAGIC_TYPE number or the effect's info.dat name (FIRE, HEAL, STORM_PU2...); <SEED> a SPELL_SEED_TYPE
// number or the seed's name (FIRE, HEAL, STORM...). Each runs once, the first turn the land exists (or the game turn
// OPENBLACK_TEST_MAGIC_TURN=<n>).

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <string>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "CastRules.h"
#include "Core/OneOffSpellSeed.h"
#include "Core/Spell.h"
#include "Core/SpellCreator.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "HealDebugHooks.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MagicLoop.h"
#include "MagicTables.h"
#include "Script/CHLSpells.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct MagicDebugHooksState
{
	bool done {false};
};

MagicDebugHooksState& MagicDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<MagicDebugHooksState>();
}

bool IsNumber(const char* text)
{
	char* end = nullptr;
	std::strtol(text, &end, 10);
	return end != text && *end == '\0';
}

int MagicFromText(const char* text)
{
	if (IsNumber(text))
	{
		return std::atoi(text);
	}
	return GetInfoFromText(Locator::infoConstants::value(), text).value_or(k_MagicTypeNotFound);
}

int SeedFromText(const char* text)
{
	if (IsNumber(text))
	{
		return std::atoi(text);
	}
	return GetSpellSeedFromText(Locator::infoConstants::value(), text).value_or(k_SpellSeedNotFound);
}

float Ground(float x, float z)
{
	return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
}

/// The defaults below (radius 10, duration -2 = the magic's own timer) are test harness values, not from the original
void TestSpell(const char* value)
{
	std::array<char, 64> name = {};
	float x = 0.0f;
	float z = 0.0f;
	float radius = 10.0f;
	float duration = -2.0f;
	int player = -1;
	float curl = 0.0f; // SPELL_AT_POS's curl (the shields' spin)
	if (std::sscanf(value, "%63[^,],%f,%f,%f,%f,%d,%f", name.data(), &x, &z, &radius, &duration, &player, &curl) < 3)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Magic test: OPENBLACK_TEST_SPELL=\"{}\" not understood", value);
		return;
	}
	const int magic = MagicFromText(name.data());
	if (magic <= 0 || magic >= static_cast<int>(k_MagicTypeCount))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Magic test: no magic type {}", name.data());
		return;
	}
	const auto type = static_cast<MagicType>(magic);
	// no duration given: the player's timer (timerWhenPlayerCasting), as a hand cast would last
	if (duration == -2.0f)
	{
		duration = GetTimerWhenPlayerCasting(Locator::infoConstants::value(), type);
	}
	// the neutral player refills its spells (the script's creator); a player number casts as that player
	ecs::components::SpellCreator creator;
	if (player >= 0 && player < static_cast<int>(PlayerNames::_COUNT))
	{
		creator = creator::OfPlayer(static_cast<PlayerNames>(player));
	}
	const glm::vec3 target(x, Ground(x, z), z);
	// SPELL_AT_POS's "from": 30 m above the target (inferred: the challenge scripts cast from the sky)
	const glm::vec3 from = target + glm::vec3(0.0f, 30.0f, 0.0f);
	const auto spell = script::CastSpellAtPos(target, type, from, creator, false, radius, duration, curl, glm::vec3(0.0f));
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Magic test: {} ({}) at ({:.1f}, {:.1f}) radius {:.1f} duration {:.1f} player {} -> spell {}",
	                   name.data(), magic, x, z, radius, duration, player, spell == entt::null ? -1 : static_cast<int>(spell));
	// what the hand's CanCast would answer there (the script path does not ask)
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic test: CanCastAt({}) there = {} (class check, vt 0x30)", name.data(),
	                   cast_rules::CanCastAt(type, ToMap(target)));
}
} // namespace

void magic::ResetDebugHooks()
{
	MagicDebugHooksData().done = false;
	heal_debug::ResetDebugHooks();
}

void magic::RunDebugHooks()
{
	heal_debug::RunDebugHooks(); // OPENBLACK_TEST_HURT_VILLAGERS (HealDebugHooks.cpp), before a heal cast below
	if (MagicDebugHooksData().done || !Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	// OPENBLACK_TEST_MAGIC_TURN=<n>: the three hooks below wait for game turn n (screenshots of what happens later)
	if (const char* wait = std::getenv("OPENBLACK_TEST_MAGIC_TURN");
	    wait != nullptr && CurrentTurn() < static_cast<unsigned int>(std::max(0, std::atoi(wait))))
	{
		return;
	}
	MagicDebugHooksData().done = true;
	if (const char* value = std::getenv("OPENBLACK_TEST_SPELL"); value != nullptr)
	{
		TestSpell(value);
	}
	if (const char* value = std::getenv("OPENBLACK_TEST_SEED"); value != nullptr)
	{
		std::array<char, 64> name = {};
		int powerUp = -1;
		if (std::sscanf(value, "%63[^,],%d", name.data(), &powerUp) >= 1)
		{
			const int seedType = SeedFromText(name.data());
			const auto seed =
			    one_off::CreateSpellIntoHand(PlayerNames::PLAYER_ONE, static_cast<SpellSeedType>(seedType), powerUp, 1.0f);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic test: seed {} ({}) pu {} into the hand -> {}", name.data(), seedType,
			                   powerUp, seed == entt::null ? -1 : static_cast<int>(seed));
		}
	}
	if (const char* value = std::getenv("OPENBLACK_TEST_ONESHOT"); value != nullptr)
	{
		std::array<char, 64> name = {};
		std::array<char, 16> tap = {};
		float x = 0.0f;
		float z = 0.0f;
		int powerUp = -1;
		if (std::sscanf(value, "%63[^,],%f,%f,%d,%15s", name.data(), &x, &z, &powerUp, tap.data()) >= 3)
		{
			const int seedType = SeedFromText(name.data());
			const auto orb =
			    one_off::Create(glm::vec3(x, Ground(x, z), z), static_cast<SpellSeedType>(seedType), powerUp, 1.0f);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic test: one-shot {} ({}) pu {} at ({:.1f}, {:.1f}) -> orb {}",
			                   name.data(), seedType, powerUp, x, z, orb == entt::null ? -1 : static_cast<int>(orb));
			if (orb != entt::null && std::strcmp(tap.data(), "tap") == 0)
			{
				const int result = one_off::InterfaceTap(orb, PlayerNames::PLAYER_ONE);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic test: tapped the orb -> {}", result);
			}
		}
	}
}
