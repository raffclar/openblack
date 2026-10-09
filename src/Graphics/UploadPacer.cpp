/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UploadPacer.h"

#include <algorithm>

using namespace openblack::graphics;

namespace
{
/// The pacer the calling thread's uploads go through, if any: only loading threads have one
thread_local UploadPacer* t_pacer = nullptr;
} // namespace

UploadPacer::Scope::Scope(UploadPacer& pacer)
{
	t_pacer = &pacer;
}

UploadPacer::Scope::~Scope()
{
	t_pacer = nullptr;
}

UploadPacer::Hurry::Hurry(UploadPacer& pacer)
    : _pacer(pacer)
{
	{
		const std::scoped_lock lock(_pacer._mutex);
		++_pacer._hurries;
	}
	_pacer._wake.notify_all();
}

UploadPacer::Hurry::~Hurry()
{
	const std::scoped_lock lock(_pacer._mutex);
	--_pacer._hurries;
}

void UploadPacer::Pace(size_t bytes)
{
	if (t_pacer != nullptr)
	{
		t_pacer->Take(bytes);
	}
}

void UploadPacer::BeginFrame(size_t bytes)
{
	{
		const std::scoped_lock lock(_mutex);
		_allowance = bytes;
		_uploaded = 0;
	}
	_wake.notify_all();
}

void UploadPacer::Release()
{
	{
		const std::scoped_lock lock(_mutex);
		_released = true;
	}
	_wake.notify_all();
}

size_t UploadPacer::GetUploaded() const
{
	const std::scoped_lock lock(_mutex);
	return _uploaded;
}

void UploadPacer::Take(size_t bytes)
{
	std::unique_lock lock(_mutex);
	_wake.wait(lock, [this, bytes] {
		return _released || _hurries > 0 || _uploaded == 0 || bytes <= _allowance - std::min(_uploaded, _allowance);
	});
	_uploaded += bytes;
}
