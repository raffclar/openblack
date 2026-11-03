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

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace openblack::resources
{
class Rereads;
}

namespace openblack::graphics
{

/// Gives the data of one owner (a mesh or a texture) to the graphics device again after a restore of the device. The
/// owner reads its data again from the game files.
using DeviceDataGiver = std::function<void(resources::Rereads&)>;

/// The owners that give their data to the graphics device again after a restore, because the device keeps no copy of
/// it. An owner adds itself after it tells the device this, and removes itself before it deletes its resources.
class DeviceDataGivers
{
public:
	/// Adds the giver of an owner, or replaces the giver that the owner has
	void Add(const void* owner, DeviceDataGiver giver)
	{
		const std::scoped_lock lock(_mutex);
		for (auto& [each, kept] : _givers)
		{
			if (each == owner)
			{
				kept = std::move(giver);
				return;
			}
		}
		_givers.emplace_back(owner, std::move(giver));
	}

	void Remove(const void* owner)
	{
		const std::scoped_lock lock(_mutex);
		std::erase_if(_givers, [owner](const auto& entry) { return entry.first == owner; });
	}

	[[nodiscard]] size_t Count() const
	{
		const std::scoped_lock lock(_mutex);
		return _givers.size();
	}

	/// Each owner gives its data again, in the order in which the owners added themselves. A giver must not add or
	/// remove an owner.
	void GiveAll(resources::Rereads& rereads) const
	{
		const std::scoped_lock lock(_mutex);
		for (const auto& [owner, giver] : _givers)
		{
			giver(rereads);
		}
	}

private:
	mutable std::mutex _mutex;
	std::vector<std::pair<const void*, DeviceDataGiver>> _givers;
};

} // namespace openblack::graphics
