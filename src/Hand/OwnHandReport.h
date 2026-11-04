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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Commands/PlayerCommands.h"
#include "Hand/HandReportRules.h"
#include "Online/GameOrigin.h"

/// The pass of the hand of this computer at each turn. Before the block of commands goes, the computer reports the
/// thing under the cursor, and where the hand and the camera are (see HandReportRules.h). The reporter keeps what the
/// computer last sent, so that a part goes only when it changed.
namespace openblack::hand_report
{

/// The pass of this turn starts: the place of the hand, and whether the hand is not the hand of the last report
struct OwnStart
{
	glm::vec3 handPoint {0.0f};
	bool newHand {false};
};

/// What the pass reads of this computer at the turn
struct OwnInputs
{
	/// The place of the hand that the pass started with
	glm::vec3 handPoint {0.0f};
	/// The thing under the cursor, null for none, and the point that the cursor picks, none for no point. With no
	/// picks at all (none), the pass sends no thing under the cursor.
	std::optional<entt::entity> picked;
	std::optional<glm::vec3> pickPoint;
	/// The camera and its focus, as the last frame drew them
	glm::vec3 camera {0.0f};
	glm::vec3 focus {0.0f};
	/// Whether a miracle needs a report at each turn. The pass asks it only while the report waits.
	std::function<bool()> spellNeedsReport;
	/// Where the network game came from, none for a game alone
	std::optional<online::GameOrigin> origin;
};

/// The commands that the pass sends in this turn, in this sequence: the thing under the cursor, then the report
struct OwnSends
{
	std::optional<player_commands::HandPick> pick;
	std::optional<player_commands::HandReport> report;
};

/// The values that this computer last sent of its hand, and the rules that send the next values
class OwnReporter
{
public:
	/// Starts the pass of a turn. None when this turn did the pass already, or when the hand has no place. A new hand,
	/// as on a new land, starts with no report.
	[[nodiscard]] std::optional<OwnStart> Start(uint32_t turn, entt::entity hand, std::optional<glm::vec3> handPoint)
	{
		// The pass goes once a turn, after the turns of a frame that played one
		if (_turn == turn)
		{
			return std::nullopt;
		}
		_turn = turn;
		if (!handPoint.has_value())
		{
			return std::nullopt;
		}
		const bool newHand = hand != _hand;
		if (newHand)
		{
			_hand = hand;
			_last = k_NoReport;
			_collidePoint = glm::vec3(0.0f);
			_lastPick = entt::null;
		}
		return OwnStart {.handPoint = *handPoint, .newHand = newHand};
	}

	/// Ends the pass of a turn that Start started: gives the commands to send
	[[nodiscard]] OwnSends Finish(const OwnInputs& inputs)
	{
		OwnSends sends;
		// The thing under the cursor goes when it changes, so that the turn counts the same time that the hand stays
		// over the creature at each frame rate and on every computer
		if (inputs.picked.has_value() && *inputs.picked != _lastPick)
		{
			_lastPick = *inputs.picked;
			sends.pick = player_commands::HandPick {.object = *inputs.picked};
		}
		// The point that the interface picks under the cursor stays as it was while the cursor picks nothing
		_collidePoint = inputs.pickPoint.value_or(_collidePoint);
		// After a send the report waits for some turns. A miracle of this player that follows the hand stops the wait.
		const bool spellNeedsReport = _wait != 0 && inputs.spellNeedsReport && inputs.spellNeedsReport();
		if (WaitsThisTurn(_wait, spellNeedsReport))
		{
			_wait = TurnsToWaitAfter(_wait, true, false, inputs.origin);
			return sends;
		}
		const player_commands::HandReport now {
		    .collidePoint = _collidePoint,
		    .handPoint = inputs.handPoint,
		    .camera = inputs.camera,
		    .focus = inputs.focus,
		};
		// Only a part that moved enough goes. The other part keeps the values that the player last sent.
		const auto toSend = ToSend(_last, now);
		_wait = TurnsToWaitAfter(_wait, false, toSend.has_value(), inputs.origin);
		if (toSend.has_value())
		{
			_last = *toSend;
			sends.report = *toSend;
		}
		return sends;
	}

	/// The turns that the report still waits after its last send
	[[nodiscard]] uint8_t TurnsToWait() const noexcept { return _wait; }

private:
	/// The report before the first send: every value is zero
	static constexpr player_commands::HandReport k_NoReport {
	    .collidePoint = glm::vec3(0.0f),
	    .handPoint = glm::vec3(0.0f),
	    .camera = glm::vec3(0.0f),
	    .focus = glm::vec3(0.0f),
	};

	std::optional<uint32_t> _turn;
	entt::entity _hand {entt::null};
	/// The last report that this computer sent
	player_commands::HandReport _last {k_NoReport};
	/// The last point that the cursor picked
	glm::vec3 _collidePoint {0.0f};
	uint8_t _wait {0};
	/// The thing under the cursor that the last player_commands::HandPick sent
	entt::entity _lastPick {entt::null};
};

} // namespace openblack::hand_report
