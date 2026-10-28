#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Finds expressions that make two or more random draws in an order that C++ does not fix.

The operands of most operators and the arguments of a function call have no fixed order of evaluation. Two compilers
(for example MSVC for the desktop and clang for the web) can then draw the random numbers in different orders, and the
synced worlds part. This script reads the C++ files and shows each place where two draws meet in such an expression.
Draws in separate statements, in a braced list, on the two sides of &&, ||, ?:, the comma operator, an assignment,
<< or >>, or in a call chain, have a fixed order and are not shown.

The script finds draws by name (see DRAW_NAMES). It does not follow calls into other functions. With --deep it also
counts the calls of functions that draw, found by name only: use this for a sweep, because it gives false hits.

Use:
    check_draw_order.py [--root DIR] [paths...]     exit 1 if it finds a place that is not in the allow list

A place that the script shows but that is safe (for example, calls that only make the functions that draw) goes in tools/build/draw_order_allow.txt as "path:text", where text is a part of the statement.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass, field

# Names of calls that draw a random number. A call to an identifier that matches is a draw.
DRAW_NAMES = re.compile(
    r"^(?:"
    r"\w*[Rr]and(?:om)?\w*"  # GameRand, GameFloatRand, LocalRand, Random, RandomInBall, CrtRandom, ...
    r"|upTo|roll|draw|noise|NextValue"  # names of draw functions passed as parameters, the screen generator
    r"|\w*_distribution"  # std:: distributions
    r")$"
)
# Names that match DRAW_NAMES but do not draw.
NOT_DRAWS = re.compile(
    r"^(?:"
    r"(?:Set|Get|Make|Has|Is|Use|Own|Seed|Restart|Reset|Start|Stop|Save|Load|Read|Write|Print|Log|Trace|To|From"
    r"|Apply|Parse|Format)\w*"
    r"|\w*[Oo]perand\w*|\w*[Ss]trand\w*|\w*[Bb]rand\w*|\w*[Gg]rand(?!om)\w*|\w*Errand\w*"
    r"|\w*Stream\w*|\w*Seeds?|\w*Randomness|\w*RandomNumberManager\w*|CrtSrand|srand"
    r"|random_device|\w*Randomi[sz]ed|randomFrac|Random\w*Count|Random\w*Limit|IsRandom\w*"
    r")$"
)

KEYWORDS_BEFORE_PAREN = {
    "if", "while", "for", "switch", "return", "catch", "sizeof", "alignof", "decltype", "noexcept", "co_return",
    "co_yield", "throw", "case", "static_assert", "requires", "constexpr", "consteval", "else", "do", "and", "or",
    "not",
}

# Binary operators by precedence (low number binds less). True: the operands have a fixed order.
OPERATORS = {
    ";": (0, True),
    ",": (1, True),
    "=": (2, True), "+=": (2, True), "-=": (2, True), "*=": (2, True), "/=": (2, True), "%=": (2, True),
    "&=": (2, True), "|=": (2, True), "^=": (2, True), "<<=": (2, True), ">>=": (2, True),
    "?": (2, True), ":": (2, True),
    "||": (3, True), "or": (3, True),
    "&&": (4, True), "and": (4, True),
    "|": (5, False), "^": (6, False), "&": (7, False),
    "==": (8, False), "!=": (8, False),
    "<=>": (9, False),
    "<": (10, False), ">": (10, False), "<=": (10, False), ">=": (10, False),
    "<<": (11, True), ">>": (11, True),
    "+": (12, False), "-": (12, False),
    "*": (13, False), "/": (13, False), "%": (13, False),
    ".*": (14, False), "->*": (14, False),
}

TOKEN = re.compile(
    r"""
    (?P<ws>\s+)
  | (?P<num>(?:\d|\.\d)[\w.']*(?:[eEpP][+-]\d+\w*)?)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op><=>|<<=|>>=|->\*|\.\.\.|::|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\|=|\^=|\.\*|[-+*/%<>=!~&|^?:;,.#\[\](){}])
  | (?P<other>.)
    """,
    re.X,
)


def strip_source(text: str) -> str:
    """Puts spaces in place of comments, string and character literals and preprocessor lines, and keeps the lines."""
    out = []
    i = 0
    n = len(text)
    at_line_start = True
    while i < n:
        c = text[i]
        if at_line_start and c == "#":
            # A preprocessor line, with its continuations.
            j = i
            while j < n:
                if text[j] == "\n" and text[j - 1] != "\\":
                    break
                j += 1
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
            continue
        if c == "/" and text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
            continue
        if c == "/" and text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
            continue
        if c == "R" and text.startswith('R"', i):
            m = re.match(r'R"([^(\s]*)\(', text[i:])
            if m:
                end = text.find(")" + m.group(1) + '"', i)
                end = n if end < 0 else end + len(m.group(1)) + 2
                out.append('""' + re.sub(r"[^\n]", " ", text[i + 2 : end]))
                i = end
                continue
        if c in "\"'":
            # A literal; a ' between digits is a digit separator.
            if c == "'" and i > 0 and text[i - 1].isalnum() and i + 1 < n and text[i + 1].isalnum():
                out.append(" ")
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + " " * (min(j, n) - i - 1) + (c if j < n else ""))
            i = j + 1
            continue
        if c == "\n":
            at_line_start = True
        elif not c.isspace():
            at_line_start = False
        out.append(c)
        i += 1
    return "".join(out)


@dataclass
class Tok:
    kind: str
    text: str
    line: int


@dataclass
class Group:
    open: str
    line: int
    items: list = field(default_factory=list)
    block: bool = False
    call: bool = False


def tokenize(text: str) -> list[Tok]:
    toks = []
    line = 1
    for m in TOKEN.finditer(text):
        kind = m.lastgroup
        value = m.group()
        if kind != "ws":
            toks.append(Tok(kind, value, line))
        line += value.count("\n")
    return toks


CLOSE = {"(": ")", "[": "]", "{": "}"}


def is_operand_end(item) -> bool:
    if isinstance(item, Group):
        return item.open in "(["  # a {} group can end an operand too, but rarely; keep it simple
    return item.kind in ("id", "num") and item.text not in KEYWORDS_BEFORE_PAREN or item.text in ("]", ")")


def block_brace(prev) -> bool:
    """True if a { after prev opens a block of statements (not a braced list)."""
    if prev is None:
        return True
    if isinstance(prev, Group):
        # ) { : function body, if/for/while body, lambda body. ] { : lambda with no parameters.
        return prev.open in "(["
    if prev.kind == "op":
        return prev.text in (";", ":", "{", "}", "->")  # "->" is a lambda's return type (rare)
    if prev.kind == "id":
        return prev.text in (
            "else", "do", "try", "const", "noexcept", "override", "final", "mutable", "constexpr",
            "namespace",
        )
    return False


def type_body(items: list) -> bool:
    """True if the items before a { are the head of a namespace, struct, class, union or enum."""
    for item in reversed(items):
        if isinstance(item, Group):
            return False
        if item.text in ("struct", "class", "union", "enum", "namespace"):
            return True
        if item.kind in ("id", "num") or item.text in ("::", ":", ",", "<", ">"):
            continue
        return False
    return False


def build_tree(toks: list[Tok]) -> Group:
    root = Group("{", 1, block=True)
    stack = [root]
    for t in toks:
        top = stack[-1]
        if t.kind == "op" and t.text in CLOSE:
            prev = top.items[-1] if top.items else None
            g = Group(t.text, t.line)
            if t.text == "{":
                # A name before { is a type in a braced list (Foo{...}), except struct/class/enum/namespace bodies.
                g.block = block_brace(prev)
                if not g.block and isinstance(prev, Tok) and prev.kind == "id":
                    g.block = type_body(top.items)
            elif t.text == "(":
                g.call = prev is not None and (
                    (isinstance(prev, Group) and prev.open in "([")
                    or (isinstance(prev, Tok) and prev.kind == "id" and prev.text not in KEYWORDS_BEFORE_PAREN)
                    or (isinstance(prev, Tok) and prev.text == ">")
                )
            top.items.append(g)
            stack.append(g)
        elif t.kind == "op" and t.text in (")", "]", "}"):
            if len(stack) > 1:
                stack.pop()
        else:
            top.items.append(t)
    return root


@dataclass
class Draw:
    name: str
    line: int
    path: list  # list of (group, index) from the statement level down


# Names of other functions that draw (the --deep pass fills it).
EXTRA_DRAWERS: set[str] = set()


def is_draw_name(name: str) -> bool:
    return (DRAW_NAMES.match(name) is not None and NOT_DRAWS.match(name) is None) or name in EXTRA_DRAWERS


def function_bodies(group: Group, out: dict):
    """Adds name -> list of bodies for each function defined in the tree."""
    items = group.items
    for i, item in enumerate(items):
        if not isinstance(item, Group):
            continue
        if item.block:
            # name ( ... ) [const|noexcept|override|final|-> type ...] { body }
            k = i - 1
            while k >= 0 and isinstance(items[k], Tok) and items[k].text not in (";", "}", "{", "=", ")"):
                k -= 1
            if k >= 1 and isinstance(items[k], Group) and items[k].open == "(" and isinstance(items[k - 1], Tok):
                name = items[k - 1].text
                if items[k - 1].kind == "id" and name not in KEYWORDS_BEFORE_PAREN:
                    out.setdefault(name, []).append(item)
        function_bodies(item, out)


def calls_in(group: Group, out: set):
    items = group.items
    for i, item in enumerate(items):
        if isinstance(item, Group):
            calls_in(item, out)
        elif item.kind == "id" and i + 1 < len(items) and isinstance(items[i + 1], Group) and items[i + 1].open == "(":
            out.add(item.text)


def find_drawers(trees: list[Group], loose: bool = False) -> set[str]:
    """Names of functions that draw, directly or through other functions, found by name only."""
    bodies: dict = {}
    for tree in trees:
        function_bodies(tree, bodies)
    # A name is a drawer only when every function of that name draws, so that a common name (Update) of many
    # functions does not count.
    calls = {}
    for name, groups in bodies.items():
        per_body = []
        for g in groups:
            names = set()
            calls_in(g, names)
            per_body.append(names)
        calls[name] = per_body
    direct = lambda c: any(DRAW_NAMES.match(x) and not NOT_DRAWS.match(x) for x in c)
    test = (lambda bs, f: len(bs) <= 4 and any(f(c) for c in bs)) if loose else (lambda bs, f: all(f(c) for c in bs))
    drawers = {n for n, bs in calls.items() if test(bs, direct)}
    changed = True
    while changed:
        changed = False
        for n, bs in calls.items():
            if n not in drawers and test(bs, lambda c: direct(c) or c & drawers):
                drawers.add(n)
                changed = True
    return {d for d in drawers if not DRAW_NAMES.match(d) or NOT_DRAWS.match(d)}


def find_draws(items: list, path: list, out: list):
    for i, item in enumerate(items):
        if isinstance(item, Group):
            if item.block:
                continue  # a lambda body or a block runs later or apart
            find_draws(item.items, path + [(item, i)], out)
        elif item.kind == "id" and is_draw_name(item.text):
            nxt = items[i + 1] if i + 1 < len(items) else None
            if isinstance(nxt, Group) and nxt.open == "(":
                after = items[i + 2] if i + 2 < len(items) else None
                # random().GameRand(...) : the first call only gives the generator.
                if isinstance(after, Tok) and after.text in (".", "->") and not nxt.items:
                    continue
                out.append(Draw(item.text, item.line, path + [(None, i)]))
            elif item.text.endswith("_distribution"):
                out.append(Draw(item.text, item.line, path + [(None, i)]))


def split_commas(items: list) -> list[int]:
    """The index of the comma-separated part of each item."""
    part = 0
    parts = []
    for item in items:
        if isinstance(item, Tok) and item.text == ",":
            part += 1
        parts.append(part)
    return parts


def weakest_operator(items: list, lo: int, hi: int):
    """The binary operator of lowest precedence between items lo and hi (exclusive), or None."""
    best = None
    for k in range(lo + 1, hi):
        t = items[k]
        if not isinstance(t, Tok):
            continue
        if t.text not in OPERATORS:
            continue
        prev = items[k - 1]
        if t.text in ("+", "-", "*", "&") and not is_operand_end(prev):
            continue  # a unary operator
        if t.text in ("*", "&") and isinstance(prev, Tok) and prev.kind == "id" and k + 1 < len(items):
            # "Type* name" or "Type& name": a declaration, not an operator, when a name follows and then = or (.
            nxt = items[k + 1]
            nxt2 = items[k + 2] if k + 2 < len(items) else None
            if isinstance(nxt, Tok) and nxt.kind == "id" and isinstance(nxt2, Tok) and nxt2.text in ("=", ";", ","):
                continue
        prec, sequenced = OPERATORS[t.text]
        if best is None or prec < best[0]:
            best = (prec, sequenced, t.text)
    return best


def check_statement(items: list, results: list, file: str):
    draws: list[Draw] = []
    find_draws(items, [], draws)
    if len(draws) < 2:
        return
    reported = set()
    for x in range(len(draws)):
        for y in range(x + 1, len(draws)):
            reason = pair_reason(items, draws[x], draws[y])
            if reason:
                key = (draws[x].line, reason)
                if key in reported:
                    continue
                reported.add(key)
                results.append((file, draws[x].line, draws[x].name, draws[y].name, reason))


def pair_reason(stmt_items: list, a: Draw, b: Draw):
    # Walk down while both paths go through the same group.
    items = stmt_items
    group = None
    k = 0
    while k < len(a.path) and k < len(b.path) and a.path[k][1] == b.path[k][1] and a.path[k][0] is not None:
        group = a.path[k][0]
        items = group.items
        k += 1
    ia = a.path[k][1]
    ib = b.path[k][1]
    if ia == ib:
        return None  # a draw inside the arguments of another draw: the inner one runs first
    lo, hi = min(ia, ib), max(ia, ib)
    if group is not None and group.open == "{":
        parts = split_commas(items)
        if parts[lo] != parts[hi]:
            return None  # a braced list runs left to right
    if group is not None and group.open == "(" and group.call:
        parts = split_commas(items)
        if parts[lo] != parts[hi]:
            return "arguments of one call"
    # The items between, without the comma parts of a call (handled above).
    op = weakest_operator(items, lo, hi)
    if op is None:
        return None  # a call chain: the called object comes first
    prec, sequenced, text = op
    if sequenced:
        return None
    return f"operands of '{text}'"


def walk_block(group: Group, results: list, file: str):
    stmt: list = []
    for item in group.items:
        if isinstance(item, Group) and item.block:
            check_statement(stmt, results, file)
            stmt = []
            walk_block(item, results, file)
            continue
        stmt.append(item)
        if isinstance(item, Tok) and item.text == ";":
            check_statement(stmt, results, file)
            stmt = []
        # Blocks inside a statement (lambda bodies) are checked as blocks of their own.
        if isinstance(item, Group):
            for sub in nested_blocks(item):
                walk_block(sub, results, file)
    check_statement(stmt, results, file)


def nested_blocks(group: Group):
    for item in group.items:
        if isinstance(item, Group):
            if item.block:
                yield item
            else:
                yield from nested_blocks(item)


def load_allow(path: str) -> list[tuple[str, str]]:
    allow = []
    if os.path.exists(path):
        for raw in open(path, encoding="utf-8"):
            raw = raw.strip()
            if not raw or raw.startswith("#"):
                continue
            file, _, text = raw.partition(":")
            allow.append((file.replace("\\", "/"), text.strip()))
    return allow


SELF_TEST = r"""
namespace a::b {
void Bad() {
	const auto v = effect.RandomInBall() * effect.Random(speed);             // BAD
	Use(random.GameRand(3), random.GameRand(4));                            // BAD
	const glm::vec3 p(GameFloatRand(1.0f), GameFloatRand(2.0f), 0.0f);      // BAD
	auto q = glm::vec3(Random(-1.0f, 1.0f), Random(1.0f, 2.0f) + 2.0f, 1.0f); // BAD
	if (GameRand(2) == GameRand(3)) {}                                     // BAD
	x = 1.0f + (GameFloatRand(1.0f) - 0.5f) * (LocalFloatRand(1.0f) + 1.0f); // BAD
	Foo(a, [&] { return 1; }, ParticleFloatRand(1.0f), CrtRand());         // BAD
	Call(random(2), random(3));                                             // BAD
	const auto w = std::uniform_int_distribution<int>(0, 2)(gen) + roll(3);   // BAD
}
void Good() {
	const auto ball = effect.RandomInBall();
	const auto speed = effect.Random(randomSpeed);
	const glm::vec3 p {GameFloatRand(1.0f), GameFloatRand(2.0f), 0.0f};
	const auto list = Point {.x = GameRand(1), .y = GameRand(2)};
	if (GameRand(2) == 0 && GameRand(3) == 1) {}
	const auto r = GameRand(2) == 0 ? GameFloatRand(1.0f) : GameFloatRand(2.0f);
	a[GameRand(3)] = GameRand(4);
	x = GameRand(GameRand(5) + 1);
	Use([&] { return GameRand(1); }, [&] { return GameRand(2); });
	random().GameRand(3);
	Locator::gameRandom::value().GameRand(3) + Locator::gameRandom::value().SetSeeds(s);
	for (int i = GameRand(3); i < 4; i += GameRand(2)) {}
	out << GameRand(1) << GameRand(2);
	// GameRand(1) + GameRand(2)
	Log("GameRand(1) + GameRand(2)");
}
}
"""


def self_test() -> int:
    results = []
    lines = SELF_TEST.splitlines()
    walk_block(build_tree(tokenize(strip_source(SELF_TEST))), results, "self")
    found = {line for _, line, *_ in results}
    expected = {number for number, text in enumerate(lines, 1) if "// BAD" in text}
    ok = found == expected
    for number in sorted(found ^ expected):
        print(f"self-test: line {number} {'missed' if number in expected else 'wrongly found'}: {lines[number - 1].strip()}")
    print("self-test:", "pass" if ok else "FAIL")
    return 0 if ok else 1


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=os.path.normpath(os.path.join(here, "..", "..")))
    parser.add_argument("--allow", default=os.path.join(here, "draw_order_allow.txt"))
    parser.add_argument("--all", action="store_true", help="show also the places in the allow list")
    parser.add_argument("paths", nargs="*", default=["src", "apps", "components"])
    parser.add_argument("--deep", action="store_true", help="count calls of other functions that draw as draws too")
    parser.add_argument("--loose", action="store_true", help="with --deep: a name draws when any of its functions draws")
    parser.add_argument("--self-test", action="store_true", help="check the script on its own examples first")
    args = parser.parse_args()

    if self_test() != 0:
        return 2
    if args.self_test:
        return 0

    allow = load_allow(args.allow)
    results = []
    sources = {}
    trees = {}
    for top in args.paths:
        base_dir = os.path.join(args.root, top)
        walk = [(os.path.dirname(base_dir), [], [os.path.basename(base_dir)])] if os.path.isfile(base_dir) else os.walk(base_dir)
        for base, _, files in walk:
            for name in sorted(files):
                if not name.endswith((".cpp", ".h", ".hpp", ".inl", ".cc")):
                    continue
                path = os.path.join(base, name)
                rel = os.path.relpath(path, args.root).replace("\\", "/")
                text = open(path, encoding="utf-8", errors="replace").read()
                lines = text.splitlines()
                sources[rel] = lines
                trees[rel] = build_tree(tokenize(strip_source(text)))
    if args.deep:
        EXTRA_DRAWERS.update(find_drawers(list(trees.values()), args.loose))
        print(f"--deep: {len(EXTRA_DRAWERS)} other functions draw", file=sys.stderr)
    for rel, tree in trees.items():
        walk_block(tree, results, rel)

    found = 0
    for file, line, first, second, reason in sorted(set(results)):
        context = " ".join(" ".join(sources[file][max(0, line - 1) : line + 2]).split())
        allowed = any(file == f and t in context for f, t in allow)
        if allowed and not args.all:
            continue
        found += 0 if allowed else 1
        mark = " (allowed)" if allowed else ""
        print(f"{file}:{line}: {first} and {second}: {reason}{mark}: {context[:200]}")
    if found:
        print(f"{found} place(s) with two random draws in no fixed order. Put each draw in a statement of its own.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
