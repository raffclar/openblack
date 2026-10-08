/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "L3DAnim.h"

#include <filesystem>
#include <stdexcept>

#include <ANMFile.h>
#include <glm/gtx/matrix_interpolation.hpp>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack;

void L3DAnim::Load(const anm::ANMFile& anm) noexcept
{
	_name = std::string(anm.GetHeader().name.data(), anm.GetHeader().name.size());
	_unknown_0x20 = anm.GetHeader().unknown0x20;
	_unknown_0x24 = anm.GetHeader().unknown0x24;
	_unknown_0x28 = anm.GetHeader().unknown0x28;
	_unknown_0x2C = anm.GetHeader().unknown0x2C;
	_unknown_0x30 = anm.GetHeader().unknown0x30;
	_unknown_0x34 = anm.GetHeader().unknown0x34;
	assert(anm.GetHeader().frameCount == anm.GetKeyframes().size());
	_unknown_0x3C = anm.GetHeader().unknown0x3C;
	_duration = anm.GetHeader().animationDuration;
	_unknown_0x44 = anm.GetHeader().unknown0x44;
	_unknown_0x48 = anm.GetHeader().unknown0x48;
	_unknown_0x50 = anm.GetHeader().unknown0x50;
	// the original's animation loader clears the loop flag of anim 323, ANM_P_OUT_OF_PRAY
	if (_name.starts_with("M_P_Out_Of_Pray"))
	{
		SetLooping(false);
	}

	_frames.reserve(anm.GetKeyframes().size());
	for (const auto& keyframe : anm.GetKeyframes())
	{
		auto& frame = _frames.emplace_back();
		frame.time = keyframe.time;
		frame.bones.resize(keyframe.bones.size());
		for (uint32_t i = 0; i < keyframe.bones.size(); ++i)
		{
			// clang-format off
			auto matrix = glm::mat4(keyframe.bones[i].matrix[0], keyframe.bones[i].matrix[1], keyframe.bones[i].matrix[2], 0.0f,
			                        keyframe.bones[i].matrix[3], keyframe.bones[i].matrix[4], keyframe.bones[i].matrix[5], 0.0f,
			                        keyframe.bones[i].matrix[6], keyframe.bones[i].matrix[7], keyframe.bones[i].matrix[8], 0.0f,
			                        keyframe.bones[i].matrix[9], keyframe.bones[i].matrix[10], keyframe.bones[i].matrix[11], 1.0f);
			// clang-format on
			frame.bones[i] = matrix;
		}
	}
}

bool L3DAnim::LoadFromFilesystem(const std::filesystem::path& path) noexcept
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading L3DAnim from file: {}", path.generic_string());
	anm::ANMFile anm;

	const auto result = anm.ReadFile(*Locator::filesystem::value().GetData(path));

	if (result != anm::ANMResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open l3d animation from buffer: {}", anm::ResultToStr(result));
		return false;
	}

	Load(anm);
	return true;
}

bool L3DAnim::LoadFromFile(const std::filesystem::path& path) noexcept
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading L3DAnim from file: {}", path.generic_string());
	anm::ANMFile anm;

	const auto result = anm.Open(Locator::filesystem::value().FindPath(path));
	if (result != anm::ANMResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open l3d mesh from filesystem {}: {}", path.generic_string(),
		                    anm::ResultToStr(result));
		return false;
	}

	Load(anm);

	return true;
}

bool L3DAnim::LoadFromBuffer(const std::vector<uint8_t>& data) noexcept
{
	anm::ANMFile anm;

	const auto result = anm.Open(data);

	if (result != anm::ANMResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open l3d animation from buffer: {}", anm::ResultToStr(result));
		return false;
	}

	Load(anm);

	return true;
}

std::vector<glm::mat4> L3DAnim::GetBoneMatrices(uint32_t time) const noexcept
{
	if (_frames.empty())
	{
		return {};
	}
	if (_duration == 0)
	{
		return _frames[0].bones;
	}
	uint32_t animationTime = time % _duration;
	uint32_t index = 0;
	uint32_t previousTime = 0;
	for (const auto& frame : _frames)
	{
		if (frame.time < animationTime)
		{
			previousTime = frame.time;
			index++;
		}
		else
		{
			break;
		}
	}
	// No interpolation needed
	if (index == 0)
	{
		return _frames[0].bones;
	}
	if (index >= _frames.size())
	{
		return _frames[_frames.size() - 1].bones;
	}
	float t = static_cast<float>(animationTime - previousTime) / (_frames[index].time - previousTime);

	// Interpolate
	std::vector<glm::mat4> bones;
	bones.resize(_frames[index].bones.size());
	for (uint32_t i = 0; i < bones.size(); ++i)
	{
		// Doing matrix interpolation is not ideal. Would prefer quaternions but
		// extracting a quaternion from a glm::mat4 does not reconstruct the
		// same matrix from the quaternion.
		bones[i] = glm::mat4(glm::mix(_frames[index - 1].bones[i][0], _frames[index].bones[i][0], t),
		                     glm::mix(_frames[index - 1].bones[i][1], _frames[index].bones[i][1], t),
		                     glm::mix(_frames[index - 1].bones[i][2], _frames[index].bones[i][2], t),
		                     glm::mix(_frames[index - 1].bones[i][3], _frames[index].bones[i][3], t));
	}

	// assert(index < _frames.size());
	return bones;
}

void L3DAnim::SampleLocal(int32_t milliseconds, std::vector<glm::mat4>& bones, bool blendSampler) const noexcept
{
	const auto frameCount = static_cast<int32_t>(_frames.size());
	const int32_t duration = GetDurationMs();
	if (frameCount == 0)
	{
		bones.clear();
		return;
	}
	const bool loop = IsLooping();
	// integer maths, like the original
	const int32_t period = loop || frameCount < 2 ? duration : duration * frameCount / (frameCount - 1);
	int32_t t = milliseconds;
	int32_t i = period > 0 ? frameCount * t / period : 0;
	if (i >= frameCount || i < 0)
	{
		i = 0;
		t = 0;
	}
	int32_t j = i + 1;
	if (j == frameCount)
	{
		j = (loop || blendSampler) ? 0 : frameCount - 1;
	}
	const float f =
	    period > 0 ? static_cast<float>(frameCount) / static_cast<float>(period) * static_cast<float>(t) - static_cast<float>(i)
	               : 0.0f;
	const auto& a = _frames[static_cast<size_t>(i)].bones;
	const auto& b = _frames[static_cast<size_t>(j)].bones;
	bones.resize(a.size());
	for (size_t k = 0; k < a.size(); ++k)
	{
		bones[k] = a[k] + (b[k] - a[k]) * f;
	}
}
