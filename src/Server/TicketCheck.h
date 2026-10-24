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
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <JoinTicket.h>

namespace openblack::server
{

/// Why the game server did not accept a ticket
enum class TicketProblem : uint8_t
{
	None,
	/// The text is not a v4.public token, or its footer or its claims do not read
	Format,
	/// The footer names a key that the server does not have. The server can get the keys of the lobby again.
	UnknownKey,
	/// The signature is not valid for the key
	Signature,
	/// Another service made the ticket
	Issuer,
	/// The ticket is for another game server
	Audience,
	Expired,
	NotYetValid,
	/// A ticket works once
	AlreadyUsed,
};

/// The check of the lobby's join tickets: the signature with the lobby's keys, the issuer, the server, the times and
/// the single use. It keeps no global state; the caller gives the time and the check of the signature.
class TicketCheck
{
public:
	/// The difference between the clocks of the lobby and the server that a check allows
	static constexpr int64_t k_ClockSkewSeconds = 5;
	/// The issuer of the lobby's tickets
	static constexpr std::string_view k_Issuer = "bwgame-service";

	using VerifySignature = std::function<bool(const join_ticket::PublicKey& key, std::span<const uint8_t> bytes,
	                                           const join_ticket::Signature& signature)>;

	TicketCheck(std::string serverId, VerifySignature verify);

	/// The keys of the lobby. The new list replaces the old one.
	void SetKeys(std::vector<join_ticket::TicketKey> keys);
	[[nodiscard]] size_t KeyCount() const { return _keys.size(); }

	struct Result
	{
		TicketProblem problem {TicketProblem::None};
		/// The claims, when there is no problem
		std::optional<join_ticket::Claims> claims;
	};

	/// Checks a ticket at a time in seconds since 1970 (UTC). An accepted ticket's id is kept until the ticket expires,
	/// so that the same ticket does not work again.
	[[nodiscard]] Result Check(std::string_view token, int64_t now);

private:
	std::string _serverId;
	VerifySignature _verify;
	std::vector<join_ticket::TicketKey> _keys;
	/// The ids of the accepted tickets, and the time when each ticket expires
	std::map<std::string, int64_t, std::less<>> _used;
};

[[nodiscard]] std::string_view ProblemName(TicketProblem problem) noexcept;

} // namespace openblack::server
