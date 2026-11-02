/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ConnectionLimits.h"

#include <algorithm>

namespace openblack::server::connection_limits
{
namespace
{

/// The bytes that went stay at the front of the queue until there are this many of them and they are half of the queue
constexpr size_t k_SentBytesToRemove = 64 * 1024;

[[nodiscard]] std::string_view Trim(std::string_view text) noexcept
{
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
	{
		text.remove_prefix(1);
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
	{
		text.remove_suffix(1);
	}
	return text;
}

} // namespace

bool IsLoopback(std::string_view address) noexcept
{
	return address.starts_with("127.") || address == "localhost";
}

std::string ClientAddress(std::string_view peer, std::string_view forwardedFor)
{
	if (!IsLoopback(peer))
	{
		return std::string(peer);
	}
	// The proxy adds the address that it saw at the end of the list
	const auto comma = forwardedFor.rfind(',');
	const auto last = Trim(comma == std::string_view::npos ? forwardedFor : forwardedFor.substr(comma + 1));
	return last.empty() ? std::string(peer) : std::string(last);
}

bool AddressCounts::Add(const std::string& address)
{
	if (IsLoopback(address))
	{
		return true;
	}
	auto& count = _counts[address];
	if (count >= k_MostConnectionsPerAddress)
	{
		return false;
	}
	++count;
	return true;
}

void AddressCounts::Remove(const std::string& address)
{
	const auto found = _counts.find(address);
	if (found == _counts.end())
	{
		return;
	}
	if (--found->second == 0)
	{
		_counts.erase(found);
	}
}

size_t AddressCounts::Of(const std::string& address) const
{
	const auto found = _counts.find(address);
	return found != _counts.end() ? found->second : 0;
}

bool OutQueue::Add(const uint8_t* data, size_t size, size_t most)
{
	if (Size() + size > most)
	{
		return false;
	}
	_bytes.insert(_bytes.end(), data, data + size);
	return true;
}

void OutQueue::Sent(size_t count)
{
	_first += std::min(count, Size());
	if (_first == _bytes.size())
	{
		_bytes.clear();
		_first = 0;
	}
	else if (_first >= k_SentBytesToRemove && _first * 2 >= _bytes.size())
	{
		_bytes.erase(_bytes.begin(), _bytes.begin() + static_cast<std::ptrdiff_t>(_first));
		_first = 0;
	}
}

} // namespace openblack::server::connection_limits
