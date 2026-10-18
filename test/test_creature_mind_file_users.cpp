/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

// Only the creature mind component reads and writes the creature files. This test looks through the source for other
// code that opens the files or makes their names itself.

namespace
{
/// The source folder, from the environment of ctest, else as it was built
std::filesystem::path SourceDir()
{
	if (const auto* dir = std::getenv("OPENBLACK_SOURCE_DIR"); dir != nullptr && *dir != '\0')
	{
		return dir;
	}
	return OPENBLACK_SOURCE_DIR;
}

std::string ReadText(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/// The lines of the text that are not comments
std::vector<std::string_view> CodeLines(std::string_view text)
{
	std::vector<std::string_view> lines;
	while (!text.empty())
	{
		const auto end = text.find('\n');
		auto line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		const auto start = line.find_first_not_of(" \t");
		if (start == std::string_view::npos || line.substr(start).starts_with("//") || line.substr(start).starts_with("*"))
		{
			continue;
		}
		lines.push_back(line);
	}
	return lines;
}

/// The headers of the creature mind component
constexpr std::array<std::string_view, 3> k_Headers {"<MindFile.h>", "<PhysiqueFile.h>", "<MindBody.h>"};
/// Code that reads or writes the bytes of a whole file
constexpr std::array<std::string_view, 6> k_RawFileAccess {
    "istreambuf_iterator", ".write(reinterpret_cast", "ReadAll(", "fopen(", "fread(", "fwrite(",
};
/// The start of the name of a physique file, which only the component gives
constexpr std::string_view k_PhysiqueName = "\"Physique";

struct Finding
{
	std::filesystem::path file;
	std::string what;
};

std::vector<Finding> FindDirectUsers()
{
	const auto root = SourceDir();
	const auto component = root / "components" / "creaturemind";
	std::vector<Finding> findings;
	for (const auto* folder : {"src", "apps", "components", "test"})
	{
		std::error_code error;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(root / folder, error))
		{
			const auto& path = entry.path();
			const auto extension = path.extension().string();
			if (!entry.is_regular_file(error) || (extension != ".cpp" && extension != ".h"))
			{
				continue;
			}
			const bool inComponent = path.generic_string().starts_with(component.generic_string() + "/");
			const auto name = path.filename().string();
			// The component and its own tests may use the bytes and the names of the files
			if (inComponent || name.starts_with("test_creature_mind_file"))
			{
				continue;
			}
			const auto text = ReadText(path);
			bool usesComponent = false;
			for (const auto header : k_Headers)
			{
				usesComponent = usesComponent || text.find(header) != std::string::npos;
			}
			// Look at the lines only when the file has one of the patterns
			bool suspect = text.find(k_PhysiqueName) != std::string::npos;
			for (const auto access : k_RawFileAccess)
			{
				suspect = suspect || (usesComponent && text.find(access) != std::string::npos);
			}
			if (!suspect)
			{
				continue;
			}
			for (const auto line : CodeLines(text))
			{
				if (line.find(k_PhysiqueName) != std::string_view::npos)
				{
					findings.push_back({path, "makes the name of a physique file: " + std::string(line)});
				}
				if (!usesComponent)
				{
					continue;
				}
				for (const auto access : k_RawFileAccess)
				{
					if (line.find(access) != std::string_view::npos)
					{
						findings.push_back({path, "reads or writes a file itself: " + std::string(line)});
					}
				}
			}
		}
	}
	return findings;
}
} // namespace

TEST(CreatureMindFileUsers, OnlyTheComponentOpensTheCreatureFiles)
{
	ASSERT_TRUE(std::filesystem::is_directory(SourceDir() / "components" / "creaturemind")) << SourceDir();
	for (const auto& finding : FindDirectUsers())
	{
		ADD_FAILURE() << finding.file.generic_string() << " " << finding.what
		              << ". Use the functions of components/creaturemind.";
	}
}
