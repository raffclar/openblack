/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraEdits.h"

#include <stdexcept>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;

std::shared_ptr<const edt::EDTFile> camera_edits::Load()
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& cache = Locator::resources::value().GetCameraEdits();
	if (!cache.Contains(k_FileId.value()))
	{
		try
		{
			const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "camera.edt";
			cache.Load(k_FileId.value(), resources::CameraEditLoader::FromDiskTag {}, path);
		}
		catch (const std::runtime_error& error)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The scripts' cameras and tracks can't be loaded: {}", error.what());
			return nullptr;
		}
	}
	return cache.Handle(k_FileId.value()).handle();
}

std::optional<edt::EDTCamera> camera_edits::FindCamera(int32_t number)
{
	const auto file = Load();
	if (file == nullptr)
	{
		return std::nullopt;
	}
	const auto found = file->GetCameras().find(number);
	if (found == file->GetCameras().end())
	{
		return std::nullopt;
	}
	return found->second;
}

std::shared_ptr<const edt::EDTTrack> camera_edits::FindTrack(int32_t number)
{
	auto file = Load();
	if (file == nullptr)
	{
		return nullptr;
	}
	const auto found = file->GetTracks().find(number);
	if (found == file->GetTracks().end())
	{
		return nullptr;
	}
	// The track shares the file's ownership
	return {std::move(file), &found->second};
}
