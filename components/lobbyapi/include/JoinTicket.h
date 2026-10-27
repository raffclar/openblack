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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "GameSocket.h"

/// The join tickets of the lobby: PASETO v4.public tokens that the lobby signs with Ed25519, and the keys that a game
/// server checks them with. Here are only the parts of the format: the parts of a token, the bytes that the signature
/// covers, the claims and the list of keys. The check of the signature is the work of the caller. Nothing here throws.
///
/// A token is "v4.public." + base64url(message + signature), then "." + base64url(footer) when it has a footer. The
/// signature covers the pre-authentication encoding (PAE) of the header, the message, the footer and the implicit
/// assertion. The footer names the key: {"kid": "k4.pid...."}.
namespace openblack::join_ticket
{

inline constexpr size_t k_PublicKeyBytes = 32;
inline constexpr size_t k_SignatureBytes = 64;

using PublicKey = std::array<uint8_t, k_PublicKeyBytes>;
using Signature = std::array<uint8_t, k_SignatureBytes>;

/// The bytes of unpadded base64url text. None for padding, another alphabet or bits that a canonical text does not have.
[[nodiscard]] std::optional<std::vector<uint8_t>> DecodeBase64Url(std::string_view text);
/// Unpadded base64url text of the bytes
[[nodiscard]] std::string EncodeBase64Url(std::span<const uint8_t> bytes);
/// The pre-authentication encoding: the count of the pieces, then each piece after its length, as little-endian 64-bit
/// numbers with the top bit clear
[[nodiscard]] std::vector<uint8_t> PreAuthenticationEncoding(std::span<const std::span<const uint8_t>> pieces);

/// The parts of a v4.public token
struct SignedToken
{
	std::string message;
	Signature signature {};
	std::string footer;
	/// The bytes that the signature covers
	std::vector<uint8_t> signedBytes;
};

/// The parts of a v4.public token. None when the text is not such a token.
[[nodiscard]] std::optional<SignedToken> ReadToken(std::string_view token, std::string_view implicit = {});
/// The token of a message, its signature and its footer, as a signer makes it. Tests and tools use it.
[[nodiscard]] std::string MakeToken(std::string_view message, const Signature& signature, std::string_view footer);
/// The bytes that the signature of a token covers
[[nodiscard]] std::vector<uint8_t> SignedBytesOf(std::string_view message, std::string_view footer,
                                                 std::string_view implicit = {});

/// The id of the key that the footer names, "k4.pid...."
[[nodiscard]] std::optional<std::string> KeyIdOf(std::string_view footer);

/// A key that signs tickets: GET /api/v1/ticket-keys gives {"keys": [{"kid", "public_key", "use"}]}
struct TicketKey
{
	/// "k4.pid...."
	std::string id;
	PublicKey publicKey {};
};

/// The keys of a ticket-keys answer. A key whose text is not a "k4.public." key is left out. None when the text is not
/// such an answer.
[[nodiscard]] std::optional<std::vector<TicketKey>> ParseTicketKeys(std::string_view text);
/// The public key of a "k4.public." text
[[nodiscard]] std::optional<PublicKey> ParsePublicKey(std::string_view text);

/// The claims of a join ticket
struct Claims
{
	std::string issuer;
	/// The account id
	std::string subject;
	/// The id of the game server that the ticket is for
	std::string audience;
	/// The times of the ticket, in seconds since 1970 (UTC)
	int64_t issuedAt {0};
	int64_t notBefore {0};
	int64_t expires {0};
	/// The random id of the ticket. A ticket works once.
	std::string id;
	/// The name of the account
	std::string name;
	/// The account has the admin right on this server
	bool admin {false};
	game_socket::Role role {game_socket::Role::God};
};

/// The claims of a ticket's message. None when a claim is missing, has the wrong type or names a role that is not known.
[[nodiscard]] std::optional<Claims> ParseClaims(std::string_view message);

/// The seconds since 1970 (UTC) of an ISO 8601 time with seconds and an offset, such as "2026-10-23T12:00:00+00:00"
[[nodiscard]] std::optional<int64_t> ParseIsoTime(std::string_view text);

} // namespace openblack::join_ticket
