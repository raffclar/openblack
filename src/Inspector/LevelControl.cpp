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

#include <glm/geometric.hpp>

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
	                "Takes a picture of the screen at an exact frame (this one by default), the camera put somewhere or "
	                "looking at an entity for it if asked. The file is written once the frame is drawn, within a few "
	                "frames, and appears whole (the adapter waits for it)",
	                {Parameter("path", "string", "Where to write the PNG; a file in the inspector's folder by default", false),
	                 Parameter("in_frames", "integer", "Frames from now (0 is this frame)", false),
	                 Parameter("at_frame", "integer", "The frame, as game.state counts them", false),
	                 Parameter("camera", "object",
	                           "{position?, focus?, yaw?, pitch?, distance?} as camera.set takes: yaw and pitch in degrees, "
	                           "distance in metres",
	                           false),
	                 Parameter("frame", "integer|object",
	                           "An entity to look at where it is drawn at the picture's frame: its id, or {id, yaw?, pitch?, "
	                           "distance?} (yaw and pitch in degrees, pitch below the horizon; distance in metres); "
	                           "not with camera",
	                           false),
	                 Parameter("hide_gui", "boolean",
	                           "Leave the debug windows, the menu bar and the input lock's notice out of the picture and the "
	                           "frames held around it",
	                           false)},
	                true),
	    Description("pending", "The pictures still to take, and any that failed", {}, false),
	};
}

std::string ScreenshotProvider::Take(const Pending& pending)
{
	return _target.Capture(pending.path, pending.hideGui);
}

void ScreenshotProvider::Hold(Pending pending)
{
	const auto captureAt = pending.frame + k_SettleFrames;
	_holding = Holding {.pending = std::move(pending), .captureAt = captureAt, .until = captureAt + k_SettleFrames};
	if (_holding->pending.hideGui)
	{
		_target.HideDebugGui();
	}
}

void ScreenshotProvider::PlaceCamera()
{
	if (!_holding.has_value() || (!_holding->pending.camera.has_value() && !_holding->pending.framing.has_value()))
	{
		return;
	}
	const auto& holding = _holding->pending;
	auto pose = holding.camera;
	if (holding.framing.has_value())
	{
		const auto now = _camera.State();
		const auto target = _camera.EntityPosition(holding.framing->id);
		if (!now.has_value() || !target.has_value())
		{
			Fail(now.has_value() ? "no entity " + std::to_string(holding.framing->id) + " with a place to frame"
			                     : "there is no camera");
			return;
		}
		pose = FramePose(*target, *holding.framing, *now);
	}
	if (auto why = _camera.Set(*pose); !why.empty())
	{
		Fail(why);
		return;
	}
	_holding->placed = *pose;
}

bool ScreenshotProvider::CameraSettled() const
{
	if (!_holding->pending.camera.has_value() && !_holding->pending.framing.has_value())
	{
		return true;
	}
	const auto now = _camera.State();
	if (!_holding->placed.has_value() || !now.has_value())
	{
		return false;
	}
	constexpr float k_Close = 1e-3f;
	return glm::distance(now->origin, _holding->placed->origin) <= k_Close &&
	       glm::distance(now->focus, _holding->placed->focus) <= k_Close;
}

void ScreenshotProvider::Fail(const std::string& why)
{
	_failures.push_back(_holding->pending.path.generic_string() + ": " + why);
	_holding.reset();
}

void ScreenshotProvider::Frame(uint64_t frame)
{
	_frame = frame;
	if (_holding.has_value() && frame > _holding->until)
	{
		_holding.reset();
	}
	while (!_pending.empty() && _pending.front().frame <= frame)
	{
		// One held picture at a time: the next waits for its frames to come free
		if (Settles(_pending.front()) && _holding.has_value())
		{
			break;
		}
		auto pending = std::move(_pending.front());
		_pending.pop_front();
		if (Settles(pending))
		{
			Hold(std::move(pending));
			continue;
		}
		if (auto why = Take(pending); !why.empty())
		{
			_failures.push_back(pending.path.generic_string() + ": " + why);
		}
	}
	if (!_holding.has_value())
	{
		return;
	}
	if (_holding->pending.hideGui)
	{
		_target.HideDebugGui();
	}
	if (!_holding->taken && frame >= _holding->captureAt)
	{
		// Only once the camera drawn is the one asked for: something else moving it delays the picture, for a while
		if (!CameraSettled())
		{
			if (frame >= _holding->captureAt + k_MostSettleFrames)
			{
				Fail("the camera didn't stay where it was put");
			}
			return;
		}
		if (auto why = Take(_holding->pending); !why.empty())
		{
			Fail(why);
			return;
		}
		_holding->taken = true;
		_holding->until = frame + k_SettleFrames;
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
	std::optional<FrameRequest> framing;
	if (params.contains("camera") && params.contains("frame"))
	{
		return QueryResult::Error("give camera or frame, not both");
	}
	if (const auto it = params.find("frame"); it != params.end())
	{
		std::string error;
		framing = ParseFrameRequest(*it, error);
		if (!framing.has_value())
		{
			return QueryResult::Error("frame: " + error);
		}
		if (!_camera.EntityPosition(framing->id).has_value())
		{
			return QueryResult::Error("frame: no entity " + std::to_string(framing->id) + " with a place");
		}
	}
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
	const auto hideParam = params.find("hide_gui");
	const bool hideGui = hideParam != params.end() && hideParam->is_boolean() && hideParam->get<bool>();
	const bool settles = camera.has_value() || framing.has_value() || hideGui;
	const auto asked = frame;
	if (settles)
	{
		// Held pictures take turns: one asked for while another holds its frames starts once they are free
		frame = std::max(frame, _heldUntil + 1);
	}
	const auto pathParam = StringMember(params, "path");
	const auto path = pathParam.has_value() ? std::filesystem::path(*pathParam)
	                                        : _target.Directory() / ("frame_" + std::to_string(frame) + ".png");
	if (path.extension() != ".png")
	{
		return QueryResult::Error("the path is a .png file");
	}
	if (camera.has_value() || framing.has_value())
	{
		if (const auto state = _camera.State(); state.has_value() && state->heldByPath)
		{
			return QueryResult::Error("a camera path (a miracle's or a script's) holds the camera");
		}
	}
	Pending pending {.frame = frame, .path = path, .camera = camera, .framing = framing, .hideGui = hideGui};
	if (settles)
	{
		_heldUntil = frame + 2 * k_SettleFrames;
	}
	if (frame == now && !settles)
	{
		// This frame, as it is
		if (auto why = Take(pending); !why.empty())
		{
			return QueryResult::Error(why);
		}
	}
	else if (frame == now && !_holding.has_value())
	{
		// From this frame: the camera put in place before the frame is drawn, and the picture taken once it has settled
		Hold(std::move(pending));
	}
	else
	{
		const auto after = std::ranges::upper_bound(_pending, frame, {}, &Pending::frame);
		_pending.insert(after, std::move(pending));
	}
	// A held picture is taken a few frames after its camera is first put in place (later still behind another)
	Json answer = {{"path", path.generic_string()},
	               {"frame", settles ? frame + k_SettleFrames : frame},
	               {"now", now},
	               {"hide_gui", hideGui}};
	if (settles)
	{
		answer["held_from"] = frame;
		if (frame != asked)
		{
			answer["note"] = "another held picture had the frames asked for: this one follows it";
		}
	}
	if (framing.has_value())
	{
		answer["framing"] = framing->id;
	}
	return QueryResult::Value(std::move(answer));
}
