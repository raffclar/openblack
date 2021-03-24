#include "LevelLocator.h"

using namespace openblack;

void LevelLocator::LoadLevels(FileSystem& fs)
{
	_campaigns.clear();
	_playgrounds.clear();
	auto campaignPaths = fs.GetAllFilePaths(fs.ScriptsPath().string(), _levelExt, false);

	for (auto path : campaignPaths)
	{
		_campaigns.emplace_back(Level(path));
	}

	auto playGroundPaths = fs.GetAllFilePaths((fs.ScriptsPath() / "Playgrounds").string(), _levelExt, false);

	for (auto path : playGroundPaths)
	{
		_playgrounds.emplace_back(Level(path));
	}
}
