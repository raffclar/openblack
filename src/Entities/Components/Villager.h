#include <cstdint>

enum class VillageEthnicities
{
	GLOBAL,
	AZTEC,
	CELTIC,
	EGYPTIAN,
	GREEK,
	INDIAN,
	JAPANESE,
	NORSE,
	TIBETAN
};

enum class VillagerTypes
{
	FARMER,
	FISHERMAN,
	FORESTER,
	BREEDER,
	SHEPHERD,
	MISSIONARY
};

enum class VillagerTasks
{
	IDLE
};

struct Villager
{
	uint32_t health;
	uint32_t age;
	uint32_t hunger;
};
