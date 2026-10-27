/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <JoinTicket.h>
#include <gtest/gtest.h>

#include "Server/Ed25519.h"
#include "Server/TicketCheck.h"

using namespace openblack;
using namespace openblack::join_ticket;
using openblack::server::TicketCheck;
using openblack::server::TicketProblem;

namespace
{

/// The public test vectors of PASETO v4.public (paseto-standard test-vectors 4-S-1 and 4-S-2)
constexpr std::string_view k_VectorPrefix =
    "v4.public.eyJkYXRhIjoidGhpcyBpcyBhIHNpZ25lZCBtZXNzYWdlIiwiZXhwIjoiMjAyMi0wMS0wMVQwMDowMDowMCswMDowMCJ9";
constexpr std::string_view k_Vector1Signature =
    "bg_XBBzds8lTZShVlwwKSgeKpLT3yukTw6JUz3W4h_ExsQV-P0V54zemZDcAxFaSeef1QlXEFtkqxT1ciiQEDA";
constexpr std::string_view k_Vector2Signature =
    "v3Jt8mx_TdM2ceTGoqwrh4yDFn0XsHvvV_D0DtwQxVrJEBMl0F2caAdgnpKlt4p7xBnx1HcO-SPo8FPp214HDw";
constexpr std::string_view k_Vector2Footer = ".eyJraWQiOiJ6VmhNaVBCUDlmUmYyc25FY1Q3Z0ZUaW9lQTlDT2NOeTlEZmdMMVc2MGhhTiJ9";
constexpr std::string_view k_VectorMessage = R"({"data":"this is a signed message","exp":"2022-01-01T00:00:00+00:00"})";

std::vector<uint8_t> FromHex(std::string_view hex)
{
	std::vector<uint8_t> out;
	for (size_t i = 0; i + 1 < hex.size(); i += 2)
	{
		out.push_back(static_cast<uint8_t>(std::stoul(std::string(hex.substr(i, 2)), nullptr, 16)));
	}
	return out;
}

PublicKey VectorKey()
{
	PublicKey key {};
	const auto bytes = FromHex("1eb9dbbbbc047c03fd70604e0071f0987e16b28b757225c11f00415d0e20b1a2");
	std::copy(bytes.begin(), bytes.end(), key.begin());
	return key;
}

ed25519::Seed VectorSeed()
{
	ed25519::Seed seed {};
	const auto bytes = FromHex("b4cbfb43df4ce210727d953e4a713307fa19bb7d9f85041438d9e11b942a3774");
	std::copy(bytes.begin(), bytes.end(), seed.begin());
	return seed;
}

/// A test key of the test, never a key of a lobby
ed25519::Seed TestSeed()
{
	ed25519::Seed seed {};
	for (size_t i = 0; i < seed.size(); ++i)
	{
		seed[i] = static_cast<uint8_t>(i * 7 + 3);
	}
	return seed;
}

constexpr std::string_view k_KeyId = "k4.pid.test-key";
constexpr int64_t k_Now = 1'792'756'800; // 2026-10-23T12:00:00Z

std::string ClaimsText(std::string_view audience = "game-1", std::string_view role = "god", std::string_view jti = "ticket-1",
                       std::string_view issuer = "bwgame-service")
{
	return std::string(R"({"iss":")") + std::string(issuer) + R"(","sub":"42","aud":")" + std::string(audience) +
	       R"(","iat":"2026-10-23T12:00:00+00:00","nbf":"2026-10-23T12:00:00+00:00","exp":"2026-10-23T12:01:00+00:00","jti":")" +
	       std::string(jti) + R"(","name":"Alice","clans":[],"admin":false,"role":")" + std::string(role) + R"("})";
}

std::string SignedTicket(std::string_view claims, const ed25519::Seed& seed = TestSeed(), std::string_view keyId = k_KeyId)
{
	const auto footer = std::string(R"({"kid":")") + std::string(keyId) + R"("})";
	const auto signature = ed25519::Sign(seed, SignedBytesOf(claims, footer));
	EXPECT_TRUE(signature.has_value());
	return MakeToken(claims, signature.value_or(Signature {}), footer);
}

TicketCheck MakeCheck()
{
	TicketCheck check("game-1", ed25519::Verify);
	check.SetKeys({TicketKey {.id = std::string(k_KeyId), .publicKey = *ed25519::PublicKeyOf(TestSeed())}});
	return check;
}

} // namespace

TEST(JoinTicket, Base64UrlIsCanonicalAndUnpadded)
{
	const std::vector<uint8_t> bytes {0xFB, 0xFF, 0x00, 0x10};
	EXPECT_EQ(EncodeBase64Url(bytes), "-_8AEA");
	EXPECT_EQ(DecodeBase64Url("-_8AEA"), bytes);
	EXPECT_FALSE(DecodeBase64Url("-_8AEA==").has_value());
	EXPECT_FALSE(DecodeBase64Url("+/8AEA").has_value());
	// Bits after the last whole byte must be clear
	EXPECT_FALSE(DecodeBase64Url("-_8AEB").has_value());
	EXPECT_FALSE(DecodeBase64Url("A").has_value());
}

TEST(JoinTicket, ThePreAuthenticationEncodingIsTheSpecificationsOne)
{
	// PAE(["test"]) from the PASETO specification
	const std::string_view test = "test";
	const std::array pieces {std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(test.data()), test.size())};
	const auto encoded = PreAuthenticationEncoding(pieces);
	const std::vector<uint8_t> expected {1, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 't', 'e', 's', 't'};
	EXPECT_EQ(encoded, expected);
	EXPECT_EQ(PreAuthenticationEncoding({}), (std::vector<uint8_t> {0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST(JoinTicket, TheTestVectorsOfTheSpecificationVerify)
{
	const auto token1 = ReadToken(std::string(k_VectorPrefix) + std::string(k_Vector1Signature));
	ASSERT_TRUE(token1.has_value());
	EXPECT_EQ(token1->message, k_VectorMessage);
	EXPECT_TRUE(token1->footer.empty());
	EXPECT_TRUE(ed25519::Verify(VectorKey(), token1->signedBytes, token1->signature));

	const auto token2 = ReadToken(std::string(k_VectorPrefix) + std::string(k_Vector2Signature) + std::string(k_Vector2Footer));
	ASSERT_TRUE(token2.has_value());
	EXPECT_EQ(token2->footer, R"({"kid":"zVhMiPBP9fRf2snEcT7gFTioeA9COcNy9DfgL1W60haN"})");
	EXPECT_EQ(KeyIdOf(token2->footer), "zVhMiPBP9fRf2snEcT7gFTioeA9COcNy9DfgL1W60haN");
	EXPECT_TRUE(ed25519::Verify(VectorKey(), token2->signedBytes, token2->signature));

	// The signer gives the same token, and its public key is the vector's
	const auto signature = ed25519::Sign(VectorSeed(), SignedBytesOf(k_VectorMessage, {}));
	ASSERT_TRUE(signature.has_value());
	EXPECT_EQ(MakeToken(k_VectorMessage, *signature, {}), std::string(k_VectorPrefix) + std::string(k_Vector1Signature));
	EXPECT_EQ(ed25519::PublicKeyOf(VectorSeed()), VectorKey());

	// One changed byte of the message breaks the signature
	auto changed = *token1;
	changed.signedBytes.back() ^= 1;
	EXPECT_FALSE(ed25519::Verify(VectorKey(), changed.signedBytes, changed.signature));
}

TEST(JoinTicket, OnlyAV4PublicTokenReads)
{
	EXPECT_FALSE(ReadToken("v4.local.abc").has_value());
	EXPECT_FALSE(ReadToken("v4.public.").has_value());
	EXPECT_FALSE(ReadToken("v4.public.AAAA").has_value());
	EXPECT_FALSE(ReadToken(std::string(k_VectorPrefix) + std::string(k_Vector1Signature) + ".").has_value());
	EXPECT_FALSE(ReadToken(std::string(k_VectorPrefix) + std::string(k_Vector1Signature) + ".e30.e30").has_value());
}

TEST(JoinTicket, TheKeysOfTheLobbyRead)
{
	const auto keys = ParseTicketKeys(
	    R"({"keys":[{"kid":"k4.pid.a","public_key":"k4.public.Hrnbu7wEfAP9cGBOAHHwmH4Wsot1ciXBHwBBXQ4gsaI","use":"current"},)"
	    R"({"kid":"k4.pid.b","public_key":"k4.secret.AAAA","use":"next"}]})");
	ASSERT_TRUE(keys.has_value());
	ASSERT_EQ(keys->size(), 1u);
	EXPECT_EQ(keys->front().id, "k4.pid.a");
	EXPECT_EQ(keys->front().publicKey, VectorKey());
	EXPECT_FALSE(ParseTicketKeys(R"({"nokeys":[]})").has_value());
}

TEST(JoinTicket, TheClaimsAndTheTimesRead)
{
	const auto claims = ParseClaims(ClaimsText("game-1", "observer"));
	ASSERT_TRUE(claims.has_value());
	EXPECT_EQ(claims->subject, "42");
	EXPECT_EQ(claims->name, "Alice");
	EXPECT_EQ(claims->role, game_socket::Role::Observer);
	EXPECT_EQ(claims->expires - claims->issuedAt, 60);
	EXPECT_EQ(ParseIsoTime("1970-01-01T00:00:00+00:00"), 0);
	EXPECT_EQ(ParseIsoTime("2026-10-23T12:00:00Z"), k_Now);
	EXPECT_EQ(ParseIsoTime("2026-10-23T14:00:00+02:00"), k_Now);
	EXPECT_EQ(ParseIsoTime("2026-10-23T12:00:00.25+00:00"), k_Now);
	EXPECT_FALSE(ParseIsoTime("2026-10-23").has_value());
	EXPECT_FALSE(ParseIsoTime("2026-13-23T12:00:00Z").has_value());
	EXPECT_FALSE(ParseClaims(ClaimsText("game-1", "king")).has_value());
}

TEST(TicketCheck, AGoodTicketWorksOnce)
{
	auto check = MakeCheck();
	const auto ticket = SignedTicket(ClaimsText());
	const auto result = check.Check(ticket, k_Now + 10);
	ASSERT_EQ(result.problem, TicketProblem::None);
	EXPECT_EQ(result.claims->name, "Alice");
	EXPECT_EQ(result.claims->role, game_socket::Role::God);
	EXPECT_EQ(check.Check(ticket, k_Now + 11).problem, TicketProblem::AlreadyUsed);
}

TEST(TicketCheck, TheCheckRefusesEachBadTicket)
{
	auto check = MakeCheck();
	EXPECT_EQ(check.Check("not a token", k_Now).problem, TicketProblem::Format);
	EXPECT_EQ(check.Check(SignedTicket(ClaimsText(), TestSeed(), "k4.pid.other"), k_Now).problem, TicketProblem::UnknownKey);
	EXPECT_EQ(check.Check(SignedTicket(ClaimsText(), VectorSeed()), k_Now).problem, TicketProblem::Signature);
	EXPECT_EQ(check.Check(SignedTicket(ClaimsText("game-2", "god", "t2")), k_Now).problem, TicketProblem::Audience);
	EXPECT_EQ(check.Check(SignedTicket(ClaimsText("game-1", "god", "t3", "other")), k_Now).problem, TicketProblem::Issuer);
	// 60 seconds of life, and 5 seconds for the difference of the clocks
	EXPECT_EQ(check.Check(SignedTicket(ClaimsText("game-1", "god", "t4")), k_Now + 65).problem, TicketProblem::Expired);
	EXPECT_EQ(check.Check(SignedTicket(ClaimsText("game-1", "god", "t5")), k_Now + 64).problem, TicketProblem::None);
	EXPECT_EQ(check.Check(SignedTicket(ClaimsText("game-1", "god", "t6")), k_Now - 6).problem, TicketProblem::NotYetValid);
	EXPECT_EQ(check.Check(SignedTicket(ClaimsText("game-1", "god", "t7")), k_Now - 5).problem, TicketProblem::None);
}
