import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import checksum_compare  # noqa: E402


def lines(pairs):
    return [f"[info] turn {turn} checksum {value:016x}" for turn, value in pairs]


class Comparing(unittest.TestCase):
    def test_the_same_turns_are_the_same(self):
        first = checksum_compare.checksums(lines([(1, 10), (2, 20), (3, 30)]))
        second = checksum_compare.checksums(lines([(1, 10), (2, 20)]))
        result = checksum_compare.compare(first, second)
        self.assertTrue(result["same"])
        self.assertEqual((result["common"], result["last_common"], result["first_different"]), (2, 2, None))

    def test_the_first_different_turn_is_named(self):
        first = checksum_compare.checksums(lines([(1, 10), (2, 20), (3, 30), (4, 40)]))
        second = checksum_compare.checksums(lines([(1, 10), (2, 21), (3, 31), (4, 40)]))
        result = checksum_compare.compare(first, second)
        self.assertFalse(result["same"])
        self.assertEqual((result["first_different"], result["different"]), (2, 2))

    def test_logs_with_no_common_turn_are_not_the_same(self):
        self.assertFalse(checksum_compare.compare({}, {1: "a"})["same"])

    def test_the_first_line_of_a_turn_counts(self):
        turns = checksum_compare.checksums(lines([(5, 1), (5, 2)]))
        self.assertEqual(turns[5], f"{1:016x}")


if __name__ == "__main__":
    unittest.main()
