/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BufferPages.h"

#include <cassert>
#include <cstring>

#include <algorithm>
#include <numeric>
#include <utility>

#include <spdlog/spdlog.h>

#include "GraphicsDeviceInterface.h"

using namespace openblack::graphics;

BufferPages::BufferPages(const GraphicsDeviceInterface& device)
    : _device(device)
{
}

BufferPages::~BufferPages()
{
	// The owners give their runs before the graphics library stops, thus no page stays. A page that stays is not deleted
	// here, because the graphics library can be stopped.
	if (!_pages.empty())
	{
		if (const auto logger = spdlog::get("graphics"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "{} buffer pages still have runs at the end", _pages.size());
		}
	}
}

template <typename Fits, typename MakePage>
std::optional<BufferRun> BufferPages::Take(uint32_t count, Fits&& fits, MakePage&& makePage)
{
	if (count == 0)
	{
		return std::nullopt;
	}
	for (const auto& page : _pages)
	{
		if (!fits(*page))
		{
			continue;
		}
		if (const auto first = page->runs.Take(count); first.has_value())
		{
			return BufferRun {.page = page.get(), .first = *first, .count = count};
		}
	}
	auto page = std::forward<MakePage>(makePage)();
	if (page == nullptr)
	{
		return std::nullopt;
	}
	const auto first = page->runs.Take(count);
	assert(first.has_value());
	// The game gives the elements of each run again after a restore of the device, or the pages give their copy
	std::visit([this](auto handle) { _device.SetContentsGivenAgain(handle); }, page->handle);
	const auto& made = _pages.emplace_back(std::move(page));
	return BufferRun {.page = made.get(), .first = *first, .count = count};
}

std::optional<BufferRun> BufferPages::TakeVertices(const VertexDecl& decl, uint32_t stride, const DeviceMemory* vertices)
{
	assert(stride > 0);
	const std::scoped_lock lock(_mutex);
	const auto count = static_cast<uint32_t>(_device.Bytes(vertices).size() / stride);
	const auto run = Take(
	    count,
	    [&decl](const BufferPage& page) {
		    return std::holds_alternative<DynamicVertexBufferHandle>(page.handle) && page.decl == decl;
	    },
	    [this, &decl, stride, count]() -> std::unique_ptr<BufferPage> {
		    // A part that is larger than a page gets a page of its own size
		    const auto capacity = std::max(k_VertexPageBytes / stride, count);
		    const auto handle = _device.CreateVertexPage(capacity, decl);
		    if (!IsValidHandle(handle))
		    {
			    return nullptr;
		    }
		    return std::make_unique<BufferPage>(
		        BufferPage {.handle = handle, .decl = decl, .elementBytes = stride, .runs = PageRuns(capacity, 1), .kept = {}});
	    });
	if (run.has_value())
	{
		WriteFirst(*run, vertices);
	}
	return run;
}

std::optional<BufferRun> BufferPages::TakeIndices(const DeviceMemory* indices)
{
	const std::scoped_lock lock(_mutex);
	const auto count = static_cast<uint32_t>(_device.Bytes(indices).size() / sizeof(uint16_t));
	const auto run = Take(
	    count, [](const BufferPage& page) { return std::holds_alternative<DynamicIndexBufferHandle>(page.handle); },
	    [this, count]() -> std::unique_ptr<BufferPage> {
		    // A part that is larger than a page gets a page of its own size, with the length that the alignment adds
		    const auto capacity =
		        std::max(k_IndexPageCount, ((count + k_IndexAlignment - 1) / k_IndexAlignment) * k_IndexAlignment);
		    const auto handle = _device.CreateIndexPage(capacity, false);
		    if (!IsValidHandle(handle))
		    {
			    return nullptr;
		    }
		    return std::make_unique<BufferPage>(BufferPage {.handle = handle,
		                                                    .decl = {},
		                                                    .elementBytes = sizeof(uint16_t),
		                                                    .runs = PageRuns(capacity, k_IndexAlignment),
		                                                    .kept = {}});
	    });
	if (run.has_value())
	{
		WriteFirst(*run, indices);
	}
	return run;
}

void BufferPages::WriteFirst(const BufferRun& run, const DeviceMemory* memory)
{
	const auto bytes = _device.Bytes(memory);
	run.page->kept.insert_or_assign(run.first, std::vector<uint8_t>(bytes.begin(), bytes.end()));
	std::visit([this, &run, memory](auto handle) { _device.Update(handle, run.first, memory); }, run.page->handle);
}

void BufferPages::Write(const BufferRun& run, uint32_t first, const DeviceMemory* memory)
{
	assert(run.page != nullptr);
	const std::scoped_lock lock(_mutex);
	const auto bytes = _device.Bytes(memory);
	const auto& page = *run.page;
	const auto offset = static_cast<size_t>(first) * page.elementBytes;
	if (first > run.count || bytes.size() > (static_cast<size_t>(run.count) * page.elementBytes) - offset)
	{
		// A write past the end of the run would change the elements of the next run. This is an error of the caller.
		if (const auto logger = spdlog::get("graphics"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "A write of {} bytes does not fit in a run of {} elements", bytes.size(), run.count);
		}
		assert(false);
		return;
	}
	if (const auto kept = run.page->kept.find(run.first); kept != run.page->kept.end())
	{
		std::ranges::copy(bytes, kept->second.begin() + static_cast<std::ptrdiff_t>(offset));
	}
	std::visit([this, &run, first, memory](auto handle) { _device.Update(handle, run.first + first, memory); }, page.handle);
}

void BufferPages::SetGivenAgain(const BufferRun& run)
{
	assert(run.page != nullptr);
	const std::scoped_lock lock(_mutex);
	run.page->kept.erase(run.first);
}

void BufferPages::Give(const BufferRun& run)
{
	assert(run.page != nullptr);
	const std::scoped_lock lock(_mutex);
	auto& page = *run.page;
	page.kept.erase(run.first);
	page.runs.Give(run.first, run.count);
	if (!page.runs.Empty())
	{
		return;
	}
	std::visit([this](auto handle) { _device.Destroy(handle); }, page.handle);
	std::erase_if(_pages, [&page](const auto& each) { return each.get() == &page; });
}

void BufferPages::WriteKeptAgain() const
{
	const std::scoped_lock lock(_mutex);
	for (const auto& page : _pages)
	{
		for (const auto& [first, bytes] : page->kept)
		{
			const auto* memory = _device.Copy(bytes.data(), static_cast<uint32_t>(bytes.size()));
			std::visit([this, first, memory](auto handle) { _device.Update(handle, first, memory); }, page->handle);
		}
	}
}

DynamicVertexBufferHandle BufferPages::VertexHandle(const BufferRun& run)
{
	assert(run.page != nullptr);
	return std::get<DynamicVertexBufferHandle>(run.page->handle);
}

DynamicIndexBufferHandle BufferPages::IndexHandle(const BufferRun& run)
{
	assert(run.page != nullptr);
	return std::get<DynamicIndexBufferHandle>(run.page->handle);
}

size_t BufferPages::PageCount() const
{
	const std::scoped_lock lock(_mutex);
	return _pages.size();
}

size_t BufferPages::KeptBytes() const
{
	const std::scoped_lock lock(_mutex);
	return std::accumulate(_pages.begin(), _pages.end(), size_t {0}, [](size_t sum, const auto& page) {
		return std::accumulate(page->kept.begin(), page->kept.end(), sum,
		                       [](size_t inner, const auto& kept) { return inner + kept.second.size(); });
	});
}
