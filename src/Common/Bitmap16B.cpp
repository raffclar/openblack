/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Bitmap16B.h"

#include <cstring> // memcpy

using namespace openblack;

Bitmap16B::Bitmap16B(std::span<const uint8_t> fileData)
{
	const auto* header = reinterpret_cast<const uint32_t*>(fileData.data());
	_width = header[1];
	_height = header[2];
	_data.resize(static_cast<size_t>(_width) * _height);
	memcpy(_data.data(), &header[4], Size());
}
