#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of a computer god's worship and building rules, in test/oracle/data:

- computer_worship_rest.txt: how much the dancers of a worship site want rest
- computer_worship_disciple.txt: which disciple a town wants, and how much the god wants to make it
- computer_worship_site_workers.txt: how much the god wants to give a building site workers
- computer_worship_site_wood.txt: how much the god wants to give a building site wood
- computer_worship_free_builder.txt: how good a free villager is as a builder for the god
- computer_worship_worker_drop.txt: where the hand puts a worker for a building site

Usage: python tools/oracle/cases/computer_worship_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
REST_DESIRE = 0x77C390  # thiscall (worship site: dance at +0xA0), RET; ST0
LIFE_DESIRE = 0x75BAE0  # a villager's want of life (vtable +0xB10)
NO_LIFE_DESIRE = 0x417310  # any other living's (vtable +0xB10): none
GET_LIFE = 0x402600  # vtable +0x11C: life at +0x48
DISCIPLE = 0x66AF60  # thiscall (computer player), arg node; RET 4; ST0, node amount at +0x78
SITE_WORKERS = 0x667E20  # thiscall (computer player), arg node; RET 4; ST0, node object at +0x94
SITE_WOOD = 0x667C90  # the same
FREE_BUILDER = 0x668510  # thiscall (computer player), args (villager, best*); RET 8; EAX
WORKER_DROP = 0x43C120  # thiscall (building site), arg (place*); RET 4

# Calls stubbed for the disciple
ANCESTOR_OBJECT = 0x655CC0  # thiscall (node): the first object above it
BREEDER_CANDIDATE = 0x668790  # thiscall, RET 4; ST0
FREE_BUILDER_IN_TOWN = 0x668650  # thiscall, RET 4; ST0
TOTEM_SHARE = 0x5614F0  # the totem's vtable +0x92C: the town's worship share; ST0
TOWN_VTABLE = 0x999AE4  # for the cast to a town

# Calls stubbed for the building sites
GET_PLAYER = 0x751D50  # the computer player's vtable +0x1C, stubbed: the player
IS_AVAILABLE = 0x401810  # the thing's vtable +0x2C: the flag at +0xA bit 0 is clear
GET_BUILDING = 0x43BC70
MAX_BUILDERS = 0x43BBD0
BUILDERS_NEEDED = 0x43BC00
IS_BUILT = 0x43BBB0
REPAIR_DESIRE = 0x43BE00
VILLAGER_DESIRE = 0x43BD70
WOOD_NEEDED = 0x43C5F0
SITE_RESOURCE = 0x43C5B0  # the site's vtable +0x98, stubbed: the wood at the site; RET 4
PLACE_PULL = 0x658630  # thiscall, arg place*; RET 4; ST0

# Calls stubbed for the free builder
VILLAGER_AVAILABLE = 0x752290
IS_CHILD = 0x55C970
IS_REACHABLE = 0x756460
FINAL_STATE = 0x751DD0
INFLUENCE_POWER = 0x657FD0
STATE_TABLE_FIELD4 = 0xDB9E7C  # the villager state table's second field, row 0
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table the distance reads, as the game does at start-up

# Calls stubbed for the worker drop
BUILDING_RADIUS = 0x638110  # the building's vtable +0x60, stubbed: its radius; ST0
GET_POSITION = 0x4019F0  # vtable +0x170: the map position at +0x14
FIND_TYPE = 0x6045C0  # thiscall (map position), args (type, after); RET 8: nothing found
MAP = 0xD0195C  # the map: its width and depth in cells at +0x59C8 and +0x59C4

TOWN_DESIRES = 13


class Rest:
    COLUMNS = ["count", "villagers", *[f"life_{i}" for i in range(8)], "groups", "desire"]

    def __init__(self, o: Oracle):
        self.o = o
        self.site = o.alloc(0x200)
        self.dance = o.alloc(0x100)
        self.villager_vtable = o.alloc(0xC00)
        self.other_vtable = o.alloc(0xC00)
        o.write_u32(self.villager_vtable + 0xB10, LIFE_DESIRE)
        o.write_u32(self.villager_vtable + 0x11C, GET_LIFE)
        o.write_u32(self.other_vtable + 0xB10, NO_LIFE_DESIRE)
        o.write_u32(self.site + 0xA0, self.dance)
        self.members = [o.alloc(0x100) for _ in range(8)]
        self.member_nodes = [o.alloc(8) for _ in range(8)]
        self.groups = [o.alloc(0x40) for _ in range(8)]
        self.group_nodes = [o.alloc(8) for _ in range(8)]

    def run(self, count: int, villagers: int, lives: list[int], groups: int) -> list[int]:
        o = self.o
        # The dancers split into groups in order: each group takes the next share of them
        sizes = [count // groups + (1 if i < count % groups else 0) for i in range(groups)]
        index = 0
        group_heads = []
        for g, size in enumerate(sizes):
            head = 0
            for m in range(index + size - 1, index - 1, -1):
                node = self.member_nodes[m]
                o.write_u32(node, head, self.members[m])
                head = node
                vtable = self.villager_vtable if villagers >> m & 1 else self.other_vtable
                o.write_u32(self.members[m], vtable)
                o.write_u32(self.members[m] + 0x48, lives[m])
            o.write_u32(self.groups[g] + 0x1C, head, size)
            group_heads.append(self.groups[g])
            index += size
        head = 0
        for g in range(groups - 1, -1, -1):
            o.write_u32(self.group_nodes[g], head, group_heads[g])
            head = self.group_nodes[g]
        o.write_u32(self.dance + 0x78, head)
        result = o.call(REST_DESIRE, this=self.site)
        assert result.popped == 0, result
        return [result.st0f]


def rest_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        count = rng.randint(1, 8)
        villagers = rng.getrandbits(8) & ((1 << count) - 1)
        if rng.random() < 0.6:
            villagers = (1 << count) - 1
        lives = [f2u(rng.choice([rng.uniform(0.0, 1.0), rng.uniform(0.9, 1.0), 1.0, 0.0, rng.uniform(0.0, 0.3)]))
                 for _ in range(8)]
        lives = [life if i < count else 0 for i, life in enumerate(lives)]
        groups = rng.randint(1, count)
        rows.append((count, villagers, *lives, groups))
    return rows


class Disciple:
    COLUMNS = [*[f"disciples_{i}" for i in range(13)], "adults", "has_totem", "share", *[f"desire_{i}" for i in range(13)],
               "breeder", "finder", "value", "amount"]

    def __init__(self, o: Oracle):
        self.o = o
        self.player = o.alloc(0x100)
        self.node = o.alloc(0x100)
        self.town = o.alloc(0xA00)
        self.totem = o.alloc(0x40)
        self.totem_vtable = o.alloc(0xA00)
        o.write_u32(self.town, TOWN_VTABLE)
        o.write_u32(self.totem, self.totem_vtable)
        o.write_u32(self.totem_vtable + 0x92C, TOTEM_SHARE)
        o.stub(ANCESTOR_OBJECT, eax=self.town, pop=0)

    def run(self, counts: list[int], adults: int, has_totem: int, share: int, desires: list[int], breeder: int,
            finder: int) -> list[int]:
        o = self.o
        o.write(self.town + 0x6D8, bytes(counts))
        o.write_u32(self.town + 0x618, adults)
        o.write_u32(self.town + 0x740, self.totem if has_totem else 0)
        o.stub(TOTEM_SHARE, st0=f32.of_bits(share), pop=0)
        o.stub(BREEDER_CANDIDATE, st0=f32.of_bits(breeder), pop=4)
        o.stub(FREE_BUILDER_IN_TOWN, st0=f32.of_bits(finder), pop=4)
        for i in range(TOWN_DESIRES):
            o.write_u32(self.town + 0x14C + i * 4, desires[i])
            o.write_u32(self.town + 0x108 + i * 4, 0)
            o.write_u32(self.town + 0xC4 + i * 4, 0)
        o.write_u32(self.node + 0x78, 0xFFFF)
        result = o.call(DISCIPLE, this=self.player, args=[self.node])
        assert result.popped == 4, result
        return [result.st0f, o.read_u32(self.node + 0x78)[0]]


def disciple_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        counts = [0] + [rng.choice([0, 0, 1, 2, 3, rng.randint(0, 12)]) for _ in range(12)]
        adults = rng.choice([0, rng.randint(1, 10), rng.randint(5, 80), rng.randint(0, 300)])
        has_totem = rng.randint(0, 1)
        share = f2u(rng.choice([0.0, 1.0, rng.uniform(0.0, 1.0), 0.5]))
        desires = [f2u(rng.choice([rng.uniform(-1.0, 1.0), rng.uniform(0.0, 1.0), 0.0])) for _ in range(TOWN_DESIRES)]
        breeder = f2u(rng.choice([0.0, 1.0, 0.0]))
        finder = f2u(rng.choice([1.0, 0.0, rng.uniform(0.0, 1.0)]))
        rows.append((*counts, adults, has_totem, share, *desires, breeder, finder))
    return rows


class Sites:
    """The site rules on a player with one town that has one building site"""

    def __init__(self, o: Oracle):
        self.o = o
        self.player = o.alloc(0x100)
        self.player_vtable = o.alloc(0x100)
        self.gplayer = o.alloc(0xB00)
        self.town = o.alloc(0xA00)
        self.site = o.alloc(0x100)
        self.site_vtable = o.alloc(0x100)
        self.building = o.alloc(0x100)
        self.node = o.alloc(0x100)
        list_node = o.alloc(8)
        o.write_u32(self.player, self.player_vtable)
        o.write_u32(self.player_vtable + 0x1C, GET_PLAYER)
        o.stub(GET_PLAYER, eax=self.gplayer, pop=0)
        o.write_u32(self.gplayer + 0xA50, self.town)
        o.write_u32(self.town + 0x790, list_node)
        o.write_u32(self.town + 0x75C, 0)
        o.write_u32(list_node, 0, self.site)
        o.write_u32(self.site, self.site_vtable)
        o.write_u32(self.site_vtable + 0x2C, IS_AVAILABLE)
        o.write_u32(self.site_vtable + 0x98, SITE_RESOURCE)
        o.stub(GET_BUILDING, eax=self.building, pop=0)

    def common(self, built: int, repair: int, pull: int) -> None:
        o = self.o
        o.stub(IS_BUILT, eax=built, pop=0)
        o.stub(REPAIR_DESIRE, st0=f32.of_bits(repair), pop=0)
        o.stub(PLACE_PULL, st0=f32.of_bits(pull), pop=4)
        o.write_u32(self.node + 0x94, 0)

    def workers(self, max_builders: int, needed: int, built: int, repair: int, desire: int, builders: int,
                pull: int) -> list[int]:
        o = self.o
        self.common(built, repair, pull)
        o.stub(MAX_BUILDERS, eax=max_builders & 0xFFFFFFFF, pop=0)
        o.stub(BUILDERS_NEEDED, eax=needed & 0xFFFFFFFF, pop=0)
        o.stub(VILLAGER_DESIRE, st0=f32.of_bits(desire), pop=0)
        o.write(self.town + 0x6DC, bytes([builders]))
        result = o.call(SITE_WORKERS, this=self.player, args=[self.node])
        assert result.popped == 4, result
        return [result.st0f, int(o.read_u32(self.node + 0x94)[0] == self.site)]

    def wood(self, needed: int, built: int, repair: int, wood: int, pull: int) -> list[int]:
        o = self.o
        self.common(built, repair, pull)
        o.stub(WOOD_NEEDED, st0=f32.of_bits(needed), pop=0)
        o.stub(SITE_RESOURCE, eax=wood, pop=4)
        result = o.call(SITE_WOOD, this=self.player, args=[self.node])
        assert result.popped == 4, result
        return [result.st0f, int(o.read_u32(self.node + 0x94)[0] == self.site)]


WORKERS_COLUMNS = ["max_builders", "needed", "built", "repair", "desire", "builders", "pull", "value", "chosen"]
WOOD_COLUMNS = ["needed", "built", "repair", "wood", "pull", "value", "chosen"]


def worker_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        max_builders = rng.choice([0, -1, rng.randint(1, 12), rng.randint(1, 12), rng.randint(1, 40), rng.randint(1, 40)])
        needed = rng.choice([0, -2, rng.randint(1, 12), rng.randint(1, 12), rng.randint(1, 12)])
        built = rng.choice([0, 0, 0, 1])
        repair = f2u(rng.choice([0.0, rng.uniform(0.0, 1.0), rng.uniform(0.0, 1.0), -0.25]))
        desire = f2u(rng.choice([rng.uniform(0.0, 1.0), 1.0, 0.0, rng.uniform(-1.0, 2.0)]))
        builders = rng.choice([0, rng.randint(0, 15), rng.randint(0, 255)])
        pull = f2u(rng.choice([1.0, rng.uniform(0.0, 1.0), 0.0]))
        rows.append((max_builders & 0xFFFFFFFF, needed & 0xFFFFFFFF, built, repair, desire, builders, pull))
    return rows


def wood_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        needed = f2u(rng.choice([0.0, -1.0, rng.uniform(0.0, 50.0), rng.uniform(0.0, 3000.0), float(rng.randint(1, 400))]))
        built = rng.choice([0, 0, 1])
        repair = f2u(rng.choice([0.0, rng.uniform(0.0, 1.0)]))
        wood = rng.choice([0, rng.randint(0, 50), rng.randint(0, 5000), rng.randint(0, 0x7FFFFFFF)])
        pull = f2u(rng.choice([1.0, rng.uniform(0.0, 1.0), 0.0]))
        rows.append((needed, built, repair, wood, pull))
    return rows


class FreeBuilder:
    COLUMNS = ["hand_x", "hand_z", "villager_x", "villager_z", "power", "life", "pull", "best", "result", "best_out"]

    def __init__(self, o: Oracle):
        self.o = o
        o.call(MAKE_INVERSE_SQRT_TABLE)
        self.player = o.alloc(0x100)
        self.state = o.alloc(0x100)
        self.villager = o.alloc(0x200)
        self.vtable = o.alloc(0xC00)
        self.best = o.alloc(8)
        o.write_u32(self.player + 0x34, self.state)
        o.write_u32(self.villager, self.vtable)
        o.write_u32(self.vtable + 0xAF8, IS_CHILD)
        o.write_u32(self.vtable + 0x530, IS_REACHABLE)
        o.write_u32(self.vtable + 0xB04, FINAL_STATE)
        o.write_u32(self.vtable + 0x11C, GET_LIFE)
        o.write_u32(self.vtable + 0x170, GET_POSITION)
        o.stub(VILLAGER_AVAILABLE, eax=1, pop=0)
        o.stub(IS_CHILD, eax=0, pop=0)
        o.stub(IS_REACHABLE, eax=1, pop=0)
        o.stub(FINAL_STATE, eax=0, pop=0)
        o.write_u32(STATE_TABLE_FIELD4, 0xFFFFFFFF)

    def run(self, hx: int, hz: int, vx: int, vz: int, power: int, life: int, pull: int, best: int) -> list[int]:
        o = self.o
        o.write_u32(self.state + 0x14, hx, hz, 0)
        o.write_u32(self.villager + 0x14, vx, vz, 0)
        o.write_u32(self.villager + 0x48, life)
        o.stub(INFLUENCE_POWER, st0=f32.of_bits(power), pop=0)
        o.stub(PLACE_PULL, st0=f32.of_bits(pull), pop=4)
        o.write_u32(self.best, best)
        result = o.call(FREE_BUILDER, this=self.player, args=[self.villager, self.best])
        assert result.popped == 8, result
        return [result.eax & 0xFF, o.read_u32(self.best)[0]]


def free_builder_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        hx = rng.randint(0x100000, 0x1F00000)
        hz = rng.randint(0x100000, 0x1F00000)
        reach = rng.choice([rng.randint(0, 0x40000), rng.randint(0, 0x400000), 0])
        vx = hx + rng.randint(-reach, reach)
        vz = hz + rng.randint(-reach, reach)
        power = f2u(rng.choice([rng.uniform(1.0, 2000.0), 100.0, rng.uniform(100.0, 600.0)]))
        life = f2u(rng.choice([1.0, rng.uniform(0.0, 1.0)]))
        pull = f2u(rng.choice([1.0, rng.uniform(0.0, 1.0), 0.0]))
        best = f2u(rng.choice([0.0, 0.0, rng.uniform(0.0, 1.0)]))
        rows.append((hx, hz, vx, vz, power, life, pull, best))
    return rows


class WorkerDrop:
    COLUMNS = ["x", "z", "altitude", "radius", "out_x", "out_z", "out_altitude"]

    def __init__(self, o: Oracle):
        self.o = o
        self.site = o.alloc(0x100)
        self.building = o.alloc(0x100)
        self.vtable = o.alloc(0x200)
        self.out = o.alloc(0x10)
        world_map = o.alloc(0x6000)
        o.write_u32(world_map + 0x59C8, 512)
        o.write_u32(world_map + 0x59C4, 512)
        o.write_u32(MAP, world_map)
        o.write_u32(self.building, self.vtable)
        o.write_u32(self.vtable + 0x60, BUILDING_RADIUS)
        o.write_u32(self.vtable + 0x170, GET_POSITION)
        o.stub(GET_BUILDING, eax=self.building, pop=0)
        o.stub(FIND_TYPE, eax=0, pop=8)

    def run(self, x: int, z: int, altitude: int, radius: int) -> list[int]:
        o = self.o
        o.write_u32(self.building + 0x14, x, z, altitude)
        o.stub(BUILDING_RADIUS, st0=f32.of_bits(radius), pop=0)
        o.write_u32(self.out, 0, 0, 0)
        result = o.call(WORKER_DROP, this=self.site, args=[self.out])
        assert result.popped == 4, result
        return o.read_u32(self.out, 3)


def worker_drop_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(2000):
        edge = rng.random() < 0.3
        x = rng.choice([rng.randint(0, 0x2000000), rng.randint(0, 0x80000)]) if edge else rng.randint(0x100000, 0x1F00000)
        z = rng.choice([rng.randint(0, 0x2000000), 0x1FF0000 + rng.randint(0, 0x20000)]) if edge else \
            rng.randint(0x100000, 0x1F00000)
        altitude = f2u(rng.choice([0.0, rng.uniform(-5.0, 5.0)]))
        radius = f2u(rng.choice([rng.uniform(0.0, 30.0), 5.0, rng.uniform(0.0, 100.0)]))
        rows.append((x, z, altitude, radius))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x66AF60)
    with Oracle() as oracle:
        rest = Rest(oracle)
        rows = [[*case, *rest.run(case[0], case[1], list(case[2:10]), case[10])] for case in rest_cases(rng)]
        n = write_pinned(out / "computer_worship_rest.txt", "How much the dancers of a worship site want rest",
                         ["Function: thiscall on the worship site (dance at +0xA0). The dancers go into groups in order.",
                          "villagers: bit i set when dancer i is a villager (else a living that wants no life).",
                          "Lives and the desire are float bits."], Rest.COLUMNS, rows)
        print(f"computer_worship_rest: {n} rows")
        oracle.reset()
        disciple = Disciple(oracle)
        rows = [[*case, *disciple.run(list(case[0:13]), case[13], case[14], case[15], list(case[16:29]), case[29],
                                      case[30])] for case in disciple_cases(rng)]
        n = write_pinned(out / "computer_worship_disciple.txt", "The disciple that a town wants from a computer god",
                         ["Function: thiscall on the computer player, arg the node. The town is the node's ancestor.",
                          "disciples_i: the count of each work. desire_i: each desire, with no boosts.",
                          "breeder and finder are the results of the breeder search and the free builder search.",
                          "amount: the node's amount after the call (0xFFFF when it did not change). Floats are bits."],
                         Disciple.COLUMNS, rows)
        print(f"computer_worship_disciple: {n} rows, {sum(1 for r in rows if r[-1] != 0xFFFF)} chosen")
        oracle.reset()
        sites = Sites(oracle)
        rows = [[*case, *sites.workers(*case)] for case in worker_cases(rng)]
        n = write_pinned(out / "computer_worship_site_workers.txt", "How much a computer god wants to give a site workers",
                         ["Function: thiscall on the computer player, arg the node; one town with one building site.",
                          "builders: the town's builder disciples. Counts are words; floats are bits.",
                          "chosen: 1 when the node's object is the site."], WORKERS_COLUMNS, rows)
        print(f"computer_worship_site_workers: {n} rows, {sum(r[-1] for r in rows)} chosen")
        rows = [[*case, *sites.wood(*case)] for case in wood_cases(rng)]
        n = write_pinned(out / "computer_worship_site_wood.txt", "How much a computer god wants to give a site wood",
                         ["Function: thiscall on the computer player, arg the node; one town with one building site.",
                          "wood: the wood at the site (a word). Floats are bits. chosen: 1 when the node's object is",
                          "the site."], WOOD_COLUMNS, rows)
        print(f"computer_worship_site_wood: {n} rows, {sum(r[-1] for r in rows)} chosen")
        oracle.reset()
        builder = FreeBuilder(oracle)
        rows = [[*case, *builder.run(*case)] for case in free_builder_cases(rng)]
        n = write_pinned(out / "computer_worship_free_builder.txt", "How good a free villager is as a builder",
                         ["Function: thiscall on the computer player, args (villager, best*); a free adult villager.",
                          "Positions are map units; floats are bits. result: EAX; best_out: the best after the call."],
                         FreeBuilder.COLUMNS, rows)
        print(f"computer_worship_free_builder: {n} rows, {sum(r[-2] for r in rows)} better")
        oracle.reset()
        drop = WorkerDrop(oracle)
        rows = [[*case, *drop.run(*case)] for case in worker_drop_cases(rng)]
        n = write_pinned(out / "computer_worship_worker_drop.txt", "Where the hand puts a worker for a building site",
                         ["Function: thiscall on the building site, arg place*; a map of 512 by 512 cells with no wood.",
                          "Positions are map units; the altitude and the radius are float bits."],
                         WorkerDrop.COLUMNS, rows)
        print(f"computer_worship_worker_drop: {n} rows")


if __name__ == "__main__":
    main()
