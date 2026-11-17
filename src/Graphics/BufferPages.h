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

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <variant>
#include <vector>

#include "BufferRun.h"
#include "GraphicsHandle.h"
#include "PageRuns.h"
#include "VertexBuffer.h"

namespace openblack::graphics
{

class GraphicsDeviceInterface;
struct DeviceMemory;

/// One page: a buffer of the device that holds the runs of many small buffers
struct BufferPage
{
	std::variant<DynamicVertexBufferHandle, DynamicIndexBufferHandle> handle;
	/// The attributes of the vertices of a page of vertices. A page of indices has none.
	VertexDecl decl;
	/// The bytes of one element: a vertex or an index
	uint32_t elementBytes;
	PageRuns runs;
	/// A copy of the elements of each run whose owner does not give them again after a restore of the device, by the
	/// first element of the run
	std::map<uint32_t, std::vector<uint8_t>> kept;
};

/// The pages that hold the vertices and the triangles of the parts of the models. The device has a limit of 4096
/// buffers whose contents can change. Each model part took two of these buffers, and a land has thousands of model
/// parts. Each part now takes a run of a large page, and a few pages hold the parts of a land.
///
/// The device keeps no copy of a page. After a restore of the device, the owner of a run gives its elements again. The
/// pages keep a copy of each run whose owner does not give it again, and write these copies again after a restore.
///
/// A load on a worker thread makes its buffers at once, thus a mutex guards the pages. A draw reads only the handle of a
/// page, which does not change.
class BufferPages
{
public:
	/// A page of vertices holds this count of bytes. The graphics library puts a page in one of its shared buffers, which
	/// hold three megabytes each, thus a new page gets no new memory from the driver.
	static constexpr uint32_t k_VertexPageBytes = 2u << 20;
	/// A page of indices holds this count of indices of 16 bits
	static constexpr uint32_t k_IndexPageCount = 1u << 18;
	/// Each run of indices starts at a multiple of four bytes, because the web device writes buffers only at these
	/// offsets
	static constexpr uint32_t k_IndexAlignment = 2;

	explicit BufferPages(const GraphicsDeviceInterface& device);
	BufferPages(const BufferPages&) = delete;
	BufferPages& operator=(const BufferPages&) = delete;
	BufferPages(BufferPages&&) = delete;
	BufferPages& operator=(BufferPages&&) = delete;
	~BufferPages();

	/// A new run with the vertices in the memory, in a page of vertices with the attributes of the declaration. The run
	/// takes the memory. Nothing when the device cannot make a page.
	[[nodiscard]] std::optional<BufferRun> TakeVertices(const VertexDecl& decl, uint32_t stride, const DeviceMemory* vertices);
	/// A new run with the indices of 16 bits in the memory. The run takes the memory. Nothing when the device cannot
	/// make a page.
	[[nodiscard]] std::optional<BufferRun> TakeIndices(const DeviceMemory* indices);
	/// Writes the elements in the memory into the run, from the first element of the run on. The elements must fit in the
	/// run.
	void Write(const BufferRun& run, uint32_t first, const DeviceMemory* memory);
	/// Tells the pages that the owner of the run gives its elements again after a restore of the device. The pages then
	/// keep no copy of the run.
	void SetGivenAgain(const BufferRun& run);
	/// Makes the run free. A page with no runs goes from the device.
	void Give(const BufferRun& run);
	/// Writes the copies of the runs again after a restore of the device. The renderer calls this before the owners give
	/// their data again.
	void WriteKeptAgain() const;

	[[nodiscard]] static DynamicVertexBufferHandle VertexHandle(const BufferRun& run);
	[[nodiscard]] static DynamicIndexBufferHandle IndexHandle(const BufferRun& run);

	[[nodiscard]] size_t PageCount() const;
	/// The bytes of the copies that the pages keep
	[[nodiscard]] size_t KeptBytes() const;

private:
	/// A run of count elements in a page that takes the page test, or in a new page from makePage
	template <typename Fits, typename MakePage>
	std::optional<BufferRun> Take(uint32_t count, Fits&& fits, MakePage&& makePage);
	/// The first write of a run: the copy that the pages keep, and the elements on the device
	void WriteFirst(const BufferRun& run, const DeviceMemory* memory);

	const GraphicsDeviceInterface& _device;
	std::vector<std::unique_ptr<BufferPage>> _pages;
	mutable std::mutex _mutex;
};

} // namespace openblack::graphics
