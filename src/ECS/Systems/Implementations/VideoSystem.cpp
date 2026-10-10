/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VideoSystem.h"

#include <utility>

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "Audio/GameMusic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Gui/GameInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Video/FallingSpellAudio.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// A falling spell's film whose frame rate is unknown is timed at 24 frames a second
constexpr int32_t k_FallbackFps = 24;

entt::id_type VideoId(const std::filesystem::path& path)
{
	return entt::hashed_string(path.generic_string().c_str()).value();
}

VideoSystem::Hooks GameHooks()
{
	return {
	    .isPaused = []() { return Locator::time::value().IsPaused(); },
	    .setPaused = [](bool paused) { Locator::time::value().SetPaused(paused); },
	    .isWideScreenOn = []() { return Locator::cinematicDirectorSystem::value().IsWideScreenOn(); },
	    .setWideScreen = [](bool on) { Locator::cinematicDirectorSystem::value().SetWideScreen(on, 0); },
	    .snapWideScreen = []() { Locator::cinematicDirectorSystem::value().SnapWideScreen(); },
	    .releaseScriptMusic =
	        []() {
		        if (auto* game = Game::Instance(); game != nullptr && game->GetGameMusic() != nullptr)
		        {
			        game->GetGameMusic()->StartScriptMusic(audio::MusicType::None);
		        }
	        },
	    .openFile = [](const std::filesystem::path& path) -> std::shared_ptr<const bink::BinkFile> {
		    auto& fileSystem = Locator::filesystem::value();
		    if (!fileSystem.Exists(path))
		    {
			    SPDLOG_LOGGER_WARN(spdlog::get("game"), "The video {} isn't there", path.generic_string());
			    return nullptr;
		    }
		    try
		    {
			    const auto [it, loaded] =
			        Locator::resources::value().GetVideos().Load(VideoId(path), resources::VideoLoader::FromDiskTag {}, path);
			    return it->second ? static_cast<std::shared_ptr<const bink::BinkFile>>(it->second.handle()) : nullptr;
		    }
		    catch (const std::exception& error)
		    {
			    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't read the video {}: {}", path.generic_string(), error.what());
			    return nullptr;
		    }
	    },
	    .closeFile = [](const std::filesystem::path& path) { Locator::resources::value().GetVideos().Erase(VideoId(path)); },
	    .playerHasCreature =
	        []() {
		        const auto local = Locator::playerSystem::value().GetLocalPlayer();
		        auto& registry = Locator::entitiesRegistry::value();
		        bool found = false;
		        registry.Each<const ecs::components::Creature>(
		            [&found, local](const ecs::components::Creature& creature) { found = found || creature.owner == local; });
		        return found;
	        },
	    .fallingSpellPath = []() { return std::filesystem::path("Data/Spells/fall/fall.bik"); },
	    .fallingSpellCue = [audio =
	                            std::make_shared<video::FallingSpellAudio>()](video::FallingSpellCue cue) { audio->Play(cue); },
	    .whiteFadeReached =
	        []() {
		        auto* game = Game::Instance();
		        return game != nullptr && game->GetInterface() != nullptr &&
		               game->GetInterface()->GetScreenFade().GetTurns() > 0;
	        },
	};
}
} // namespace

VideoSystem::VideoSystem()
    : VideoSystem(GameHooks())
{
}

VideoSystem::VideoSystem(Hooks hooks)
    : _hooks(std::move(hooks))
{
}

bool VideoSystem::Play(const std::filesystem::path& path)
{
	_isIntro = false;
	if (_video)
	{
		Delete();
	}
	_alpha = 1.0f;
	_previousPause = _hooks.isPaused();
	_hooks.setPaused(true);

	// A video exists even when its file can't be opened: with no frames it ends at the next update
	_finishedPending = false;
	auto& video = _video.emplace(Video {.path = path});
	if (_filmsEnabled)
	{
		video.file = _hooks.openFile(path);
	}
	if (video.file)
	{
		video.fps = static_cast<int32_t>(video.file->IntegerFps());
		video.frameCount = static_cast<int32_t>(video.file->FrameCount());
#if defined(OPENBLACK_BINK_DECODER)
		std::string error;
		video.reader = bink::FrameReader::Create(video.file, &error);
		if (!video.reader)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The video {} plays black: {}", path.generic_string(), error);
		}
#endif
	}
	_schedule = video::DefaultSchedule(video.frameCount, video.fps);
	_playing.store(true, std::memory_order_release);

	_previousWideScreen = _hooks.isWideScreenOn();
	if (!_previousWideScreen)
	{
		_hooks.setWideScreen(true);
	}
	// Even when a script had them coming in already, they are all the way in from the first frame
	_hooks.snapWideScreen();
	return video.file != nullptr;
}

void VideoSystem::ScheduleIntro()
{
	// With films turned off the intro ends at once, as any other film does
	if (!_video || !_filmsEnabled)
	{
		return;
	}
	_schedule = video::IntroSchedule(_video->fps);
	_isIntro = true;
}

void VideoSystem::StartFallingSpell()
{
	if (!_hooks.playerHasCreature())
	{
		return;
	}
	EndFallingSpell();
	_fallingSpell = true;
	_timeline = {};
	Play(_hooks.fallingSpellPath());
	if (_video)
	{
		_schedule = video::WithoutFade(_schedule);
	}
}

void VideoSystem::EndFallingSpell()
{
	if (!_fallingSpell)
	{
		return;
	}
	_fallingSpell = false;
	// The film then fades as Escape would fade any other
	Skip();
}

void VideoSystem::Update(std::chrono::steady_clock::time_point now)
{
	if (_finishedPending)
	{
		Finished();
	}
	if (_video)
	{
		DecodeDue(now);
	}
	if (_video)
	{
		_alpha = 1.0f;
		switch (video::PhaseAt(_video->frame, _schedule))
		{
		case video::Phase::Ended:
			_alpha = 0.0f;
			Delete();
			break;
		case video::Phase::Fading:
			// The game runs again under the fade
			if (_hooks.isPaused() != _previousPause)
			{
				_hooks.setPaused(_previousPause);
			}
			_alpha = video::VideoAlpha(_video->frame, _schedule);
			break;
		case video::Phase::Playing:
			break;
		}
	}
	if (_fallingSpell)
	{
		UpdateFallingSpell();
	}
}

void VideoSystem::UpdateFallingSpell()
{
	if (!_video)
	{
		_timeline.AdvanceWithoutFilm();
	}
	else
	{
		const int32_t fps = _video->fps > 0 ? _video->fps : k_FallbackFps;
		_cues.clear();
		_timeline.Advance(video::FilmMilliseconds(_video->frame, fps), _hooks.whiteFadeReached(), _cues);
		for (const auto cue : _cues)
		{
			_hooks.fallingSpellCue(cue);
		}
	}
	if (!_video || _timeline.HasEnded())
	{
		EndFallingSpell();
	}
}

void VideoSystem::DecodeDue(std::chrono::steady_clock::time_point now)
{
	auto& video = *_video;
	// A file that didn't open has no frames to count: its counter stays where it is. The intro's schedule then holds
	// it, black and paused, until Escape ends it
	if (!video.file)
	{
		return;
	}
	if (!video.start)
	{
		video.start = now;
	}
	const auto& header = video.file ? video.file->GetHeader() : bink::Header {};
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - *video.start);
	const auto due = static_cast<int32_t>(video::FramesDue(elapsed, header.fpsNumerator > 0 ? header.fpsNumerator : 1,
	                                                       header.fpsDenominator > 0 ? header.fpsDenominator : 1));
	// Every frame due is decoded, however late
	while (_video && _video->frame < due)
	{
		if (_video->frame > _schedule.end)
		{
			Delete();
			break;
		}
		DecodeNext();
	}
}

void VideoSystem::DecodeNext()
{
	auto& video = *_video;
	if (video.frame < video.frameCount && video.reader)
	{
		if (video.reader->DecodeFrame(static_cast<uint32_t>(video.frame)))
		{
			video.hasPicture = true;
			++video.serial;
		}
	}
	++video.frame;
}

void VideoSystem::Delete()
{
	if (!_video)
	{
		return;
	}
	const auto path = _video->path;
	const bool opened = _video->file != nullptr;
	_video.reset();
	_playing.store(false, std::memory_order_release);
	_finishedPending = true;
	_noSkip = false;
	if (opened)
	{
		_hooks.closeFile(path);
	}
}

void VideoSystem::Finished()
{
	_hooks.setPaused(_previousPause);
	if (_hooks.isWideScreenOn() != _previousWideScreen)
	{
		_hooks.setWideScreen(_previousWideScreen);
	}
	_noSkip = false;
	_finishedPending = false;
}

bool VideoSystem::Escape(bool shift, bool ctrl)
{
	if (!_video)
	{
		return false;
	}
	// With Shift or Ctrl held, or while it can't be skipped, Escape does nothing at all
	if (shift || ctrl || _noSkip)
	{
		return true;
	}
	Skip();
	_hooks.releaseScriptMusic();
	return true;
}

void VideoSystem::Skip()
{
	if (_fallingSpell)
	{
		EndFallingSpell();
		return;
	}
	if (!_video)
	{
		return;
	}
	const auto schedule = video::SkipSchedule(_video->frame, _schedule, _video->frameCount);
	if (!schedule)
	{
		Delete();
		return;
	}
	_schedule = *schedule;
	if (_hooks.isPaused() != _previousPause)
	{
		_hooks.setPaused(_previousPause);
	}
}

void VideoSystem::Stop()
{
	Delete();
}

bool VideoSystem::CoversScreen() const
{
	return _video.has_value() && _alpha == 1.0f && !_fallingSpell;
}

std::optional<VideoPicture> VideoSystem::GetPicture() const
{
	if (!_video || _alpha <= 0.0f)
	{
		return std::nullopt;
	}
	VideoPicture picture {
	    .serial = _video->serial,
	    .alpha = video::DrawAlpha(_alpha, _fallingSpell),
	};
	if (_video->file)
	{
		picture.width = _video->file->Width();
		picture.height = _video->file->Height();
	}
	if (_video->reader && _video->hasPicture)
	{
		const auto planes = _video->reader->GetPicture();
		picture.y = planes.y.pixels;
		picture.u = planes.u.pixels;
		picture.v = planes.v.pixels;
		picture.yStride = planes.y.stride;
		picture.chromaStride = planes.u.stride;
	}
	return picture;
}

std::optional<VideoSystemInterface::Status> VideoSystem::GetStatus() const
{
	if (!_video)
	{
		return std::nullopt;
	}
	return Status {
	    .frame = _video->frame,
	    .frameCount = _video->frameCount,
	    .fps = _video->fps,
	    .schedule = _schedule,
	    .alpha = _alpha,
	    .fallingSpell = _fallingSpell,
	    .fallingSpellState = _timeline.State(),
	};
}
