#include <Entities/Component.h>

namespace OpenBlack
{
namespace Entities
{

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

class Villager: Component
{
  private:
	uint32_t _age;
	uint32_t _health;
	uint32_t _hunger;
	VillagerTasks _task;
};
} // namespace Entities
} // namespace OpenBlack
