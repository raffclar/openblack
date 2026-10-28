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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <CreatureUploadCfg.h>
#include <LobbyApi.h>

#include "Common/PlayerProfiles.h"
#include "Online/CreatureUpload.h"
#include "Online/HttpClientInterface.h"

/// The dialog of the creature upload tool (apps/creatureupload), as the creature upload tool of the game showed it. The
/// player picks a profile, types the login name and the password of the lobby, and clicks the upload button. The tool
/// logs in to the lobby, reads the files of the profile's creature, and sends them to the lobby. The lobby shows the
/// creature on a web page.
///
/// The flow here has no window. It gives the changes of the dialog as a list of effects, in the order that the dialog
/// must show them. A message box stops the dialog until the player closes it, so the effects after a message box show
/// after it closes.
namespace openblack::creature_upload::dialog
{

/// The keys of the texts in the language file of the tool
namespace keys
{
constexpr std::string_view k_AppTitle = "IDC_APPTITLE";
constexpr std::string_view k_ErrorBox = "IDC_ERROR_BOX";
constexpr std::string_view k_Validating = "IDC_INFO_VALIDATING";
constexpr std::string_view k_OpenFile = "IDC_INFO_OPEN_FILE";
constexpr std::string_view k_Sending = "IDC_INFO_SENDING";
constexpr std::string_view k_SendOk = "IDC_INFO_SENDOK";
constexpr std::string_view k_SentToServer = "IDC_INFO_SENTTOSERVER";
constexpr std::string_view k_GroupBox = "IDC_STATIC_GROUPBOX";
constexpr std::string_view k_Profile = "IDC_STATIC_PROFILE";
constexpr std::string_view k_LoginName = "IDC_STATIC_LOGINNAME";
constexpr std::string_view k_LoginPassword = "IDC_STATIC_LOGINPASSWORD";
constexpr std::string_view k_DoUpload = "IDC_DO_UPLOAD";
constexpr std::string_view k_Cancel = "IDCANCEL";
constexpr std::string_view k_ErrorConnect = "ERROR_CONNECT";
constexpr std::string_view k_ErrorSendingData = "ERROR_SENDING_DATA";
constexpr std::string_view k_ErrorRetrievingData = "ERROR_RETRIEVING_DATA";
constexpr std::string_view k_ErrorOpenCreatureFile = "ERROR_OPEN_CREATUREFILE";
constexpr std::string_view k_ErrorTimeout = "ERROR_TIMEOUT";
constexpr std::string_view k_ErrorValidation = "ERROR_VALIDATION";
constexpr std::string_view k_ErrorNoProfiles = "ERROR_NO_PROFILES";
constexpr std::string_view k_ErrorNoPassword = "ERROR_NO_PASSWORD";
constexpr std::string_view k_ErrorTitle = "ERROR_TITLE";
} // namespace keys

/// The name of the language file of the tool, in the folder of the game
constexpr std::string_view k_LanguageFile = "creatureupload.cfg";

/// The progress bar goes from 0 to this value while the tool logs in and opens the files. Each step adds this much.
constexpr uint32_t k_LoginProgressMost = 20;
constexpr uint32_t k_ProgressStep = 2;
/// The time between two updates of the progress bar while the tool sends the files
constexpr uint32_t k_SendTimerMs = 120;

/// The texts of the dialog: the texts of the language file of the game, or the English texts of the game when the file
/// does not have the key
class Texts
{
public:
	explicit Texts(creatureuploadcfg::Texts fromFile = {});

	[[nodiscard]] std::string Get(std::string_view key) const;

private:
	creatureuploadcfg::Texts _fromFile;
};

/// The text with the first "%s" replaced by the value
[[nodiscard]] std::string Format(std::string_view text, std::string_view value);
/// The text after a successful upload. The text of the game gives the address of the creature's page on the site of the
/// game, with the login name. The lobby gives the address of the page, so the address in the text changes to it. A text
/// with no web address gets the address at its "%s".
[[nodiscard]] std::string PageMessage(std::string_view sentOkText, std::string_view pageAddress);
/// The name of the host of an https:// origin, for example "bw.example.org"
[[nodiscard]] std::string HostOf(std::string_view origin);
/// Where the files of a profile's creature are. None when the store has no folders.
[[nodiscard]] std::optional<Paths> PathsFor(const player_profiles::Store& store, const player_profiles::Profile& profile);

/// The status line under the boxes
struct SetStatus
{
	std::string text;
};
/// The progress bar goes from 0 to the most
struct SetProgressRange
{
	uint32_t most {0};
};
struct SetProgress
{
	uint32_t position {0};
};
struct EnableUpload
{
	bool enabled {true};
};
enum class MessageIcon : uint8_t
{
	None,
	Error,
};
/// A message box with an OK button
struct ShowMessage
{
	std::string text;
	std::string caption;
	MessageIcon icon {MessageIcon::None};
};
using Effect = std::variant<SetStatus, SetProgressRange, SetProgress, EnableUpload, ShowMessage>;

/// Reads and checks the files of the creature, after the login
using GatherFiles = std::function<std::variant<lobby_api::CreatureUpload, Problem>()>;

class UploadFlow
{
public:
	/// The origin is the https:// origin of the lobby. The seed makes the boundaries of the bodies different.
	UploadFlow(online::HttpClientInterface& http, std::string origin, Texts texts, uint64_t seed);
	~UploadFlow();
	UploadFlow(const UploadFlow&) = delete;
	UploadFlow& operator=(const UploadFlow&) = delete;
	UploadFlow(UploadFlow&&) = delete;
	UploadFlow& operator=(UploadFlow&&) = delete;

	/// The player clicked the upload button. The tool does nothing while an upload is in progress.
	void Upload(std::string loginName, std::string password, GatherFiles gather);
	/// The timer of the progress bar while the tool sends the files
	void Tick();
	/// The player closed the dialog: the requests stop
	void Close();

	/// The changes of the dialog since the last call, in their order
	[[nodiscard]] std::vector<Effect> TakeEffects();
	[[nodiscard]] bool IsBusy() const noexcept { return _stage != Stage::Idle; }
	[[nodiscard]] bool IsSending() const noexcept { return _stage == Stage::Sending; }

private:
	enum class Stage : uint8_t
	{
		Idle,
		LoggingIn,
		Sending,
	};

	void LoggedIn(online::HttpResponse&& response, GatherFiles gather);
	void Sent(online::HttpResponse&& response);
	void Fail(std::string_view errorKey);
	void SetPosition(uint32_t position);
	void Step();
	void Logout();
	void Emit(Effect effect);

	online::HttpClientInterface& _http;
	std::string _origin;
	Texts _texts;
	uint64_t _seed;
	Stage _stage {Stage::Idle};
	std::vector<Effect> _effects;
	std::string _accessToken;
	std::optional<online::HttpRequestId> _request;
	uint32_t _position {0};
	uint32_t _most {k_LoginProgressMost};
	uint32_t _bodyBytes {0};
	/// The callbacks of the requests do nothing after the flow ends
	std::shared_ptr<bool> _alive;
};

} // namespace openblack::creature_upload::dialog
