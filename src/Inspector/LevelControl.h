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

#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <InspectorProvider.h>

#include "CameraControl.h"

namespace openblack::inspector
{

/// A land the game can load
struct LevelInfo
{
	std::string name;
	/// "story" or "playground"
	std::string kind;
	bool valid {false};
};

/// How a land is loaded: as the land menu loads it, the scripts starting again from scratch; or as the story changes
/// land, the scripts going on running
enum class LoadHow : uint8_t
{
	Fresh,
	Story,
};

class LevelTargetInterface
{
public:
	virtual ~LevelTargetInterface() = default;
	[[nodiscard]] virtual std::vector<LevelInfo> Levels() const = 0;
	/// Loads a land by its name, through the game's own loading; why not, if it couldn't
	virtual std::string Load(std::string_view name, LoadHow how) = 0;
	virtual std::string LoadTestbed() = 0;
	/// The land loaded now, by its script's name, or "testbed"; empty before one is
	[[nodiscard]] virtual std::string Current() const = 0;
};

///   level.list                         the story lands and playgrounds
///   level.current                      the land loaded now
///   level.load {name, how?}            loads one, fresh as the land menu does or as the story's change of land does
///   level.testbed                      loads the empty testbed
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeLevelProvider(LevelTargetInterface& levels);

/// Where a kept picture comes from, for its name and its line in the catalogue
struct ShotSource
{
	/// The branch and commit of the worktree the game was built from; empty when unknown
	std::string branch;
	std::string commit;
	/// The renderer: vulkan, d3d12, d3d11, opengl or metal
	std::string backend;
	/// Today, as YYYY-MM-DD
	std::string date;
};

/// The folder kept pictures go to: the one given (--screenshot-root), else OPENBLACK_SCREENSHOT_ROOT's, else
/// E:/openblack/screenshots when the drive is there (on Windows); none otherwise
[[nodiscard]] std::optional<std::filesystem::path>
ResolveShotRoot(const std::optional<std::filesystem::path>& given, const char* environment,
                const std::function<bool(const std::filesystem::path&)>& exists);
/// Whether a picture's feature is a domain and a feature, as the progress tracker's folders name them
/// ("story/opening_cinematic"), and its what a short kebab-case description ("yogi-face-whole-f30")
[[nodiscard]] bool ValidShotFeature(std::string_view feature);
[[nodiscard]] bool ValidShotWhat(std::string_view what);
/// A kept picture's file in its feature's folder: <root>/<domain>/<feature>/<date>_<branch>_<what>[_<backend>].png, with
/// _2, _3... before the extension when one of that name is taken already
[[nodiscard]] std::filesystem::path KeptShotPath(const std::filesystem::path& root, std::string_view feature,
                                                 std::string_view what, const ShotSource& source,
                                                 const std::function<bool(const std::filesystem::path&)>& taken);
/// The date of a moment, as YYYY-MM-DD (UTC)
[[nodiscard]] std::string DateOf(std::chrono::system_clock::time_point moment);

/// What takes a picture of the screen
class ScreenshotTargetInterface
{
public:
	virtual ~ScreenshotTargetInterface() = default;
	/// Asks for the frame being made to be written to a file once it is drawn, without the debug windows if asked. The
	/// file appears whole once written (it is written aside, then renamed).
	virtual std::string Capture(const std::filesystem::path& path, bool hideDebugGui) = 0;
	/// Leaves the debug windows (and the input lock's notice) out of the frame being made, for the frames around a
	/// picture without them
	virtual void HideDebugGui() = 0;
	/// Where pictures go when no path is given and they aren't kept: this game's own folder, gone with the game
	[[nodiscard]] virtual std::filesystem::path Directory() const = 0;
	/// Where kept pictures go, by feature, with their catalogue; none when there is no such folder
	[[nodiscard]] virtual std::optional<std::filesystem::path> Root() const = 0;
	/// The branch, commit, renderer and date a kept picture is named and catalogued with
	[[nodiscard]] virtual ShotSource Source() const = 0;
	/// Whether a file is there (a picture is, once written whole)
	[[nodiscard]] virtual bool Exists(const std::filesystem::path& path) const = 0;
	/// Appends a line to a text file, making it if need be; why not, if it couldn't
	virtual std::string AppendLine(const std::filesystem::path& file, std::string_view line) = 0;
};

/// Pictures of the screen at exact frames, the camera put somewhere, or looking at an entity, for the picture if asked:
///   screenshot.take {path?, feature?, what?, note?, root?, in_frames?, at_frame?, camera? | frame?, hide_gui?}
///                                                              the picture's path and its frame; with a feature and a
///                                                              what it is kept by feature and catalogued
///   screenshot.pending                                         the pictures still to take
class ScreenshotProvider final: public ProviderInterface
{
public:
	ScreenshotProvider(ScreenshotTargetInterface& target, CameraControlInterface& camera);

	[[nodiscard]] std::string_view Name() const override { return "screenshot"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

	/// A picture with a camera, a framing or without the debug windows is taken this many frames after its camera is
	/// first put in place, the camera kept there and the windows kept out from then until this many frames after it: the
	/// frame the picture is read from may be one drawn a little before or after it, which must look the same
	static constexpr uint64_t k_SettleFrames = 3;

	/// Once a frame, as the inspector serves its requests: takes the pictures due this frame
	void Frame(uint64_t frame);
	/// Once a frame, after the player's camera has moved and before the frame is drawn: puts the camera where a picture
	/// due this frame wants it, so that nothing moving the camera this frame (its own easing, a flight, the land's
	/// height under it) spoils the picture, and a framed entity is framed where it is at the picture's frame
	void PlaceCamera();

private:
	struct Pending
	{
		uint64_t frame;
		std::filesystem::path path;
		std::optional<CameraPose> camera;
		std::optional<FrameRequest> framing;
		bool hideGui {false};
		/// A kept picture's line in the catalogue (its camera and frame filled in as it is taken), and the catalogue
		std::optional<Json> record;
		std::filesystem::path catalogue;
	};
	/// A kept picture taken, its line written to the catalogue once the file is whole
	struct Cataloguing
	{
		std::filesystem::path path;
		std::filesystem::path catalogue;
		Json record;
		uint64_t giveUpAt;
	};
	/// How many frames a kept picture's file has to appear before its line is given up
	static constexpr uint64_t k_MostWriteFrames = 600;
	/// Writes the lines of the kept pictures written whole since
	void Catalogue();
	/// Whether a picture needs the frames around it held the same
	[[nodiscard]] static bool Settles(const Pending& pending)
	{
		return pending.camera.has_value() || pending.framing.has_value() || pending.hideGui;
	}
	/// Asks for the picture; why not, if it couldn't
	std::string Take(const Pending& pending, uint64_t frame, const std::optional<CameraPose>& placed);
	/// Starts holding a picture's frames from its frame
	void Hold(Pending pending);
	/// Whether the camera is where the held picture put it
	[[nodiscard]] bool CameraSettled() const;
	/// Gives up the held picture, saying why
	void Fail(const std::string& why);
	/// How many frames past its frame a held picture waits for the camera to stay put before it is given up
	static constexpr uint64_t k_MostSettleFrames = 30;

	ScreenshotTargetInterface& _target;
	CameraControlInterface& _camera;
	std::deque<Pending> _pending;
	/// The picture being held: its camera put in place and the debug windows kept out each frame, from the frame it is
	/// due until the frame it is taken (its frame, settled) and a little after
	struct Holding
	{
		Pending pending;
		/// The first frame it may be taken, once the camera drawn is the one asked for
		uint64_t captureAt;
		/// The last frame held
		uint64_t until;
		bool taken {false};
		/// Where the camera was last put
		std::optional<CameraPose> placed;
	};
	std::optional<Holding> _holding;
	/// The last frame the held pictures asked for so far hold: the next held picture starts after it
	uint64_t _heldUntil {0};
	std::vector<Cataloguing> _cataloguing;
	/// Kept pictures' files asked for, which no later picture may take though not yet written
	std::set<std::filesystem::path> _reserved;
	uint64_t _frame {0};
	std::vector<std::string> _failures;
};

} // namespace openblack::inspector
