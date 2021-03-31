/*****************************************************************************
 * Copyright (c) 2018-2021 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "CameraPath.h"

#include "CamFile.h"

#include <glm/gtx/transform.hpp>


using namespace openblack;

bool CameraPath::LoadFromFile(const fs::path& path)
{
	cam::CamFile file;
	file.Open(path.string());
	auto filePoints = file.GetPoints();

	for (auto& filePoint : filePoints)
	{
		CameraPoint node;
		node.position = glm::vec3(filePoint.x, filePoint.y, filePoint.z);
		node.rotation = glm::normalize(node.position + glm::vec3(filePoint.pitch, filePoint.yaw, filePoint.roll));
		_points.emplace_back(node);
	}

	return true;
}
