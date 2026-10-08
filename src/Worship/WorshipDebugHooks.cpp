/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Environment-variable test hooks of the worship and miracle supply (documented in docs/bw1-notes/openblack-internals.md):
//   OPENBLACK_TEST_TOWN_SPELL="<town>,<MAGIC>[;<town>,<MAGIC>...]"   SET_MAGIC_IN_OBJECT(town, magic, 1), turn 1
//   OPENBLACK_TEST_MANA="<chants>"                                   GAME_SET_MANA on the local player's first site, turn 1
//   OPENBLACK_TEST_WORSHIP="<town>,<fraction>"                       SetWorshipPercentage (the totem drag), turn 1
//   OPENBLACK_TEST_TAP_ICON="<SEED>[,turn[,turn...]]"                 a tap on the local player's site icon of that seed
//   OPENBLACK_TEST_TAP="x,z,turn"                                    a tap on the tappable object nearest (x, z): an
//                                                                    icon, a town centre icon or a one-shot orb
//   OPENBLACK_TEST_DISPENSER="<ABODE>,x,z,<MAGIC>[,seconds]"         a dispenser as the challenge script makes one
//                                                                    (GiveSpellDispenserReward), turn 1
//   OPENBLACK_TEST_FIREFLY_REWARD="x,z[,n]"                          fire_fly::Reward n times there, turn 1
//   OPENBLACK_TEST_WORSHIP_PLAYER="<n>"                              the hooks act as player n, not the human one
//   OPENBLACK_TEST_WORSHIP_SITE="<TRIBE>[,<SEED>...]"                CREATE_WORSHIP_SITE on that player's citadel, with
//                                                                    an icon of each seed, turn 1
//   OPENBLACK_CAMERA_LOCK="ox,oy,oz,fx,fy,fz"                         the camera put there every turn (screenshots while
//                                                                    the Land 1 script moves the camera)
// <MAGIC> is a MAGIC_TYPE number or its info.dat name, <SEED> a SPELL_SEED_TYPE number or name, <ABODE> an abode info
// name (NORSE_ABODE_SPELL_DISPENSER...), <town> a town id.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Citadel.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "FireFlyReward.h"
#include "InfoConstants.h"
#include "InterfaceStatus.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicTables.h"
#include "PlayerSpellIcons.h"
#include "SpecialPoints.h"
#include "SpellDispenser.h"
#include "TownMagic.h"
#include "Worship.h"
#include "WorshipPercentage.h"
#include "WorshipSite.h"
#include "WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct WorshipDebugHooksState
{
	bool firstDone {false};
};

WorshipDebugHooksState& WorshipDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<WorshipDebugHooksState>();
}

std::vector<std::string> Split(const std::string& text, char separator)
{
	std::vector<std::string> parts;
	std::stringstream stream(text);
	std::string part;
	while (std::getline(stream, part, separator))
	{
		parts.push_back(part);
	}
	return parts;
}

bool IsNumber(const std::string& text)
{
	char* end = nullptr;
	std::strtol(text.c_str(), &end, 10);
	return !text.empty() && end != text.c_str() && *end == '\0';
}

int MagicFromText(const std::string& text)
{
	return IsNumber(text) ? std::atoi(text.c_str())
	                      : magic::GetInfoFromText(Locator::infoConstants::value(), text).value_or(magic::k_MagicTypeNotFound);
}

int SeedFromText(const std::string& text)
{
	return IsNumber(text)
	           ? std::atoi(text.c_str())
	           : magic::GetSpellSeedFromText(Locator::infoConstants::value(), text).value_or(magic::k_SpellSeedNotFound);
}

PlayerNames LocalPlayer()
{
	// OPENBLACK_TEST_WORSHIP_PLAYER=<n>: the hooks act as that player instead of the human one. Land 2 starts the human
	// player with only a planned citadel and an empty town, so its own site appears later: this is how the chain is
	// tested on another player's citadel.
	static const debug_env::Variable k_TestWorshipPlayer("OPENBLACK_TEST_WORSHIP_PLAYER");
	if (const char* value = k_TestWorshipPlayer.Get(); value != nullptr)
	{
		const int player = std::atoi(value);
		if (player >= 0 && player < static_cast<int>(PlayerNames::_COUNT))
		{
			return static_cast<PlayerNames>(player);
		}
	}
	for (int p = 0; p < static_cast<int>(PlayerNames::_COUNT); ++p)
	{
		if (magic::players::IsHuman(static_cast<PlayerNames>(p)))
		{
			return static_cast<PlayerNames>(p);
		}
	}
	return PlayerNames::PLAYER_ONE;
}

glm::vec3 LandPoint(float x, float z)
{
	glm::vec3 point(x, 0.0f, z);
	point.y = GroundAt(point);
	return point;
}

void TestTownSpells(const char* value)
{
	for (const auto& entry : Split(value, ';'))
	{
		const auto parts = Split(entry, ',');
		if (parts.size() < 2)
		{
			continue;
		}
		const auto town = town::FromId(static_cast<uint32_t>(std::atoi(parts[0].c_str())));
		const int type = MagicFromText(parts[1]);
		if (town == entt::null || type <= 0 || type >= static_cast<int>(magic::k_MagicTypeCount))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "OPENBLACK_TEST_TOWN_SPELL: no town {} or magic {}", parts[0], parts[1]);
			continue;
		}
		town::AddMagicTypesHeld(town, static_cast<MagicType>(type));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_TOWN_SPELL: town {} holds {}", parts[0], parts[1]);
	}
}

entt::entity FirstSite(PlayerNames player)
{
	const auto citadel = citadel::Of(player);
	if (citadel == entt::null)
	{
		return entt::null;
	}
	for (const auto site : Locator::entitiesRegistry::value().Get<const CitadelWorship>(citadel).sites)
	{
		if (site != entt::null && Locator::entitiesRegistry::value().Valid(site))
		{
			return site;
		}
	}
	return entt::null;
}

/// The stock land scripts never give the human player a built town centre, so its citadel never gets a site of its own
/// (the village would have to grow). This runs the real CREATE_WORSHIP_SITE on its citadel and, for each seed listed,
/// site::AddSpellIconIfNecessary.
void TestWorshipSite(const char* value)
{
	const auto parts = Split(value, ',');
	if (parts.empty())
	{
		return;
	}
	const auto player = LocalPlayer();
	const auto citadelEntity = citadel::Of(player);
	if (citadelEntity == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "OPENBLACK_TEST_WORSHIP_SITE: player {} has no citadel",
		                   static_cast<int>(player));
		return;
	}
	auto tribe = static_cast<Tribe>(std::atoi(parts[0].c_str()));
	if (!IsNumber(parts[0]))
	{
		tribe = Tribe::NONE;
		for (size_t t = 0; t < k_TribeStrs.size(); ++t)
		{
			if (k_TribeStrs.at(t) == parts[0])
			{
				tribe = static_cast<Tribe>(t);
			}
		}
	}
	const auto site = citadel::CreateBuiltWorshipSite(citadelEntity, tribe);
	if (site == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "OPENBLACK_TEST_WORSHIP_SITE: no site for tribe {}", parts[0]);
		return;
	}
	for (size_t i = 1; i < parts.size(); ++i)
	{
		const int seed = SeedFromText(parts[i]);
		if (seed >= 0 && seed < static_cast<int>(magic::k_SpellSeedCount))
		{
			site::AddSpellIconIfNecessary(site, static_cast<SpellSeedType>(seed));
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_WORSHIP_SITE: site {} of tribe {} for player {}",
	                   static_cast<uint32_t>(site), parts[0], static_cast<int>(player));
}

void TestMana(const char* value)
{
	const auto site = FirstSite(LocalPlayer());
	if (site == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "OPENBLACK_TEST_MANA: the local player has no worship site");
		return;
	}
	site::SetMana(site, static_cast<float>(std::atof(value)));
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_MANA: site {} battery {}", static_cast<uint32_t>(site), value);
}

void TestWorship(const char* value)
{
	const auto parts = Split(value, ',');
	if (parts.size() < 2)
	{
		return;
	}
	const auto town = town::FromId(static_cast<uint32_t>(std::atoi(parts[0].c_str())));
	if (town == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "OPENBLACK_TEST_WORSHIP: no town {}", parts[0]);
		return;
	}
	percentage::SetWorshipPercentage(town, static_cast<float>(std::atof(parts[1].c_str())));
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_WORSHIP: town {} at {} ({} villagers)", parts[0], parts[1],
	                   town::Population(town));
}

void TestTapIcon(const char* value, uint32_t turn)
{
	const auto parts = Split(value, ',');
	if (parts.empty())
	{
		return;
	}
	bool now = parts.size() == 1 && turn == 5;
	for (size_t i = 1; i < parts.size(); ++i)
	{
		now |= static_cast<uint32_t>(std::atoi(parts[i].c_str())) == turn;
	}
	if (!now)
	{
		return;
	}
	const auto seed = static_cast<SpellSeedType>(SeedFromText(parts[0]));
	const auto player = LocalPlayer();
	for (const auto icon : player::Icons(player))
	{
		if (icon::SeedTypeOf(icon) == seed)
		{
			const float required = icon::GetChantRequired(icon);
			const auto magicType = icon::MagicTypeOf(icon, -1);
			const int result = InterfaceTap(icon, player);
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "OPENBLACK_TEST_TAP_ICON: turn {} tap on icon {} (magic {}, needs {:.0f}) -> {} "
			                   "(store {:.0f}, charging {}; hand {}, valid {})",
			                   turn, static_cast<uint32_t>(icon), static_cast<int>(magicType), required, result,
			                   Locator::entitiesRegistry::value().Get<const WorshipSpellIcon>(icon).chantStore,
			                   icon::IsCharging(icon, player, false), interface::IsHandReadyForObject(player),
			                   icon::ValidForStartCharge(icon, player, -1, false));
			return;
		}
	}
	SPDLOG_LOGGER_WARN(spdlog::get("game"), "OPENBLACK_TEST_TAP_ICON: no icon of seed {}", parts[0]);
}

void TestTap(const char* value, uint32_t turn)
{
	const auto parts = Split(value, ',');
	if (parts.size() < 3 || static_cast<uint32_t>(std::atoi(parts[2].c_str())) != turn)
	{
		return;
	}
	const glm::vec2 at(static_cast<float>(std::atof(parts[0].c_str())), static_cast<float>(std::atof(parts[1].c_str())));
	auto& registry = Locator::entitiesRegistry::value();
	const auto player = LocalPlayer();
	entt::entity best = entt::null;
	float bestDistance = std::numeric_limits<float>::max();
	registry.Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		if (!registry.AnyOf<SpellIcon, OneOffSpellSeed>(entity))
		{
			return;
		}
		const float distance = glm::distance(at, glm::vec2(transform.position.x, transform.position.z));
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = entity;
		}
	});
	if (best == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "OPENBLACK_TEST_TAP: nothing tappable");
		return;
	}
	const int result = InterfaceTap(best, player);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_TAP: turn {} tap on {} ({:.1f} m away) -> {}", turn,
	                   static_cast<uint32_t>(best), bestDistance, result);
}

void TestDispenser(const char* value)
{
	const auto parts = Split(value, ',');
	if (parts.size() < 4)
	{
		return;
	}
	const auto abode = GAbodeInfo::Find(parts[0]);
	const auto position =
	    LandPoint(static_cast<float>(std::atof(parts[1].c_str())), static_cast<float>(std::atof(parts[2].c_str())));
	const int type = MagicFromText(parts[3]);
	// GiveSpellDispenserReward: CREATE_WITH_ANGLE_AND_SCALE(SPELL_DISPENSER, abode, pos, 1.0, angle),
	// SET_MAGIC_PROPERTIES(d, magic, 0), SET_ACTIVE(d, 1), and SET_TIMER_TIME(d, seconds) when given
	const auto dispenser = dispenser::Create(position, abode, -1, 0.0f, 1.0f);
	if (dispenser == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "OPENBLACK_TEST_DISPENSER: no dispenser {}", parts[0]);
		return;
	}
	dispenser::SetMagicProperties(dispenser, static_cast<MagicType>(type), 0.0f);
	dispenser::SetActive(dispenser, true);
	if (parts.size() > 4 && std::atof(parts[4].c_str()) > 0.0)
	{
		dispenser::SetTimerTime(dispenser, static_cast<float>(std::atof(parts[4].c_str())));
	}
}

void TestFireFlyReward(const char* value)
{
	const auto parts = Split(value, ',');
	if (parts.size() < 2)
	{
		return;
	}
	const auto position =
	    LandPoint(static_cast<float>(std::atof(parts[0].c_str())), static_cast<float>(std::atof(parts[1].c_str())));
	const int count = parts.size() > 2 ? std::atoi(parts[2].c_str()) : 1;
	for (int i = 0; i < count; ++i)
	{
		const auto orb = fire_fly::Reward(position + glm::vec3(static_cast<float>(i) * 6.0f, 0.0f, 0.0f));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "OPENBLACK_TEST_FIREFLY_REWARD: total {:.1f} -> orb {}", fire_fly::Total(),
		                   orb == entt::null ? -1 : static_cast<int>(static_cast<uint32_t>(orb)));
	}
}
} // namespace

void worship::RunDebugHooks(uint32_t turn)
{
	if (!WorshipDebugHooksData().firstDone && turn >= 1)
	{
		WorshipDebugHooksData().firstDone = true;
		static const debug_env::Variable k_TestWorshipSite("OPENBLACK_TEST_WORSHIP_SITE");
		if (const char* value = k_TestWorshipSite.Get(); value != nullptr)
		{
			TestWorshipSite(value);
		}
		static const debug_env::Variable k_TestTownSpell("OPENBLACK_TEST_TOWN_SPELL");
		if (const char* value = k_TestTownSpell.Get(); value != nullptr)
		{
			TestTownSpells(value);
		}
		static const debug_env::Variable k_TestMana("OPENBLACK_TEST_MANA");
		if (const char* value = k_TestMana.Get(); value != nullptr)
		{
			TestMana(value);
		}
		static const debug_env::Variable k_TestWorship("OPENBLACK_TEST_WORSHIP");
		if (const char* value = k_TestWorship.Get(); value != nullptr)
		{
			TestWorship(value);
		}
		static const debug_env::Variable k_TestDispenser("OPENBLACK_TEST_DISPENSER");
		if (const char* value = k_TestDispenser.Get(); value != nullptr)
		{
			TestDispenser(value);
		}
		static const debug_env::Variable k_TestFireflyReward("OPENBLACK_TEST_FIREFLY_REWARD");
		if (const char* value = k_TestFireflyReward.Get(); value != nullptr)
		{
			TestFireFlyReward(value);
		}
	}
	static const debug_env::Variable k_TestTapIcon("OPENBLACK_TEST_TAP_ICON");
	if (const char* value = k_TestTapIcon.Get(); value != nullptr)
	{
		TestTapIcon(value, turn);
	}
	static const debug_env::Variable k_TestTap("OPENBLACK_TEST_TAP");
	if (const char* value = k_TestTap.Get(); value != nullptr)
	{
		TestTap(value, turn);
	}
	// OPENBLACK_CAMERA_FLY also holds the camera at its end point every turn: the land scripts take the camera
	// (START_CAMERA_CONTROL; Land 1's intro waits for MOVE_GAME_THING), so a one-off flight is undone
	static const debug_env::Variable k_CameraLock("OPENBLACK_CAMERA_LOCK");
	const char* lock = k_CameraLock.Get();
	if (lock == nullptr)
	{
		static const debug_env::Variable k_CameraFly("OPENBLACK_CAMERA_FLY");
		lock = k_CameraFly.Get();
	}
	if (const char* value = lock; value != nullptr && Locator::camera::has_value())
	{
		glm::vec3 origin(0.0f);
		glm::vec3 focus(0.0f);
		if (std::sscanf(value, "%f,%f,%f,%f,%f,%f", &origin.x, &origin.y, &origin.z, &focus.x, &focus.y, &focus.z) == 6)
		{
			Locator::camera::value().SetOrigin(origin).SetFocus(focus);
		}
	}
}

void worship::ResetDebugHooks()
{
	WorshipDebugHooksData().firstDone = false;
}
