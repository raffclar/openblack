/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ServerAdmin.h"

#include <numeric>

namespace openblack::server::admin
{

std::string_view StateName(LockstepHost::SeatState state) noexcept
{
	switch (state)
	{
	case LockstepHost::SeatState::Open:
		return "open";
	case LockstepHost::SeatState::Taken:
		return "taken";
	case LockstepHost::SeatState::Away:
		return "away";
	case LockstepHost::SeatState::Closed:
		return "closed";
	}
	return "unknown";
}

SeatReport SeatReportOf(uint8_t seat, const LockstepHost::Seat& state)
{
	const auto commands = std::accumulate(state.commands.begin(), state.commands.end(), uint32_t {0},
	                                      [](uint32_t sum, const auto& each) { return sum + each.second; });
	return SeatReport {
	    .seat = seat,
	    .state = StateName(state.state),
	    .name = state.name,
	    .account = state.account,
	    .ready = state.ready,
	    .connected = state.connection.has_value(),
	    .sessionNumber = state.sessionNumber,
	    .resumes = state.resumes,
	    .commands = commands,
	    .blocksAgain = state.blocksAgain,
	    .checksumsCompared = state.compares.compared,
	    .checksumsSame = state.compares.same,
	    .firstDifferentTurn = state.compares.firstDifferent,
	    .differs = state.compares.differs,
	};
}

RoomReport RoomReportOf(const LockstepHost& host)
{
	RoomReport report {
	    .starting = host.Starting(),
	    .started = host.Started(),
	    .paused = host.Paused(),
	    .gameOver = host.GameOver(),
	    .replaying = host.Replaying(),
	    .turnsDone = host.TurnsDone(),
	    .stoppedAfter = host.StoppedAfter(),
	    .observers = static_cast<uint32_t>(host.Observers()),
	    .refusedBlocks = host.RefusedBlocks(),
	    .bans = {host.Bans().begin(), host.Bans().end()},
	    .seats = {},
	};
	const auto& seats = host.Seats();
	report.seats.reserve(seats.size());
	for (size_t i = 0; i < seats.size(); ++i)
	{
		report.seats.push_back(SeatReportOf(static_cast<uint8_t>(i), seats[i]));
	}
	return report;
}

} // namespace openblack::server::admin
