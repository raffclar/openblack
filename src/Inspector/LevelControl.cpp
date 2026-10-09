/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LevelControl.h"

#include <cmath>

#include <algorithm>
#include <utility>

using namespace openblack::inspector;

namespace
{

/// The furthest ahead a picture may be asked for
constexpr double k_FurthestFrames = 100000.0;

ParameterDescription Parameter(std::string name, std::string type, std::string description, bool required)
{
	return {.name = std::move(name), .type = std::move(type), .description = std::move(description), .required = required};
}

QueryDescription Description(std::string name, std::string description, std::vector<ParameterDescription> parameters,
                             bool writes, ResultKind kind = ResultKind::Object)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = kind,
	        .needsNear = false,
	        .writes = writes};
}

} // namespace

std::unique_ptr<ProviderInterface> openblack::inspector::MakeLevelProvider(LevelTargetInterface& levels)
{
	auto provider = std::make_unique<FunctionProvider>("level");
	provider->Add(Description("list", "The lands the game can load: the story's and the playgrounds, by name", {}, false,
	                          ResultKind::List),
	              [&levels](const QueryContext& /*context*/) {
		              Json items = Json::array();
		              for (const auto& level : levels.Levels())
		              {
			              items.push_back({{"name", level.name}, {"kind", level.kind}, {"valid", level.valid}});
		              }
		              return QueryResult::Value(std::move(items));
	              });
	provider->Add(Description("current", "The land loaded now", {}, false),
	              [&levels](const QueryContext& /*context*/) { return QueryResult::Value({{"land", levels.Current()}}); });
	provider->Add(
	    Description("load",
	                "Loads a land by its name (level.list). how fresh (the default) loads it as the land menu "
	                "does, the scripts starting again; how story changes land as the story does, through the "
	                "game's own change of land, the scripts going on. Answers once it has loaded",
	                {Parameter("name", "string", "The land's name", true), Parameter("how", "string", "fresh or story", false)},
	                true),
	    [&levels](const QueryContext& context) {
		    const auto name = StringMember(context.params, "name").value_or("");
		    const auto how = StringMember(context.params, "how").value_or("fresh");
		    if (how != "fresh" && how != "story")
		    {
			    return QueryResult::Error("how is fresh or story");
		    }
		    const auto all = levels.Levels();
		    const auto found = std::ranges::find_if(all, [&name](const auto& level) { return level.name == name; });
		    if (found == all.end())
		    {
			    return QueryResult::Error("no land " + name + "; ask level.list");
		    }
		    if (!found->valid)
		    {
			    return QueryResult::Error("the land " + name + " isn't one the game can load");
		    }
		    if (auto why = levels.Load(name, how == "story" ? LoadHow::Story : LoadHow::Fresh); !why.empty())
		    {
			    return QueryResult::Error(why);
		    }
		    return QueryResult::Value({{"loaded", name}, {"how", how}, {"land", levels.Current()}});
	    });
	provider->Add(Description("testbed", "Loads the empty testbed, as the land menu's Creature Testbed does", {}, true),
	              [&levels](const QueryContext& /*context*/) {
		              if (auto why = levels.LoadTestbed(); !why.empty())
		              {
			              return QueryResult::Error(why);
		              }
		              return QueryResult::Value({{"land", levels.Current()}});
	              });
	return provider;
}

ScreenshotProvider::ScreenshotProvider(ScreenshotTargetInterface& target, CameraControlInterface& camera)
    : _target(target)
    , _camera(camera)
{
}

std::vector<QueryDescription> ScreenshotProvider::Describe() const
{
	return {
	    Description("take",
	                "Takes a picture of the screen at an exact frame (this one by default), the camera put somewhere first "
	                "if asked. The file is written once the frame is drawn, within a few frames; step the game to it",
	                {Parameter("path", "string", "Where to write the PNG; a file in the inspector's folder by default", false),
	                 Parameter("in_frames", "integer", "Frames from now (0 is this frame)", false),
	                 Parameter("at_frame", "integer", "The frame, as game.state counts them", false),
	                 Parameter("camera", "object", "{position?, focus?, yaw?, pitch?, distance?} as camera.set takes", false)},
	                true),
	    Description("pending", "The pictures still to take, and any that failed", {}, false),
	};
}

std::string ScreenshotProvider::Take(const Pending& pending)
{
	if (pending.camera.has_value())
	{
		if (auto why = _camera.Set(*pending.camera); !why.empty())
		{
			return why;
		}
	}
	return _target.Capture(pending.path);
}

void ScreenshotProvider::Frame(uint64_t frame)
{
	_frame = frame;
	while (!_pending.empty() && _pending.front().frame <= frame)
	{
		auto pending = std::move(_pending.front());
		_pending.pop_front();
		if (auto why = Take(pending); !why.empty())
		{
			_failures.push_back(pending.path.string() + ": " + why);
		}
	}
}

QueryResult ScreenshotProvider::Run(std::string_view query, const QueryContext& context)
{
	const auto& params = context.params;
	if (query == "pending")
	{
		Json pending = Json::array();
		for (const auto& each : _pending)
		{
			pending.push_back({{"frame", each.frame}, {"path", each.path.generic_string()}});
		}
		return QueryResult::Value({{"pending", std::move(pending)}, {"failed", _failures}});
	}
	if (query != "take")
	{
		return QueryResult::Error("no query screenshot." + std::string(query));
	}
	// Requests are answered as a frame starts: that frame is the next one counted
	const auto now = _frame + 1;
	auto frame = now;
	const auto in = NumberMember(params, "in_frames");
	const auto at = NumberMember(params, "at_frame");
	if (in.has_value() && at.has_value())
	{
		return QueryResult::Error("give in_frames or at_frame, not both");
	}
	if (in.has_value())
	{
		if (*in < 0.0 || *in > k_FurthestFrames || std::floor(*in) != *in)
		{
			return QueryResult::Error("in_frames is a whole number from 0");
		}
		frame = now + static_cast<uint64_t>(*in);
	}
	if (at.has_value())
	{
		if (*at < static_cast<double>(now) || *at > static_cast<double>(now) + k_FurthestFrames || std::floor(*at) != *at)
		{
			return QueryResult::Error("at_frame is this frame (" + std::to_string(now) + ") or a later one");
		}
		frame = static_cast<uint64_t>(*at);
	}
	std::optional<CameraPose> camera;
	if (const auto it = params.find("camera"); it != params.end())
	{
		const auto state = _camera.State();
		std::string error;
		camera = state.has_value() ? ResolveCameraPose(*it, *state, _camera, error) : std::nullopt;
		if (!camera.has_value())
		{
			return QueryResult::Error(error.empty() ? "there is no camera" : "camera: " + error);
		}
	}
	const auto pathParam = StringMember(params, "path");
	const auto path = pathParam.has_value() ? std::filesystem::path(*pathParam)
	                                        : _target.Directory() / ("frame_" + std::to_string(frame) + ".png");
	if (path.extension() != ".png")
	{
		return QueryResult::Error("the path is a .png file");
	}
	Pending pending {.frame = frame, .path = path, .camera = camera};
	if (frame == now)
	{
		// This frame: the camera goes there and the picture is asked for before the frame is drawn
		if (auto why = Take(pending); !why.empty())
		{
			return QueryResult::Error(why);
		}
	}
	else
	{
		const auto after = std::ranges::upper_bound(_pending, frame, {}, &Pending::frame);
		_pending.insert(after, std::move(pending));
	}
	return QueryResult::Value({{"path", path.generic_string()}, {"frame", frame}, {"now", now}});
}
