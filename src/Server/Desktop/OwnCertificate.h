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

#include <optional>
#include <string>
#include <string_view>

#include "Online/ServerKeyPin.h"

/// The certificate that a game server on the local network makes for itself at its start (deviation 113, LAN-1 b). No
/// authority signs it. The answer to the search gives the pin of its public key, so a client accepts only this server.
namespace openblack::server::own_certificate
{

/// The time before the start that the certificate is valid from, for a client clock that is a little behind
constexpr int64_t k_ValidBeforeSeconds = 60 * 60;
/// The time after the start that the certificate is valid to. A server makes a new certificate at each start.
constexpr int64_t k_ValidAfterSeconds = 30 * 24 * 60 * 60;

/// A new key and its certificate, as PEM text, and the pin of the public key
struct OwnCertificate
{
	std::string certificatePem;
	std::string keyPem;
	online::server_key::Pin pin {};
};

/// Makes a new ECDSA key on the P-256 curve and a certificate for it that the key signs, with the name as its common
/// name. None when OpenSSL cannot make them.
[[nodiscard]] std::optional<OwnCertificate> Make(std::string_view name);

/// The pin of the public key of a certificate in PEM text. None when the text holds no certificate.
[[nodiscard]] std::optional<online::server_key::Pin> PinOfCertificate(std::string_view certificatePem);

} // namespace openblack::server::own_certificate
