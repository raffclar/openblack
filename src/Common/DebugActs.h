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

#include <array>
#include <optional>
#include <string_view>

#include <glm/vec2.hpp>

#include "Commands/PlayerCommands.h"
#include "Enums.h"

/// The acts of the debug windows and of the menus of the debug menu bar that change the world: the acts of the advisors,
/// the changes of the time of the day, of the alignment of a player and of the bonus creatures, and the lines of the
/// debug Console. In a network game each act goes as a command of the turns, and each computer does it at the turn. In
/// a game of this computer alone the act occurs at once.
namespace openblack::debug_acts
{

/// The advisor of an act: both, the good one or the evil one. The good and the evil one have the numbers of the
/// spirits of the scripts.
inline constexpr uint8_t k_BothAdvisors = 0;
inline constexpr uint8_t k_GoodAdvisor = 1;
inline constexpr uint8_t k_EvilAdvisor = 2;
/// The count of the animations of an advisor
inline constexpr uint8_t k_AdvisorAnims = 80;
/// The last emotion of an advisor
inline constexpr uint8_t k_LastEmotion = 7;
/// The slowest and the fastest speed of an animation of an advisor, as the Advisors window gives it
inline constexpr float k_SlowestAnim = 0.1f;
inline constexpr float k_FastestAnim = 3.0f;
/// The hours of a day
inline constexpr float k_HoursInDay = 24.0f;

[[nodiscard]] std::string_view NameOf(player_commands::DebugAdvisor::Act act) noexcept;
[[nodiscard]] std::string_view NameOf(player_commands::DebugWorld::Act act) noexcept;

/// Why no computer does the act, none when each computer does it. The game server refuses an act with a refusal, and each
/// computer checks the act again at the turn.
[[nodiscard]] std::optional<std::string_view> AdvisorActRefusal(const player_commands::DebugAdvisor& act) noexcept;
[[nodiscard]] std::optional<std::string_view> WorldActRefusal(const player_commands::DebugWorld& act) noexcept;
[[nodiscard]] std::optional<std::string_view> ConsoleLineRefusal(const player_commands::DebugConsoleLine& line) noexcept;
/// The refusal of a command that only a player with the debugger claim sends, none for every other command
[[nodiscard]] std::optional<std::string_view> RefusalOf(const player_commands::Action& action) noexcept;

/// The numbers of the spirits of the advisors of an act: one or two numbers, and 0 for no spirit
[[nodiscard]] constexpr std::array<int32_t, 2> SpiritsOf(uint8_t advisor) noexcept
{
	if (advisor == k_BothAdvisors)
	{
		return {k_GoodAdvisor, k_EvilAdvisor};
	}
	return {static_cast<int32_t>(advisor), 0};
}

/// Does an act on the advisors. The control is the controller of the advisors of the game, or a fake for the tests. It
/// has the same functions that the scripts use. Returns false when the act is refused, and then nothing changes.
template <class Control>
bool ApplyAdvisorAct(Control& control, const player_commands::DebugAdvisor& act)
{
	using Act = player_commands::DebugAdvisor::Act;
	if (AdvisorActRefusal(act).has_value())
	{
		return false;
	}
	for (const auto spirit : SpiritsOf(act.advisor))
	{
		if (spirit == 0)
		{
			continue;
		}
		switch (act.act)
		{
		case Act::Out:
			control.SpiritEject(spirit, false);
			break;
		case Act::Appear:
			control.SpiritEject(spirit, true);
			break;
		case Act::Home:
			control.SpiritHome(spirit, false);
			break;
		case Act::Vanish:
			control.SpiritHome(spirit, true);
			break;
		case Act::Fly:
			control.SpiritFly(spirit, act.place.x, act.place.y);
			break;
		case Act::Cling:
			control.SpiritCling(spirit, act.place.x, act.place.y);
			break;
		case Act::ScreenPoint:
		{
			// Each computer points at the same part of its own screen
			const auto& screen = control.GetScreen();
			control.SpiritScreenPoint(spirit,
			                          glm::ivec2(static_cast<int32_t>(static_cast<float>(screen.width) * act.place.x),
			                                     static_cast<int32_t>(static_cast<float>(screen.height) * act.place.y)));
			break;
		}
		case Act::PointAt:
			control.SpiritPointPosition(spirit, act.position, act.inWorld);
			break;
		case Act::LookAt:
			control.SpiritLookAtPosition(spirit, act.position);
			break;
		case Act::StopPointing:
			control.SpiritStopPointing(spirit);
			break;
		case Act::StopLooking:
			control.SpiritStopLooking(spirit);
			break;
		case Act::PlayAnim:
			control.SpiritPlayAnim(spirit, act.place.x, act.place.y, act.anim, act.speed);
			break;
		case Act::Feel:
			// The good advisor is the first of the two, and the evil advisor is the second
			control.Dude(spirit == k_GoodAdvisor ? 0 : 1).SetEmotion(act.emotion, 1.0f);
			break;
		}
	}
	return true;
}

/// The parts of the world that a change of the debug menus changes. The game gives its systems, and the tests give a
/// fake.
class WorldTarget
{
public:
	virtual ~WorldTarget() = default;
	/// The time of the day goes to the hours, from 0 to less than 24
	virtual void SetTimeOfDay(float hours) = 0;
	virtual void SetPlayerAlignment(PlayerNames player, float alignment) = 0;
	virtual void SetBonusUnlocked(size_t bonus, bool unlocked) = 0;
};

/// Does a change of the debug menus. Returns false when the change is refused, and then nothing changes.
bool ApplyWorldAct(WorldTarget& target, const player_commands::DebugWorld& act);

} // namespace openblack::debug_acts
