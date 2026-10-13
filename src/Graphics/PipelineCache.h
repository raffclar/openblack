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
#include <map>
#include <span>
#include <vector>

namespace openblack::graphics
{

/// Keeps the driver's compiled pipelines in a file between runs. The renderer makes a pipeline the first time that it
/// draws with a new set of states. Without a kept copy, the driver compiles the pipeline in the middle of that frame,
/// which can take some milliseconds for each pipeline.
class PipelineCache
{
public:
	/// How the renderer keeps its pipelines
	enum class Kind : uint8_t
	{
		/// One driver cache holds all the pipelines (Vulkan). The renderer starts each new pipeline from all the kept
		/// ones, and gives back the cache with the new pipeline added. The renderer gives each pipeline a number that is
		/// different in each run, so this kind does not use the numbers.
		Shared,
		/// Each pipeline has a cache of its own, kept by its number (Direct3D 12). The number is the same in each run
		/// for the same pipeline. If a number comes again for a different pipeline, the driver refuses the kept cache
		/// and compiles the pipeline.
		PerPipeline,
	};

	/// Reads the kept pipelines from the file, if it is there
	explicit PipelineCache(std::filesystem::path path, Kind kind = Kind::Shared);
	/// Writes the pipelines back to the file if there are new ones
	~PipelineCache();
	PipelineCache(const PipelineCache&) = delete;
	PipelineCache& operator=(const PipelineCache&) = delete;
	PipelineCache(PipelineCache&&) = delete;
	PipelineCache& operator=(PipelineCache&&) = delete;

	/// The size of the kept cache for the pipeline, or 0 when there is none
	[[nodiscard]] uint32_t ReadSize(uint64_t id) const;
	/// Copies the kept cache for the pipeline into data. False when the size is not the size of the kept cache.
	bool Read(uint64_t id, std::span<std::byte> data) const;
	/// Keeps the driver's cache for the pipeline
	void Write(uint64_t id, std::span<const std::byte> data);

private:
	/// The key of a pipeline's cache: all the pipelines share key 0 when the kind is Shared
	[[nodiscard]] uint64_t KeyOf(uint64_t id) const { return _kind == Kind::Shared ? 0 : id; }

	std::filesystem::path _path;
	Kind _kind;
	std::map<uint64_t, std::vector<std::byte>> _caches;
	bool _changed {false};
};

} // namespace openblack::graphics
