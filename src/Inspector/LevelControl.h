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

#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
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

/// What takes a picture of the screen
class ScreenshotTargetInterface
{
public:
	virtual ~ScreenshotTargetInterface() = default;
	/// Asks for the frame being made to be written to a file once it is drawn
	virtual std::string Capture(const std::filesystem::path& path) = 0;
	/// Where pictures go when no path is given
	[[nodiscard]] virtual std::filesystem::path Directory() const = 0;
};

/// Pictures of the screen at exact frames, the camera put somewhere first if asked:
///   screenshot.take {path?, in_frames?, at_frame?, camera?}   the picture's path and the frame it is of
///   screenshot.pending                                         the pictures still to take
class ScreenshotProvider final: public ProviderInterface
{
public:
	ScreenshotProvider(ScreenshotTargetInterface& target, CameraControlInterface& camera);

	[[nodiscard]] std::string_view Name() const override { return "screenshot"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

	/// Once a frame, as the inspector serves its requests: takes the pictures due this frame
	void Frame(uint64_t frame);

private:
	struct Pending
	{
		uint64_t frame;
		std::filesystem::path path;
		std::optional<CameraPose> camera;
	};
	/// Places the camera and asks for the picture; why not, if it couldn't
	std::string Take(const Pending& pending);

	ScreenshotTargetInterface& _target;
	CameraControlInterface& _camera;
	std::deque<Pending> _pending;
	uint64_t _frame {0};
	std::vector<std::string> _failures;
};

} // namespace openblack::inspector
