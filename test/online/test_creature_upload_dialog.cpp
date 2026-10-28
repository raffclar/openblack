/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <LobbyApi.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Online/CreatureUploadDialog.h"
#include "Online/HttpClientInterface.h"

using namespace openblack;
using namespace openblack::creature_upload;
using namespace openblack::creature_upload::dialog;
using online::HttpResponse;

namespace
{

class FakeHttp final: public online::HttpClientInterface
{
public:
	struct Sent
	{
		online::HttpRequestId id {0};
		online::HttpRequest request;
		online::HttpCallback callback;
	};

	online::HttpRequestId Send(online::HttpRequest request, online::HttpCallback callback) override
	{
		const auto id = _nextId++;
		sent.push_back({id, std::move(request), std::move(callback)});
		return id;
	}
	void Cancel(online::HttpRequestId id) override { cancelled.push_back(id); }
	void Poll() override {}
	[[nodiscard]] size_t Pending() const override { return sent.size(); }
	[[nodiscard]] std::optional<uint64_t> SentBytes(online::HttpRequestId /*id*/) const override { return sentBytes; }

	void Answer(int32_t status, std::string_view body)
	{
		ASSERT_FALSE(sent.empty());
		auto next = std::move(sent.front());
		sent.pop_front();
		next.callback(HttpResponse {.status = status, .body = {body.begin(), body.end()}});
	}
	void Fail(online::HttpError error)
	{
		ASSERT_FALSE(sent.empty());
		auto next = std::move(sent.front());
		sent.pop_front();
		next.callback(HttpResponse {.error = error});
	}
	[[nodiscard]] const online::HttpRequest& Next() const { return sent.front().request; }
	[[nodiscard]] std::string NextBody() const { return {Next().body.begin(), Next().body.end()}; }
	[[nodiscard]] std::optional<std::string> NextHeader(std::string_view name) const
	{
		for (const auto& [key, value] : Next().headers)
		{
			if (key == name)
			{
				return value;
			}
		}
		return std::nullopt;
	}

	std::deque<Sent> sent;
	std::vector<online::HttpRequestId> cancelled;
	std::optional<uint64_t> sentBytes;

private:
	online::HttpRequestId _nextId {1};
};

constexpr std::string_view k_Session =
    R"({"token_type":"Bearer","access_token":"access1","access_token_expires":"2026-10-22T10:15:00.000Z",)"
    R"("refresh_token":"refresh1","refresh_token_expires":"2026-11-21T10:00:00.000Z","account":{"id":7,"name":"abc"}})";
constexpr std::string_view k_Uploaded =
    R"({"id":3,"name":"Rex","url":"https://bw.example/creature/abc/","uploaded":"2026-10-24T10:00:00Z"})";

/// An effect as a line of text, so that a test can compare the list of effects
std::string Describe(const Effect& effect)
{
	if (const auto* status = std::get_if<SetStatus>(&effect))
	{
		return fmt::format("status {}", status->text);
	}
	if (const auto* range = std::get_if<SetProgressRange>(&effect))
	{
		return fmt::format("range {}", range->most);
	}
	if (const auto* position = std::get_if<SetProgress>(&effect))
	{
		return fmt::format("position {}", position->position);
	}
	if (const auto* enable = std::get_if<EnableUpload>(&effect))
	{
		return fmt::format("upload {}", enable->enabled ? "on" : "off");
	}
	const auto& message = std::get<ShowMessage>(effect);
	return fmt::format("message [{}] {}{}", message.caption, message.text,
	                   message.icon == MessageIcon::Error ? " (error)" : "");
}

lobby_api::CreatureUpload FakeFiles()
{
	return lobby_api::CreatureUpload {
	    .package = {'L', 'H', 'N', 'I', 'L', 'D'},
	    .mind = {1, 2, 3},
	    .mindFileName = "_A_b.chl",
	    .physique = {4, 5},
	    .physiqueFileName = "_A_b.phy",
	};
}

class CreatureUploadDialogTest: public ::testing::Test
{
protected:
	void SetUp() override { _flow = std::make_unique<UploadFlow>(_http, "https://bw.example", Texts {}, 1); }

	std::vector<std::string> Effects()
	{
		std::vector<std::string> lines;
		for (const auto& effect : _flow->TakeEffects())
		{
			lines.push_back(Describe(effect));
		}
		return lines;
	}

	void StartAndLogIn(GatherFiles gather = []() { return FakeFiles(); })
	{
		_flow->Upload("abc", "secret", std::move(gather));
		_http.Answer(201, k_Session);
	}

	FakeHttp _http;
	std::unique_ptr<UploadFlow> _flow;
};

} // namespace

TEST(CreatureUploadDialogTexts, TheGameTextsAreTheDefaults)
{
	const Texts english;
	EXPECT_EQ(english.Get(keys::k_AppTitle), "Creature Upload Utility");
	EXPECT_EQ(english.Get(keys::k_ErrorNoPassword), "Please enter password!");
	EXPECT_EQ(english.Get("NO_SUCH_KEY"), "");
	const Texts fromFile(creatureuploadcfg::Texts {{"IDC_APPTITLE", "Kreatur"}});
	EXPECT_EQ(fromFile.Get(keys::k_AppTitle), "Kreatur");
	EXPECT_EQ(fromFile.Get(keys::k_ErrorTitle), "Error");
}

TEST(CreatureUploadDialogTexts, ThePageAddressReplacesTheAddressOfTheGameSite)
{
	EXPECT_EQ(PageMessage("Your creaturefile has been sent to the server, look at: http://www.bwcreatures.com/%s",
	                      "https://bw.example/creature/abc/"),
	          "Your creaturefile has been sent to the server, look at: https://bw.example/creature/abc/");
	EXPECT_EQ(PageMessage("Sent: http://site/%s now", "https://p/"), "Sent: https://p/ now");
	EXPECT_EQ(PageMessage("See %s", "https://p/"), "See https://p/");
	EXPECT_EQ(Format("Sending to %s.", "host"), "Sending to host.");
	EXPECT_EQ(Format("No value", "host"), "No value");
}

TEST(CreatureUploadDialogTexts, TheHostHasNoSchemeOrPort)
{
	EXPECT_EQ(HostOf("https://bw.example"), "bw.example");
	EXPECT_EQ(HostOf("HTTPS://localhost:8443"), "localhost");
	EXPECT_EQ(HostOf("https://bw.example/path"), "bw.example");
}

TEST(CreatureUploadDialogTexts, TheProfileGivesThePathsOfItsFiles)
{
	const player_profiles::Store store {.folder = "profiles", .creatureMinds = "profiles/Scripts/CreatureMind"};
	player_profiles::Profile profile {.name = "Ab", .creatureFile = "_x.chl"};
	const auto paths = PathsFor(store, profile);
	ASSERT_TRUE(paths.has_value());
	EXPECT_EQ(paths->package, std::filesystem::path("profiles") / player_profiles::ProfileKey(u"Ab") / "creature.lhp");
	EXPECT_EQ(paths->mind, std::filesystem::path("profiles/Scripts/CreatureMind") / "_x.chl");
	EXPECT_EQ(paths->physique.parent_path(), std::filesystem::path("profiles/Scripts/CreatureMind"));
	profile.creatureFile.clear();
	EXPECT_FALSE(PathsFor(store, profile).has_value());
	EXPECT_FALSE(PathsFor(player_profiles::Store {}, player_profiles::Profile {.creatureFile = "_x.chl"}).has_value());
}

TEST_F(CreatureUploadDialogTest, AnEmptyLoginNameShowsTheProfileError)
{
	_flow->Upload("", "secret", {});
	EXPECT_EQ(Effects(), (std::vector<std::string> {
	                         "position 0",
	                         "range 20",
	                         "message [Error] No Userprofile found. You need to register online to use this service. (error)",
	                     }));
	EXPECT_TRUE(_http.sent.empty());
	EXPECT_FALSE(_flow->IsBusy());
}

TEST_F(CreatureUploadDialogTest, AnEmptyPasswordShowsThePasswordError)
{
	_flow->Upload("abc", "", {});
	EXPECT_EQ(Effects(), (std::vector<std::string> {
	                         "position 0",
	                         "range 20",
	                         "message [Error] Please enter password! (error)",
	                     }));
	EXPECT_TRUE(_http.sent.empty());
}

TEST_F(CreatureUploadDialogTest, ASuccessfulUploadShowsThePageAndSignsOut)
{
	_flow->Upload("abc", "secret", []() { return FakeFiles(); });
	EXPECT_EQ(Effects(), (std::vector<std::string> {
	                         "position 0",
	                         "range 20",
	                         "position 2",
	                         "status Validating useraccount, may take a while...",
	                     }));
	ASSERT_EQ(_http.sent.size(), 1);
	EXPECT_EQ(_http.Next().method, online::HttpMethod::Post);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions");
	EXPECT_NE(_http.NextBody().find(R"("name":"abc")"), std::string::npos);
	EXPECT_NE(_http.NextBody().find(R"("password":"secret")"), std::string::npos);
	EXPECT_TRUE(_flow->IsBusy());

	_http.Answer(201, k_Session);
	ASSERT_EQ(_http.sent.size(), 1);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/me/creatures");
	EXPECT_EQ(_http.NextHeader("Authorization"), "Bearer access1");
	const auto body = _http.NextBody();
	EXPECT_NE(body.find(R"(name="lhp"; filename="creature.lhp")"), std::string::npos);
	EXPECT_NE(body.find(R"(name="mind"; filename="_A_b.chl")"), std::string::npos);
	EXPECT_NE(body.find(R"(name="physique"; filename="_A_b.phy")"), std::string::npos);
	const auto most = body.size() * 2;
	EXPECT_EQ(Effects(), (std::vector<std::string> {
	                         "upload off",
	                         "position 20",
	                         "status Opening Creature file",
	                         "position 2",
	                         "status Sending creature data to server: bw.example",
	                         "position 0",
	                         fmt::format("range {}", most),
	                     }));
	EXPECT_TRUE(_flow->IsSending());

	_http.sentBytes = 100;
	_flow->Tick();
	EXPECT_EQ(Effects(), (std::vector<std::string> {"position 100"}));
	_flow->Tick();
	EXPECT_TRUE(Effects().empty());

	_http.Answer(201, k_Uploaded);
	EXPECT_EQ(Effects(), (std::vector<std::string> {
	                         fmt::format("position {}", most),
	                         "message [Creature Upload Utility] Your creaturefile has been sent to the server, look at: "
	                         "https://bw.example/creature/abc/",
	                         "status Creaturedata was sent to the server",
	                         "position 0",
	                         "upload on",
	                     }));
	EXPECT_FALSE(_flow->IsBusy());
	ASSERT_EQ(_http.sent.size(), 1);
	EXPECT_EQ(_http.Next().method, online::HttpMethod::Delete);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions/current");
	EXPECT_EQ(_http.NextHeader("Authorization"), "Bearer access1");
}

TEST_F(CreatureUploadDialogTest, ARefusedLoginShowsTheValidationError)
{
	_flow->Upload("abc", "wrong", []() { return FakeFiles(); });
	(void)Effects();
	_http.Answer(401, R"({"status":401,"code":"bad_credentials"})");
	EXPECT_EQ(Effects(), (std::vector<std::string> {
	                         "message [Creature Upload Utility] Error:\nUser could not be validated. Upload aborted.",
	                         "position 0",
	                     }));
	EXPECT_FALSE(_flow->IsBusy());
	EXPECT_TRUE(_http.sent.empty());
}

TEST_F(CreatureUploadDialogTest, LoginFailuresWithNoAnswerShowTheConnectionErrors)
{
	_flow->Upload("abc", "secret", {});
	(void)Effects();
	_http.Fail(online::HttpError::Network);
	EXPECT_EQ(Effects().front(), "message [Creature Upload Utility] Error:\nCould not connect to the server!");

	_flow->Upload("abc", "secret", {});
	(void)Effects();
	_http.Fail(online::HttpError::Tls);
	EXPECT_EQ(Effects().front(), "message [Creature Upload Utility] Error:\nCould not connect to the server!");

	_flow->Upload("abc", "secret", {});
	(void)Effects();
	_http.Fail(online::HttpError::Timeout);
	EXPECT_EQ(Effects().front(), "message [Creature Upload Utility] Error:\nConnection timed out!");

	_flow->Upload("abc", "secret", {});
	(void)Effects();
	_http.Answer(503, "");
	EXPECT_EQ(Effects().front(), "message [Creature Upload Utility] Error:\nCould not retrieve data from server!");

	_flow->Upload("abc", "secret", {});
	(void)Effects();
	_http.Answer(201, "not json");
	EXPECT_EQ(Effects().front(), "message [Creature Upload Utility] Error:\nCould not retrieve data from server!");
}

TEST_F(CreatureUploadDialogTest, MissingFilesShowTheFileErrorAndKeepTheButtonOff)
{
	StartAndLogIn([]() { return Problem::NoPackage; });
	EXPECT_EQ(Effects(), (std::vector<std::string> {
	                         "position 0",
	                         "range 20",
	                         "position 2",
	                         "status Validating useraccount, may take a while...",
	                         "upload off",
	                         "position 20",
	                         "status Opening Creature file",
	                         "message [Creature Upload Utility] Error:\nCould not open creature file!",
	                         "position 0",
	                     }));
	EXPECT_FALSE(_flow->IsBusy());
	ASSERT_EQ(_http.sent.size(), 1);
	EXPECT_EQ(_http.Next().url, "https://bw.example/api/v1/sessions/current");
}

TEST_F(CreatureUploadDialogTest, ARefusedUploadShowsTheValidationErrorAndTurnsTheButtonOn)
{
	StartAndLogIn();
	(void)Effects();
	_http.Answer(422, R"({"status":422,"code":"bad_upload"})");
	const auto effects = Effects();
	ASSERT_EQ(effects.size(), 4);
	EXPECT_EQ(effects[1], "message [Creature Upload Utility] Error:\nUser could not be validated. Upload aborted.");
	EXPECT_EQ(effects[2], "position 0");
	EXPECT_EQ(effects[3], "upload on");
	ASSERT_EQ(_http.sent.size(), 1);
	EXPECT_EQ(_http.Next().method, online::HttpMethod::Delete);
}

TEST_F(CreatureUploadDialogTest, UploadFailuresWithNoAnswerShowTheSendErrors)
{
	StartAndLogIn();
	(void)Effects();
	_http.Fail(online::HttpError::Network);
	EXPECT_EQ(Effects()[1], "message [Creature Upload Utility] Error:\nCould not send data to server!");
	_http.sent.clear();

	StartAndLogIn();
	(void)Effects();
	_http.Fail(online::HttpError::Timeout);
	EXPECT_EQ(Effects()[1], "message [Creature Upload Utility] Error:\nConnection timed out!");
	_http.sent.clear();

	StartAndLogIn();
	(void)Effects();
	_http.Answer(201, "{}");
	EXPECT_EQ(Effects()[1], "message [Creature Upload Utility] Error:\nCould not retrieve data from server!");
}

TEST_F(CreatureUploadDialogTest, AClickWhileBusyDoesNothing)
{
	_flow->Upload("abc", "secret", {});
	(void)Effects();
	_flow->Upload("abc", "secret", {});
	EXPECT_TRUE(Effects().empty());
	EXPECT_EQ(_http.sent.size(), 1);
}

TEST_F(CreatureUploadDialogTest, TheNameAndThePasswordStopAt32Characters)
{
	const std::string longText(40, 'a');
	_flow->Upload(longText, longText + "\xC3\xA9", {});
	const auto body = _http.NextBody();
	EXPECT_NE(body.find(fmt::format(R"("name":"{}")", std::string(32, 'a'))), std::string::npos);
	EXPECT_NE(body.find(fmt::format(R"("password":"{}")", std::string(32, 'a'))), std::string::npos);
}

TEST_F(CreatureUploadDialogTest, CloseStopsTheRequestAndSignsOut)
{
	StartAndLogIn();
	ASSERT_EQ(_http.sent.size(), 1);
	const auto upload = _http.sent.front().id;
	_flow->Close();
	EXPECT_EQ(_http.cancelled, std::vector<online::HttpRequestId> {upload});
	EXPECT_FALSE(_flow->IsBusy());
	ASSERT_EQ(_http.sent.size(), 2);
	EXPECT_EQ(_http.sent.back().request.url, "https://bw.example/api/v1/sessions/current");
	// The callback of the stopped request does nothing after the flow ends
	_flow.reset();
	_http.Fail(online::HttpError::Cancelled);
	EXPECT_EQ(_http.sent.size(), 1);
}
