/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UserData.h"

#include <string>

#include <sys/stat.h>

#import <Foundation/Foundation.h>

// The folder is the folder that SDL gives for the organisation "openblack" and the application "openblack": the two
// folders in the application support folder of the user. The test test_input_device compares the two.
std::optional<std::filesystem::path> openblack::user_data::Folder()
{
	@autoreleasepool
	{
		NSArray* folders = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
		if ([folders count] == 0)
		{
			return std::nullopt;
		}
		const char* base = [[folders objectAtIndex:0] fileSystemRepresentation];
		if (base == nullptr)
		{
			return std::nullopt;
		}
		std::string folder = std::string(base) + "/openblack/openblack/";
		// Make each folder of the path that is not there yet. A fault does not stop the next folders.
		for (size_t slash = folder.find('/', 1); slash != std::string::npos; slash = folder.find('/', slash + 1))
		{
			mkdir(folder.substr(0, slash).c_str(), 0700);
		}
		mkdir(folder.c_str(), 0700);
		// The folder is UTF-8
		return std::filesystem::path(std::u8string(folder.begin(), folder.end()));
	}
}
