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
#include <cstdio>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
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

std::optional<std::filesystem::path>
openblack::inspector::ResolveShotRoot(const std::optional<std::filesystem::path>& given, const char* environment,
                                      const std::function<bool(const std::filesystem::path&)>& exists)
{
	if (given.has_value() && !given->empty())
	{
		return given;
	}
	if (environment != nullptr && *environment != '\0')
	{
		return std::filesystem::path(environment);
	}
#if defined(_WIN32)
	if (exists && exists(std::filesystem::path("E:/")))
	{
		return std::filesystem::path("E:/openblack/screenshots");
	}
#else
	static_cast<void>(exists);
#endif
	return std::nullopt;
}

namespace
{

/// Lower case letters, digits, _ and -, at least one
bool NamePart(std::string_view part)
{
	return !part.empty() && std::ranges::all_of(part, [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
	});
}

/// A name made safe for a file: anything but letters, digits, _, - and . becomes -
std::string FileNamePart(std::string_view text)
{
	std::string part(text);
	std::ranges::replace_if(
	    part,
	    [](char c) {
		    return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
		             c == '.');
	    },
	    '-');
	return part;
}

} // namespace

bool openblack::inspector::ValidShotFeature(std::string_view feature)
{
	const auto slash = feature.find('/');
	return slash != std::string_view::npos && NamePart(feature.substr(0, slash)) && NamePart(feature.substr(slash + 1));
}

bool openblack::inspector::ValidShotWhat(std::string_view what)
{
	if (what.empty() || what.size() > 80 || what.front() == '-' || what.back() == '-' ||
	    what.find("--") != std::string_view::npos)
	{
		return false;
	}
	return std::ranges::all_of(what, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

std::filesystem::path openblack::inspector::KeptShotPath(const std::filesystem::path& root, std::string_view feature,
                                                         std::string_view what, const ShotSource& source,
                                                         const std::function<bool(const std::filesystem::path&)>& taken)
{
	const auto slash = feature.find('/');
	const auto folder = root / std::string(feature.substr(0, slash)) / std::string(feature.substr(slash + 1));
	std::string stem =
	    source.date + "_" + FileNamePart(source.branch.empty() ? "unknown-branch" : source.branch) + "_" + std::string(what);
	if (!source.backend.empty())
	{
		stem += "_" + FileNamePart(source.backend);
	}
	auto path = folder / (stem + ".png");
	for (int suffix = 2; taken && taken(path); ++suffix)
	{
		path = folder / (stem + "_" + std::to_string(suffix) + ".png");
	}
	return path;
}

std::string openblack::inspector::DateOf(std::chrono::system_clock::time_point moment)
{
	// The civil date of a count of days since 1970-01-01
	const auto days = std::chrono::floor<std::chrono::days>(moment).time_since_epoch().count() + 719468;
	const auto era = (days >= 0 ? days : days - 146096) / 146097;
	const auto dayOfEra = days - era * 146097;
	const auto yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
	const auto dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
	const auto shifted = (5 * dayOfYear + 2) / 153;
	const auto day = dayOfYear - (153 * shifted + 2) / 5 + 1;
	const auto month = shifted < 10 ? shifted + 3 : shifted - 9;
	const auto year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
	std::array<char, 16> text {};
	std::snprintf(text.data(), text.size(), "%04d-%02d-%02d", static_cast<int>(year), static_cast<int>(month),
	              static_cast<int>(day));
	return text.data();
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
	                {Parameter("path", "string",
	                           "Where to write the PNG; without it and without feature, a temporary file in this game's "
	                           "own folder, gone with the game",
	                           false),
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
	                 Parameter("feature", "string",
	                           "Keep it under the screenshot folder by feature, as the progress tracker names them: "
	                           "\"domain/feature\", e.g. \"story/opening_cinematic\"; with what",
	                           false),
	                 Parameter("what", "string",
	                           "With feature: a short kebab-case description, e.g. \"yogi-face-whole-f30\". It is saved "
	                           "as <root>/<domain>/<feature>/<date>_<branch>_<what>_<backend>.png, never over another, "
	                           "and catalogued in <root>/catalogue.jsonl once written",
	                           false),
	                 Parameter("note", "string", "With feature: a note for the catalogue's line", false),
	                 Parameter("root", "string",
	                           "The screenshot folder, for this picture; the game's own (--screenshot-root, "
	                           "OPENBLACK_SCREENSHOT_ROOT or E:/openblack/screenshots) by default",
	                           false),
	                 Parameter("hide_gui", "boolean",
	                           "Leave the debug windows, the menu bar and the input lock's notice out of the picture and the "
	                           "frames held around it",
	                           false)},
	                true),
	    Description("pending",
	                "The pictures still to take (pending), the held one waiting for its camera (holding), those taken "
	                "but not yet written (writing), and any that failed with why",
	                {}, false),
	};
}

std::string ScreenshotProvider::Take(const Pending& pending, uint64_t frame, const std::optional<CameraPose>& placed)
{
	// A file of that name from before would read as this picture written at once, though the new one never comes
	if (_target.Exists(pending.path))
	{
		if (auto why = _target.Remove(pending.path); !why.empty())
		{
			_reserved.erase(pending.path);
			return "a file is there already and can't be replaced: " + why;
		}
	}
	if (auto why = _target.Capture(pending.path, pending.hideGui); !why.empty())
	{
		_reserved.erase(pending.path);
		return why;
	}
	if (pending.record.has_value())
	{
		// Its line says the frame and the camera it was taken from: where the picture's camera was put, or the player's
		auto record = *pending.record;
		record["frame"] = frame;
		std::optional<CameraPose> pose = placed;
		if (!pose.has_value())
		{
			if (const auto state = _camera.State(); state.has_value())
			{
				pose = CameraPose {.origin = state->origin, .focus = state->focus};
			}
		}
		if (pose.has_value())
		{
			const auto angles = AnglesOf(*pose);
			record["camera"] = {{"origin", {pose->origin.x, pose->origin.y, pose->origin.z}},
			                    {"focus", {pose->focus.x, pose->focus.y, pose->focus.z}},
			                    {"yaw", angles.yaw},
			                    {"pitch", angles.pitch},
			                    {"distance", angles.distance}};
		}
		_cataloguing.push_back({.path = pending.path,
		                        .catalogue = pending.catalogue,
		                        .record = std::move(record),
		                        .giveUpAt = frame + k_MostWriteFrames});
		return {};
	}
	// Watched until written too, so that a picture the renderer never gives back fails, saying so
	_cataloguing.push_back({.path = pending.path, .giveUpAt = frame + k_MostWriteFrames});
	return {};
}

void ScreenshotProvider::Catalogue()
{
	std::erase_if(_cataloguing, [this](const Cataloguing& each) {
		if (_target.Exists(each.path))
		{
			// Written whole: a kept one's line goes in the catalogue now, never before
			if (auto why = each.record.is_null() ? std::string {} : _target.AppendLine(each.catalogue, Dump(each.record));
			    !why.empty())
			{
				_failures.push_back(each.path.generic_string() + ": not catalogued: " + why);
			}
			_reserved.erase(each.path);
			return true;
		}
		if (_frame >= each.giveUpAt)
		{
			_failures.push_back(each.path.generic_string() + ": never written in " + std::to_string(k_MostWriteFrames) +
			                    " frames: the renderer didn't give the picture back" +
			                    (each.record.is_null() ? "" : ", so not catalogued"));
			_reserved.erase(each.path);
			return true;
		}
		return false;
	});
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
	if (auto why = _camera.Pin(*pose); !why.empty())
	{
		Fail(why);
		return;
	}
	_holding->placed = *pose;
	// Where the camera is now, after everything else moved it this frame, is where this frame is drawn from. Its focus
	// reads back as where the middle of the view meets the land, which needn't be the point asked to look at: the
	// direction it looks in is compared instead.
	const auto now = _camera.State();
	if (!now.has_value())
	{
		_holding->inPlace = false;
		return;
	}
	constexpr float k_Close = 1e-3f;
	const auto asked = pose->focus - pose->origin;
	const bool looksThere = glm::distance(now->focus, pose->focus) <= k_Close ||
	                        (glm::length(now->forward) > 0.0f && glm::length(asked) > 0.0f &&
	                         glm::dot(glm::normalize(now->forward), glm::normalize(asked)) >= 1.0f - k_Close);
	_holding->inPlace = glm::distance(now->origin, pose->origin) <= k_Close && looksThere;
}

void openblack::inspector::ShowCameraForDrawing(CameraControlInterface& camera, ScreenshotProvider& screenshots)
{
	if (const auto overridden = camera.Override(); overridden.has_value())
	{
		static_cast<void>(camera.Pin(*overridden));
	}
	screenshots.PlaceCamera();
}

bool ScreenshotProvider::CameraSettled() const
{
	if (!_holding->pending.camera.has_value() && !_holding->pending.framing.has_value())
	{
		return true;
	}
	// As the camera was put for the last frame drawn: by the time the next frame starts the inspector has given the
	// camera back its own place (it carries on beneath the picture's), so it isn't read again here
	return _holding->inPlace;
}

void ScreenshotProvider::Fail(const std::string& why)
{
	_failures.push_back(_holding->pending.path.generic_string() + ": " + why);
	_holding.reset();
}

void ScreenshotProvider::Frame(uint64_t frame)
{
	_frame = frame;
	Catalogue();
	// Let go of once its frames are drawn after the picture: one not yet taken waits for its camera, and fails when it
	// never comes, rather than being dropped unsaid
	if (_holding.has_value() && _holding->taken && frame > _holding->until)
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
		if (auto why = Take(pending, frame, std::nullopt); !why.empty())
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
		if (auto why = Take(_holding->pending, frame, _holding->placed); !why.empty())
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
		Json answer = {{"pending", std::move(pending)}, {"failed", _failures}};
		if (_holding.has_value() && !_holding->taken)
		{
			answer["holding"] = {{"path", _holding->pending.path.generic_string()},
			                     {"held_from", _holding->pending.frame},
			                     {"capture_at", _holding->captureAt},
			                     {"camera_in_place", CameraSettled()}};
		}
		Json writing = Json::array();
		for (const auto& each : _cataloguing)
		{
			writing.push_back({{"path", each.path.generic_string()}, {"gives_up_at", each.giveUpAt}});
		}
		answer["writing"] = std::move(writing);
		return QueryResult::Value(std::move(answer));
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
	const auto feature = StringMember(params, "feature");
	const auto what = StringMember(params, "what");
	const auto note = StringMember(params, "note");
	if (feature.has_value() != what.has_value())
	{
		return QueryResult::Error("a kept picture needs both feature (\"domain/feature\") and what (kebab-case)");
	}
	if (feature.has_value() && !ValidShotFeature(*feature))
	{
		return QueryResult::Error("feature is a domain and a feature as the progress tracker names them, e.g. "
		                          "\"story/opening_cinematic\" (lower case, digits, _ and -)");
	}
	if (what.has_value() && !ValidShotWhat(*what))
	{
		return QueryResult::Error("what is a short kebab-case description, e.g. \"yogi-face-whole-f30\"");
	}
	std::optional<std::filesystem::path> root;
	if (const auto rootParam = StringMember(params, "root"); rootParam.has_value())
	{
		root = std::filesystem::path(*rootParam);
	}
	else if (feature.has_value())
	{
		root = _target.Root();
	}
	if (feature.has_value() && !root.has_value())
	{
		return QueryResult::Error("there is no screenshot folder to keep it in: start the game with --screenshot-root or "
		                          "OPENBLACK_SCREENSHOT_ROOT, or give root");
	}
	const auto source = feature.has_value() ? _target.Source() : ShotSource {};
	std::filesystem::path path;
	if (pathParam.has_value())
	{
		path = *pathParam;
	}
	else if (feature.has_value())
	{
		path = KeptShotPath(*root, *feature, *what, source, [this](const std::filesystem::path& candidate) {
			return _reserved.contains(candidate) || _target.Exists(candidate);
		});
	}
	else
	{
		path = _target.Directory() / ("frame_" + std::to_string(frame) + ".png");
	}
	if (path.extension() != ".png")
	{
		return QueryResult::Error("the path is a .png file");
	}
	Pending pending {.frame = frame, .path = path, .camera = camera, .framing = framing, .hideGui = hideGui};
	if (feature.has_value())
	{
		// Kept: its line in the catalogue, written once the picture is
		Json record = {{"path", path.generic_string()},
		               {"feature", *feature},
		               {"branch", source.branch},
		               {"commit", source.commit},
		               {"date", source.date},
		               {"backend", source.backend},
		               {"frame", frame},
		               {"camera", Json::object()},
		               {"what", *what}};
		if (note.has_value())
		{
			record["agent_note"] = *note;
		}
		pending.record = std::move(record);
		pending.catalogue = *root / "catalogue.jsonl";
		_reserved.insert(path);
	}
	if (settles)
	{
		_heldUntil = frame + 2 * k_SettleFrames;
	}
	if (frame == now && !settles)
	{
		// This frame, as it is
		if (auto why = Take(pending, now, std::nullopt); !why.empty())
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
	if (feature.has_value())
	{
		answer["kept"] = true;
		answer["catalogue"] = (*root / "catalogue.jsonl").generic_string();
	}
	else if (!pathParam.has_value())
	{
		answer["temporary"] = true;
		answer["kept_note"] = "a temporary picture in this game's own folder, gone with the game: give feature and what "
		                      "to keep it under the screenshot folder and catalogue it";
	}
	return QueryResult::Value(std::move(answer));
}
