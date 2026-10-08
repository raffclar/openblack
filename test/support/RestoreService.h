/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::test
{
/// Keeps the service a Locator entry (an entt::locator) holds when made and puts it back when it goes, also after a
/// failed ASSERT: what the test emplaces or resets in between does not reach the next test
template <typename ServiceLocator>
class RestoreService
{
public:
	RestoreService()
	    : _previous(ServiceLocator::handle())
	{
	}
	~RestoreService() { ServiceLocator::reset(_previous); }
	RestoreService(const RestoreService&) = delete;
	RestoreService& operator=(const RestoreService&) = delete;
	RestoreService(RestoreService&&) = delete;
	RestoreService& operator=(RestoreService&&) = delete;

private:
	typename ServiceLocator::node_type _previous;
};
} // namespace openblack::test
