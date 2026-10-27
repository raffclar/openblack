/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TicketCheck.h"

#include <algorithm>
#include <utility>

namespace openblack::server
{

TicketCheck::TicketCheck(std::string serverId, VerifySignature verify)
    : _serverId(std::move(serverId))
    , _verify(std::move(verify))
{
}

void TicketCheck::SetKeys(std::vector<join_ticket::TicketKey> keys)
{
	_keys = std::move(keys);
}

TicketCheck::Result TicketCheck::Check(std::string_view token, int64_t now)
{
	const auto parts = join_ticket::ReadToken(token);
	if (!parts.has_value())
	{
		return {.problem = TicketProblem::Format, .claims = std::nullopt};
	}
	const auto keyId = join_ticket::KeyIdOf(parts->footer);
	if (!keyId.has_value())
	{
		return {.problem = TicketProblem::Format, .claims = std::nullopt};
	}
	const auto key = std::ranges::find(_keys, *keyId, &join_ticket::TicketKey::id);
	if (key == _keys.end())
	{
		return {.problem = TicketProblem::UnknownKey, .claims = std::nullopt};
	}
	if (!_verify || !_verify(key->publicKey, parts->signedBytes, parts->signature))
	{
		return {.problem = TicketProblem::Signature, .claims = std::nullopt};
	}
	auto claims = join_ticket::ParseClaims(parts->message);
	if (!claims.has_value())
	{
		return {.problem = TicketProblem::Format, .claims = std::nullopt};
	}
	if (claims->issuer != k_Issuer)
	{
		return {.problem = TicketProblem::Issuer, .claims = std::nullopt};
	}
	if (claims->audience != _serverId)
	{
		return {.problem = TicketProblem::Audience, .claims = std::nullopt};
	}
	if (now >= claims->expires + k_ClockSkewSeconds)
	{
		return {.problem = TicketProblem::Expired, .claims = std::nullopt};
	}
	if (now + k_ClockSkewSeconds < claims->notBefore)
	{
		return {.problem = TicketProblem::NotYetValid, .claims = std::nullopt};
	}
	// The ids of tickets that expired are no longer kept
	std::erase_if(_used, [now](const auto& entry) { return entry.second + k_ClockSkewSeconds < now; });
	if (!_used.emplace(claims->id, claims->expires).second)
	{
		return {.problem = TicketProblem::AlreadyUsed, .claims = std::nullopt};
	}
	return {.problem = TicketProblem::None, .claims = std::move(claims)};
}

std::string_view ProblemName(TicketProblem problem) noexcept
{
	switch (problem)
	{
	case TicketProblem::None:
		return "none";
	case TicketProblem::Format:
		return "format";
	case TicketProblem::UnknownKey:
		return "unknown key";
	case TicketProblem::Signature:
		return "signature";
	case TicketProblem::Issuer:
		return "issuer";
	case TicketProblem::Audience:
		return "audience";
	case TicketProblem::Expired:
		return "expired";
	case TicketProblem::NotYetValid:
		return "not valid yet";
	case TicketProblem::AlreadyUsed:
		return "already used";
	}
	return "unknown";
}

} // namespace openblack::server
