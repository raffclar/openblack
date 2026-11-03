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
#include <string>
#include <string_view>

/// The key of a game server on the local network (deviation 113, LAN-1 b). The server makes a certificate of its own at
/// its start. Its answer to the search gives the SHA-256 of the public key of that certificate. The client then accepts
/// only a server with that public key at that address.
namespace openblack::online::server_key
{

/// The SHA-256 of the public key of a certificate, in the DER form of the public key information
using Pin = std::array<uint8_t, 32>;

/// The count of hex digits of a pin
constexpr size_t k_PinHexDigits = 64;

/// The pin of 64 hex digits, in lower case or upper case. None for other text.
[[nodiscard]] std::optional<Pin> ParseHex(std::string_view text);

/// The pin as 64 hex digits in lower case
[[nodiscard]] std::string HexOf(const Pin& pin);

/// The pin in the form of the pinned public key of libcurl: "sha256//" and the pin in base64
[[nodiscard]] std::string CurlPinOf(const Pin& pin);

} // namespace openblack::online::server_key
