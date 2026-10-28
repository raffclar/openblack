/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureUploadDialog.h"

#include <cctype>

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include <LobbyApiJson.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureCarryOver.h"
#include "Creature/CreatureWebPage.h"
#include "Gui/TextDatabase.h"

namespace openblack::creature_upload::dialog
{
namespace
{

using online::HttpError;
using online::HttpMethod;
using online::HttpResponse;

constexpr std::string_view k_JsonType = "application/json";
constexpr std::string_view k_BinaryType = "application/octet-stream";

/// The English texts of the language file of the game
constexpr std::array<std::pair<std::string_view, std::string_view>, 24> k_EnglishTexts {{
    {keys::k_AppTitle, "Creature Upload Utility"},
    {keys::k_ErrorBox, "Error:"},
    {keys::k_Validating, "Validating useraccount, may take a while..."},
    {keys::k_OpenFile, "Opening Creature file"},
    {keys::k_Sending, "Sending creature data to server: %s"},
    {keys::k_SendOk, "Your creaturefile has been sent to the server, look at: http://www.bwcreatures.com/%s"},
    {keys::k_SentToServer, "Creaturedata was sent to the server"},
    {keys::k_GroupBox, "User validation"},
    {keys::k_Profile, "Profile:"},
    {keys::k_LoginName, "Username:"},
    {keys::k_LoginPassword, "Userpassword:"},
    {keys::k_DoUpload, "&Upload Creature"},
    {keys::k_Cancel, "&Exit"},
    {keys::k_ErrorConnect, "Could not connect to the server!"},
    {keys::k_ErrorSendingData, "Could not send data to server!"},
    {keys::k_ErrorRetrievingData, "Could not retrieve data from server!"},
    {keys::k_ErrorOpenCreatureFile, "Could not open creature file!"},
    {keys::k_ErrorTimeout, "Connection timed out!"},
    {keys::k_ErrorValidation, "User could not be validated. Upload aborted."},
    {"ERROR_USER_NOT_FOUND", "User not found!"},
    {"ERROR_USER_WRONG_PASSWORD", "Userpassword is incorrect!"},
    {keys::k_ErrorNoProfiles, "No Userprofile found. You need to register online to use this service."},
    {keys::k_ErrorNoPassword, "Please enter password!"},
    {keys::k_ErrorTitle, "Error"},
}};

/// The text cut to the most characters, at the end of a whole UTF-8 character. The boxes of the game took 32
/// characters.
std::string Cut(std::string text, size_t mostCharacters)
{
	size_t characters = 0;
	for (size_t at = 0; at < text.size(); ++at)
	{
		const auto byte = static_cast<unsigned char>(text[at]);
		if ((byte & 0xC0) != 0x80)
		{
			if (characters == mostCharacters)
			{
				text.resize(at);
				break;
			}
			++characters;
		}
	}
	return text;
}

std::vector<uint8_t> BytesOf(std::string_view text)
{
	return {text.begin(), text.end()};
}

bool IsSuccess(const HttpResponse& response)
{
	return response.error == HttpError::None && response.status >= 200 && response.status < 300;
}

/// The error of the dialog when the tool got no answer
std::string_view NoAnswerKey(HttpError error, std::string_view networkKey)
{
	return error == HttpError::Timeout ? keys::k_ErrorTimeout : networkKey;
}

/// Writes why a request failed to the log, with no name, password or token
void LogFailure(std::string_view stage, const HttpResponse& response)
{
	const auto logger = spdlog::get("creatureupload");
	if (!logger)
	{
		return;
	}
	const auto problem = lobby_api::ParseProblem(response.Text());
	SPDLOG_LOGGER_WARN(logger, "The {} failed: error {}, status {}, code \"{}\"", stage, static_cast<int>(response.error),
	                   response.status, problem.has_value() ? problem->code : std::string());
}

} // namespace

Texts::Texts(creatureuploadcfg::Texts fromFile)
    : _fromFile(std::move(fromFile))
{
}

std::string Texts::Get(std::string_view key) const
{
	if (const auto found = _fromFile.find(key); found != _fromFile.end())
	{
		return found->second;
	}
	const auto english = std::ranges::find(k_EnglishTexts, key, &std::pair<std::string_view, std::string_view>::first);
	return english != k_EnglishTexts.end() ? std::string(english->second) : std::string();
}

std::string Format(std::string_view text, std::string_view value)
{
	const auto at = text.find("%s");
	if (at == std::string_view::npos)
	{
		return std::string(text);
	}
	return fmt::format("{}{}{}", text.substr(0, at), value, text.substr(at + 2));
}

std::string PageMessage(std::string_view sentOkText, std::string_view pageAddress)
{
	auto start = sentOkText.find("http://");
	if (start == std::string_view::npos)
	{
		start = sentOkText.find("https://");
	}
	if (start == std::string_view::npos)
	{
		return Format(sentOkText, pageAddress);
	}
	auto end = start;
	while (end < sentOkText.size() && std::isspace(static_cast<unsigned char>(sentOkText[end])) == 0)
	{
		++end;
	}
	return fmt::format("{}{}{}", sentOkText.substr(0, start), pageAddress, sentOkText.substr(end));
}

std::string HostOf(std::string_view origin)
{
	constexpr std::string_view k_Https = "https://";
	if (origin.size() >= k_Https.size() && std::ranges::equal(origin.substr(0, k_Https.size()), k_Https, [](char a, char b) {
		    return std::tolower(static_cast<unsigned char>(a)) == b;
	    }))
	{
		origin.remove_prefix(k_Https.size());
	}
	return std::string(origin.substr(0, origin.find_first_of(":/")));
}

std::optional<Paths> PathsFor(const player_profiles::Store& store, const player_profiles::Profile& profile)
{
	if (!store.folder.has_value() || !store.creatureMinds.has_value() || profile.creatureFile.empty())
	{
		return std::nullopt;
	}
	const auto kept = creature_carry_over::KeptFilesIn(*store.creatureMinds, profile.creatureFile);
	return Paths {
	    .package = *store.folder / player_profiles::ProfileKey(gui::ToUtf16(profile.name)) /
	               std::string(creature_web_page::k_UploadFile),
	    .mind = kept.mind,
	    .physique = kept.physique,
	};
}

UploadFlow::UploadFlow(online::HttpClientInterface& http, std::string origin, Texts texts, uint64_t seed)
    : _http(http)
    , _origin(std::move(origin))
    , _texts(std::move(texts))
    , _seed(seed)
    , _alive(std::make_shared<bool>(true))
{
}

UploadFlow::~UploadFlow()
{
	Close();
}

void UploadFlow::Upload(std::string loginName, std::string password, GatherFiles gather)
{
	if (IsBusy())
	{
		return;
	}
	_most = k_LoginProgressMost;
	SetPosition(0);
	Emit(SetProgressRange {.most = _most});
	// The game shows this text for an empty login name, as the name comes from the profile
	if (loginName.empty())
	{
		Emit(ShowMessage {.text = _texts.Get(keys::k_ErrorNoProfiles),
		                  .caption = _texts.Get(keys::k_ErrorTitle),
		                  .icon = MessageIcon::Error});
		return;
	}
	if (password.empty())
	{
		Emit(ShowMessage {.text = _texts.Get(keys::k_ErrorNoPassword),
		                  .caption = _texts.Get(keys::k_ErrorTitle),
		                  .icon = MessageIcon::Error});
		return;
	}
	Step();
	Emit(SetStatus {.text = _texts.Get(keys::k_Validating)});
	_stage = Stage::LoggingIn;
	const auto body = lobby_api::ToJson(lobby_api::LoginRequest {
	    .name = Cut(std::move(loginName), lobby_api::k_MostNameCharacters),
	    .password = Cut(std::move(password), lobby_api::k_MostPasswordCharacters),
	    .cookie = false,
	});
	_request = _http.Send(
	    online::HttpRequest {
	        .method = HttpMethod::Post,
	        .url = fmt::format("{}{}/sessions", _origin, lobby_api::k_ApiPrefix),
	        .headers = {{"Content-Type", std::string(k_JsonType)}, {"Accept", std::string(k_JsonType)}},
	        .body = BytesOf(body),
	    },
	    [this, alive = std::weak_ptr(_alive), gather = std::move(gather)](HttpResponse&& response) mutable {
		    if (!alive.expired())
		    {
			    LoggedIn(std::move(response), std::move(gather));
		    }
	    });
}

void UploadFlow::LoggedIn(HttpResponse&& response, GatherFiles gather)
{
	_request.reset();
	if (response.error == HttpError::Cancelled)
	{
		return;
	}
	if (!IsSuccess(response))
	{
		LogFailure("login", response);
		if (response.error != HttpError::None)
		{
			Fail(NoAnswerKey(response.error, keys::k_ErrorConnect));
		}
		else
		{
			// The lobby refused the name and the password (it gives one answer for both), or the account cannot log in
			Fail(response.status >= 400 && response.status < 500 ? keys::k_ErrorValidation : keys::k_ErrorRetrievingData);
		}
		return;
	}
	auto session = lobby_api::ParseSession(response.Text());
	if (!session.has_value())
	{
		LogFailure("login", response);
		Fail(keys::k_ErrorRetrievingData);
		return;
	}
	_accessToken = std::move(session->accessToken);

	Emit(EnableUpload {.enabled = false});
	SetPosition(k_LoginProgressMost);
	Emit(SetStatus {.text = _texts.Get(keys::k_OpenFile)});
	auto gathered = gather ? gather() : std::variant<lobby_api::CreatureUpload, Problem>(Problem::NoPackage);
	if (std::holds_alternative<Problem>(gathered))
	{
		if (const auto logger = spdlog::get("creatureupload"))
		{
			SPDLOG_LOGGER_WARN(logger, "The files of the creature failed the checks: problem {}",
			                   static_cast<int>(std::get<Problem>(gathered)));
		}
		// The game keeps the upload button off after this error, until the tool starts again
		Fail(keys::k_ErrorOpenCreatureFile);
		Logout();
		return;
	}
	auto& upload = std::get<lobby_api::CreatureUpload>(gathered);
	Step();
	Emit(SetStatus {.text = Format(_texts.Get(keys::k_Sending), HostOf(_origin))});

	const std::array parts {
	    lobby_api::FormPart {.name = std::string(lobby_api::k_UploadPackageField),
	                         .fileName = std::string(lobby_api::k_UploadPackageFileName),
	                         .contentType = std::string(k_BinaryType),
	                         .bytes = std::move(upload.package)},
	    lobby_api::FormPart {.name = std::string(lobby_api::k_UploadMindField),
	                         .fileName = std::move(upload.mindFileName),
	                         .contentType = std::string(k_BinaryType),
	                         .bytes = std::move(upload.mind)},
	    lobby_api::FormPart {.name = std::string(lobby_api::k_UploadPhysiqueField),
	                         .fileName = std::move(upload.physiqueFileName),
	                         .contentType = std::string(k_BinaryType),
	                         .bytes = std::move(upload.physique)},
	};
	const auto boundary = lobby_api::MultipartBoundary(parts, _seed);
	auto body = lobby_api::MultipartBody(boundary, parts);
	_bodyBytes = static_cast<uint32_t>(body.size());
	_stage = Stage::Sending;
	// The bar fills to the half as the body goes, and to the end with the answer
	SetPosition(0);
	_most = _bodyBytes * 2;
	Emit(SetProgressRange {.most = _most});
	_request = _http.Send(
	    online::HttpRequest {
	        .method = HttpMethod::Post,
	        .url = fmt::format("{}{}/me/creatures", _origin, lobby_api::k_ApiPrefix),
	        .headers = {{"Content-Type", lobby_api::MultipartContentType(boundary)},
	                    {"Accept", std::string(k_JsonType)},
	                    {"Authorization", fmt::format("Bearer {}", _accessToken)}},
	        .body = std::move(body),
	    },
	    [this, alive = std::weak_ptr(_alive)](HttpResponse&& answer) {
		    if (!alive.expired())
		    {
			    Sent(std::move(answer));
		    }
	    });
}

void UploadFlow::Tick()
{
	if (_stage != Stage::Sending || !_request.has_value())
	{
		return;
	}
	if (const auto sent = _http.SentBytes(*_request); sent.has_value())
	{
		const auto position = static_cast<uint32_t>(std::min<uint64_t>(*sent, _most));
		if (position != _position)
		{
			SetPosition(position);
		}
	}
}

void UploadFlow::Sent(HttpResponse&& response)
{
	_request.reset();
	if (response.error == HttpError::Cancelled)
	{
		return;
	}
	SetPosition(_most);
	std::optional<lobby_api::CreatureUploadReply> reply;
	if (IsSuccess(response))
	{
		reply = lobby_api::ParseCreatureUploadReply(response.Text());
	}
	if (reply.has_value())
	{
		Emit(ShowMessage {
		    .text = PageMessage(_texts.Get(keys::k_SendOk), reply->url),
		    .caption = _texts.Get(keys::k_AppTitle),
		    .icon = MessageIcon::None,
		});
		Emit(SetStatus {.text = _texts.Get(keys::k_SentToServer)});
		SetPosition(0);
	}
	else
	{
		LogFailure("upload", response);
		if (response.error != HttpError::None)
		{
			Fail(NoAnswerKey(response.error, keys::k_ErrorSendingData));
		}
		else
		{
			// The lobby did not take the files, as the server of the game refused an upload with a bad result
			Fail(IsSuccess(response) ? keys::k_ErrorRetrievingData : keys::k_ErrorValidation);
		}
	}
	Emit(EnableUpload {.enabled = true});
	_stage = Stage::Idle;
	Logout();
}

void UploadFlow::Fail(std::string_view errorKey)
{
	Emit(ShowMessage {
	    .text = fmt::format("{}\n{}", _texts.Get(keys::k_ErrorBox), _texts.Get(errorKey)),
	    .caption = _texts.Get(keys::k_AppTitle),
	    .icon = MessageIcon::None,
	});
	SetPosition(0);
	_stage = Stage::Idle;
}

void UploadFlow::Close()
{
	if (_request.has_value())
	{
		_http.Cancel(*_request);
		_request.reset();
	}
	_stage = Stage::Idle;
	Logout();
}

void UploadFlow::Logout()
{
	if (_accessToken.empty())
	{
		return;
	}
	// The sign-in of the tool ends with the upload. The tool does not keep it.
	_http.Send(
	    online::HttpRequest {
	        .method = HttpMethod::Delete,
	        .url = fmt::format("{}{}/sessions/current", _origin, lobby_api::k_ApiPrefix),
	        .headers = {{"Authorization", fmt::format("Bearer {}", _accessToken)}},
	        .body = {},
	    },
	    [](HttpResponse&& /*response*/) {});
	_accessToken.clear();
}

void UploadFlow::SetPosition(uint32_t position)
{
	_position = position;
	Emit(SetProgress {.position = position});
}

void UploadFlow::Step()
{
	// A step past the end of the bar starts the bar again from the start, as the progress bar of Windows does
	auto position = _position + k_ProgressStep;
	if (position > _most && _most > 0)
	{
		position %= _most;
	}
	SetPosition(position);
}

void UploadFlow::Emit(Effect effect)
{
	_effects.push_back(std::move(effect));
}

std::vector<Effect> UploadFlow::TakeEffects()
{
	return std::exchange(_effects, {});
}

} // namespace openblack::creature_upload::dialog
