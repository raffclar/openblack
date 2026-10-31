#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the comparison of two saved worlds: the blocks, the entities block and the systems block"""

import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import save_diff  # noqa: E402


def u32(value):
    return struct.pack("<I", value)


def name(text):
    data = text.encode()
    return u32(len(data)) + data


def entities_block(entities, components):
    """An entities block: components is [(name, [fields], [(entity, [field bytes])])]"""
    data = b"OBEW" + u32(1) + u32(len(entities)) + u32(len(entities)) + b"".join(u32(e) for e in entities)
    data += u32(len(components))
    for component, fields, rows in components:
        block = u32(len(fields)) + b"".join(name(f) for f in fields) + u32(len(rows))
        for entity, values in rows:
            block += u32(entity) + b"".join(u32(len(v)) + v for v in values)
        data += name(component) + u32(len(block)) + block
    return data


def sections_block(sections):
    return u32(len(sections)) + b"".join(name(n) + u32(len(b)) + b for n, b in sections)


def world(blocks, attempts=0):
    data = b"OBSV" + u32(4) + u32(attempts) + u32(len(blocks))
    for tag, block in blocks:
        data += tag.encode() + u32(len(block)) + block
    return data


def clock_block(synced_seed=7, draws=100, remainder=0, local_seed=3, streams=()):
    """A clock block (TIME) as the game writes it"""
    data = struct.pack("<f", 1.0) + b"\x00" + struct.pack("<i", remainder) + u32(synced_seed) + u32(local_seed)
    data += b"\x00" + b"\x00" + u32(1) + u32(len(streams)) + b"".join(name(text) for text in streams)
    return data + name("general") + struct.pack("<Q", draws)


def state_map(sections, parts=(), turn=250, seed=7, draws=100):
    """The map of a world state: sections is [(name, size, [(first, last)])]"""
    return {"turn": turn, "syncedSeed": seed, "syncedDraws": draws, "waitingSounds": 0,
            "presentationParts": [{"component": c, "field": f, "marker": m} for c, f, m in parts],
            "sections": [{"name": n, "size": size, "presentation": [list(r) for r in ranges]}
                         for n, size, ranges in sections]}


# The presentation parts of these tests: a whole component, a field, and a field only of the entities with a marker
PARTS = [("Sprite", "", ""), ("Animal", "clipPlace", ""), ("Transform", "rotation", "Animal")]


class SyncedCompareTests(unittest.TestCase):
    def entities(self, clip=b"\x00", rotation=b"\x00", position=b"\x00", sprite=b"\x00"):
        return entities_block([1, 2], [("Animal", ["clipPlace", "speed"], [(1, [clip, b"\x05"])]),
                                       ("Transform", ["position", "rotation"],
                                        [(1, [b"\x00", rotation]), (2, [position, rotation])]),
                                       ("Sprite", ["frame"], [(2, [sprite])])])

    def compare(self, blocks_a, blocks_b, map_a=None, map_b=None):
        map_a = map_a or state_map([], PARTS)
        map_b = map_b or state_map([], PARTS)
        return save_diff.compare_synced(world(blocks_a), map_a, world(blocks_b), map_b)

    def test_the_presentation_parts_of_the_entities_do_not_count(self):
        first = self.entities()
        second = self.entities(clip=b"\x01", sprite=b"\x09")
        self.assertEqual(self.compare([("ECSW", first)], [("ECSW", second)]), {})

    def test_a_marker_covers_only_the_entities_that_have_it(self):
        # Entity 1 is an animal, so its rotation only shows the world. Entity 2 is not, so its rotation is synced.
        first = self.entities()
        second = entities_block([1, 2], [("Animal", ["clipPlace", "speed"], [(1, [b"\x00", b"\x05"])]),
                                         ("Transform", ["position", "rotation"],
                                          [(1, [b"\x00", b"\x04"]), (2, [b"\x00", b"\x04"])]),
                                         ("Sprite", ["frame"], [(2, [b"\x00"])])])
        result = self.compare([("ECSW", first)], [("ECSW", second)])
        values = result["ECSW"]["components"]["Transform"]["values"]
        self.assertEqual(list(values), ["rotation"])
        self.assertEqual(values["rotation"]["entities"], 1)
        self.assertEqual(values["rotation"]["examples"][0]["entity"], "2v0")

    def test_a_synced_field_that_differs_is_named(self):
        result = self.compare([("ECSW", self.entities())], [("ECSW", self.entities(position=b"\x03"))])
        self.assertEqual(list(result), ["ECSW"])
        self.assertEqual(list(result["ECSW"]["components"]["Transform"]["values"]), ["position"])

    def test_the_presentation_bytes_of_a_section_do_not_count(self):
        first = sections_block([("magic", b"\x01\x02\x03\x04")])
        second = sections_block([("magic", b"\x01\x09\x09\x04")])
        same = state_map([("magic", 4, [(1, 3)])])
        self.assertEqual(self.compare([("SYST", first)], [("SYST", second)], same, same), {})
        synced = state_map([("magic", 4, [(1, 2)])])
        result = self.compare([("SYST", first)], [("SYST", second)], synced, synced)
        self.assertEqual(list(result["SYST"]), ["magic"])

    def test_the_presentation_bytes_of_the_context_do_not_count(self):
        first, second = b"\x01\x02\x03", b"\x01\x09\x03"
        same = dict(state_map([]), contextPresentation=[[1, 2]])
        self.assertEqual(self.compare([("RCTX", first)], [("RCTX", second)], same, same), {})
        result = self.compare([("RCTX", first)], [("RCTX", second)])
        self.assertEqual(list(result), ["RCTX"])

    def test_a_section_of_presentation_only_that_the_server_does_not_write_is_the_same(self):
        server = sections_block([("magic", b"\x01")])
        client = sections_block([("magic", b"\x01"), ("localParticles", b"\x05\x06")])
        result = self.compare([("SYST", server)], [("SYST", client)], state_map([("magic", 1, [])]),
                              state_map([("magic", 1, []), ("localParticles", 2, [(0, 2)])]))
        self.assertEqual(result, {})

    def test_a_synced_section_that_only_one_computer_writes_differs(self):
        server = sections_block([("magic", b"\x01")])
        client = sections_block([("magic", b"\x01"), ("fire", b"\x05")])
        result = self.compare([("SYST", server)], [("SYST", client)], state_map([("magic", 1, [])]),
                              state_map([("magic", 1, []), ("fire", 1, [])]))
        self.assertEqual(list(result["SYST"]), ["fire"])

    def test_a_map_that_does_not_give_a_section_is_refused(self):
        block = sections_block([("magic", b"\x01\x02")])
        with self.assertRaises(ValueError):
            self.compare([("SYST", block)], [("SYST", block)], state_map([("magic", 1, [])]),
                         state_map([("magic", 2, [])]))

    def test_the_camera_and_the_local_values_of_the_clock_do_not_count(self):
        first = [("CAMR", b"\x01"), ("TIME", clock_block(remainder=5, local_seed=1, streams=["a"]))]
        second = [("CAMR", b"\x02"), ("TIME", clock_block(remainder=9, local_seed=2, streams=["b"]))]
        self.assertEqual(self.compare(first, second), {})

    def test_the_synced_seed_and_its_draws_count(self):
        result = self.compare([("TIME", clock_block(draws=100))], [("TIME", clock_block(draws=101))],
                              state_map([], PARTS, draws=100), state_map([], PARTS, draws=101))
        self.assertEqual(result["TIME"], {"syncedDraws": [100, 101]})
        self.assertEqual(result["syncedDraws"], [100, 101])

    def test_other_blocks_count_whole(self):
        result = self.compare([("SOBJ", b"\x01\x02")], [("SOBJ", b"\x01\x03")])
        self.assertEqual(result["SOBJ"]["first"], 1)
        result = self.compare([("SOBJ", b"\x01")], [])
        self.assertEqual(result["SOBJ"], {"only_in": "a"})

    def test_world_states_of_other_turns_differ(self):
        result = self.compare([], [], state_map([], turn=250), state_map([], turn=251))
        self.assertEqual(result["turn"], [250, 251])


class SaveDiffTests(unittest.TestCase):
    def test_same_files_give_nothing(self):
        data = world([("TIME", b"\x01\x02"), ("SYST", sections_block([("fire", b"\x00")]))])
        self.assertEqual(save_diff.compare_files(data, data), {})

    def test_field_that_differs_is_named_with_its_entity(self):
        first = entities_block([1, 2], [("Transform", ["position", "scale"], [(1, [b"\x00" * 4, b"\x01"]),
                                                                              (2, [b"\x00" * 4, b"\x01"])])])
        second = entities_block([1, 2], [("Transform", ["position", "scale"], [(1, [b"\x00" * 4, b"\x01"]),
                                                                               (2, [b"\x00" * 4, b"\x02"])])])
        result = save_diff.compare_files(world([("ECSW", first)]), world([("ECSW", second)]))
        values = result["ECSW"]["detail"]["components"]["Transform"]["values"]
        self.assertEqual(list(values), ["scale"])
        self.assertEqual(values["scale"]["entities"], 1)
        self.assertEqual(values["scale"]["examples"][0]["entity"], "2v0")

    def test_rows_in_another_order_are_told(self):
        rows = [(1, [b"\x00"]), (2, [b"\x00"])]
        first = entities_block([1, 2], [("Tag", ["value"], rows)])
        second = entities_block([1, 2], [("Tag", ["value"], list(reversed(rows)))])
        result = save_diff.compare_files(world([("ECSW", first)]), world([("ECSW", second)]))
        self.assertTrue(result["ECSW"]["detail"]["components"]["Tag"]["rows"]["same_set_other_order"])

    def test_section_that_differs_is_named(self):
        first = sections_block([("fire", b"\x00\x00"), ("snow", b"\x01")])
        second = sections_block([("fire", b"\x00\x00\x05"), ("snow", b"\x01")])
        result = save_diff.compare_files(world([("SYST", first)]), world([("SYST", second)]))
        detail = result["SYST"]["detail"]
        self.assertEqual(list(detail), ["fire"])
        self.assertEqual(detail["fire"]["sizes"], [2, 3])

    def test_other_block_gives_the_first_place(self):
        result = save_diff.compare_files(world([("TIME", b"\x00\x00\x00\x00\x07")]),
                                         world([("TIME", b"\x00\x00\x00\x00\x08")]))
        self.assertEqual(result["TIME"]["bytes"]["first"], 4)

    def test_attempts_differ_in_the_head(self):
        result = save_diff.compare_files(world([], attempts=1), world([], attempts=0))
        self.assertIn("head", result)


if __name__ == "__main__":
    unittest.main()
