/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <span>
#include <vector>

namespace openblack::graphics
{

/// Keeps the Vulkan driver's compiled pipelines in a file between runs. The renderer makes a pipeline the first time
/// that it draws with a new set of states. Without a kept copy, the driver compiles the pipeline in the middle of that
/// frame, which can take some milliseconds for each pipeline.
///
/// The renderer gives each pipeline a number that is different in each run, so this cache does not use the numbers. One driver
/// cache holds all the pipelines: the renderer starts each new pipeline from all the kept ones, and gives back the
/// cache with the new pipeline added.
class PipelineCache
{
public:
	/// Reads the kept pipelines from the file, if it is there
	explicit PipelineCache(std::filesystem::path path);
	/// Writes the pipelines back to the file if there are new ones
	~PipelineCache();
	PipelineCache(const PipelineCache&) = delete;
	PipelineCache& operator=(const PipelineCache&) = delete;
	PipelineCache(PipelineCache&&) = delete;
	PipelineCache& operator=(PipelineCache&&) = delete;

	/// The size of the kept pipelines, or 0 when there are none
	[[nodiscard]] uint32_t ReadSize() const;
	/// Copies the kept pipelines into data. False when the size is not the size of the kept pipelines.
	bool Read(std::span<std::byte> data) const;
	/// Keeps the driver's cache, which has the new pipeline in it
	void Write(std::span<const std::byte> data);

private:
	std::filesystem::path _path;
	std::vector<std::byte> _bytes;
	bool _changed {false};
};

} // namespace openblack::graphics
