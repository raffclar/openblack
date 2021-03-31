/*****************************************************************************
 * Copyright (c) 2018-2021 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "Entities/Components/CameraSceneNode.h"
#include "Common/FileSystem.h"

#include <glm/vec3.hpp>

namespace openblack
{

struct CameraPoint
{
	glm::vec3 position;
	glm::vec3 rotation;
};

class CameraPath
{
public:
	CameraPath() {};
	~CameraPath() = default;
	bool LoadFromFile(const fs::path& path);
	const std::vector<CameraPoint>& GetPoints() const { return _points; }

private:
	std::vector<CameraPoint> _points;
};
}