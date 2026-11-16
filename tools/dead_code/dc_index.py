"""Index the definitions and the uses of the project with libclang.

    python tools/dead_code/dc_index.py <compile_commands.json> <repo root> <out.json> [jobs]

For each translation unit under src/, components/, tools/, test/ and apps/ the script records, keyed by USR:
  defs: file, line, kind, name, qualified name, virtual flag and the base classes of a type.
  refs: for each use, the target, the file of the use and the USR of the definition that holds the use.
A call, a use by address, a constructor call, a type name, a base class, a template argument and an override all count as a
use. A use that clang cannot resolve (a call in a template or in a generic lambda) is recorded by name as "n:<name>".
A use of a template specialization is also recorded by name, so that the template itself shows as used.

Set DC_LIBCLANG to the libclang library (default C:/lib/LLVM/bin/libclang.dll). Set DC_PYLIB to the folder that holds the
Python "clang" package when it is not installed.
"""

import ctypes
import json
import os
import shlex
import sys
from concurrent.futures import ProcessPoolExecutor

if os.environ.get("DC_PYLIB"):
    sys.path.insert(0, os.environ["DC_PYLIB"])
from clang import cindex  # noqa: E402

cindex.Config.set_library_file(os.environ.get("DC_LIBCLANG", "C:/lib/LLVM/bin/libclang.dll"))
CK = cindex.CursorKind

DEF_KINDS = {
    CK.FUNCTION_DECL, CK.CXX_METHOD, CK.CONSTRUCTOR, CK.DESTRUCTOR, CK.CONVERSION_FUNCTION,
    CK.FUNCTION_TEMPLATE, CK.CLASS_DECL, CK.STRUCT_DECL, CK.CLASS_TEMPLATE, CK.ENUM_DECL, CK.VAR_DECL,
    CK.TYPE_ALIAS_DECL, CK.TYPEDEF_DECL,
}
FUNC_KINDS = {CK.FUNCTION_DECL, CK.CXX_METHOD, CK.CONSTRUCTOR, CK.DESTRUCTOR, CK.CONVERSION_FUNCTION,
              CK.FUNCTION_TEMPLATE}
TYPE_KINDS = {CK.CLASS_DECL, CK.STRUCT_DECL, CK.CLASS_TEMPLATE, CK.ENUM_DECL}
UNRES_KINDS = {CK.MEMBER_REF_EXPR, CK.DECL_REF_EXPR, CK.CALL_EXPR, CK.OVERLOADED_DECL_REF, CK.MEMBER_REF}
SCOPES = ("src/", "components/", "tools/", "test/", "apps/")

lib = cindex.conf.lib
lib.clang_getNumOverloadedDecls.argtypes = [cindex.Cursor]
lib.clang_getNumOverloadedDecls.restype = ctypes.c_uint
lib.clang_getOverloadedDecl.argtypes = [cindex.Cursor, ctypes.c_uint]
lib.clang_getOverloadedDecl.restype = cindex.Cursor


def rel(root, path):
    if not path:
        return None
    p = os.path.normcase(os.path.abspath(path)).replace("\\", "/")
    r = os.path.normcase(os.path.abspath(root)).replace("\\", "/") + "/"
    if p.startswith(r):
        return p[len(r):]
    return None


def is_def(c):
    if c.kind in FUNC_KINDS or c.kind in TYPE_KINDS:
        return c.is_definition()
    return True


def qualname(c):
    parts = []
    p = c
    while p is not None and p.kind != CK.TRANSLATION_UNIT:
        if p.spelling:
            parts.append(p.spelling)
        p = p.semantic_parent
    return "::".join(reversed(parts))


def record_def(ch, fr, defs):
    """Adds the definition that the cursor makes, and returns its USR (or None)."""
    usr = ch.get_usr()
    if not usr or not is_def(ch):
        return None
    # A member variable is a field: only the static variables and the variables of a namespace matter.
    if ch.kind == CK.VAR_DECL and ch.semantic_parent.kind not in (
            CK.TRANSLATION_UNIT, CK.NAMESPACE, CK.CLASS_DECL, CK.STRUCT_DECL):
        return None
    d = defs.setdefault(usr, {
        "file": fr, "line": ch.location.line, "kind": ch.kind.name,
        "name": ch.spelling, "qual": qualname(ch),
        "virtual": bool(ch.kind == CK.CXX_METHOD and ch.is_virtual_method()),
        "override": [],
        "defaulted": bool(ch.kind in FUNC_KINDS and ch.is_default_method()),
    })
    if ch.kind in (CK.CXX_METHOD, CK.CONSTRUCTOR):
        d["cls"] = ch.semantic_parent.get_usr()
        d["sig"] = ch.displayname
    if ch.kind in (CK.CLASS_DECL, CK.STRUCT_DECL, CK.CLASS_TEMPLATE):
        d["bases"] = [b.referenced.get_usr() if b.referenced is not None else ""
                      for b in ch.get_children() if b.kind == CK.CXX_BASE_SPECIFIER]
    return usr


def record_refs(ch, fr, enc, refs):
    """Adds the uses that the cursor makes."""
    if ch.kind in UNRES_KINDS and ch.spelling:
        r0 = ch.referenced
        if r0 is None or r0.kind == CK.OVERLOADED_DECL_REF:
            refs.append(("n:" + ch.spelling, fr, enc))
            if r0 is not None:
                for k in range(lib.clang_getNumOverloadedDecls(r0)):
                    o = lib.clang_getOverloadedDecl(r0, k)
                    o._tu = ch._tu
                    ou = o.get_usr()
                    if ou:
                        refs.append((ou, fr, enc))
    ref = ch.referenced
    if ref is None or ref.kind == CK.TRANSLATION_UNIT:
        return
    ru = ref.get_usr()
    if not ru:
        return
    if ch.kind not in DEF_KINDS or ch.kind == CK.CALL_EXPR or ref != ch:
        refs.append((ru, fr, enc))
    # The USR of a specialization is not the USR of its template: record the name as well.
    if "<#" in ru or ">#" in ru:
        refs.append(("n:" + ref.spelling, fr, enc))


def walk(root, tu_cursor, defs, refs):
    # A node that clang adds without a source place (the call of an implicit conversion, for example) has no file. Its
    # children can be project code, so the walk goes on below it with the file of its parent.
    stack = [(tu_cursor, None, "")]
    while stack:
        c, enclosing, parent_file = stack.pop()
        for ch in c.get_children():
            loc_file = ch.location.file
            if loc_file is None:
                fr = parent_file
                if not fr:
                    continue
            else:
                fr = rel(root, loc_file.name)
                if fr is None or not fr.startswith(SCOPES):
                    continue  # system and external headers
            enc = enclosing
            if loc_file is not None and ch.kind in DEF_KINDS:
                usr = record_def(ch, fr, defs)
                if usr:
                    enc = usr
            record_refs(ch, fr, enc or "", refs)
            stack.append((ch, enc, fr))


def parse_one(args):
    root, entry = args
    cmd = entry.get("command") or " ".join(entry["arguments"])
    argv = shlex.split(cmd, posix=False)
    # Drop the compiler, the source file and the output switches. Keep the include, define and language switches.
    keep = []
    skip = False
    for a in argv[1:]:
        a = a.strip('"')
        if skip:
            skip = False
            continue
        if a in ("/Fo", "/Fd", "/Fe", "-o", "/FI"):
            skip = True
            continue
        if a.startswith(("/Fo", "/Fd", "/Fe", "/Yu", "/Yc", "/Fp", "/Z7", "/Zi", "/ZI", "/MP", "/showIncludes")):
            continue
        if a.lower().endswith((".cpp", ".cc", ".c")):
            continue
        if a.startswith("-external:I"):
            a = "/imsvc" + a[len("-external:I"):]
        elif a.startswith("-external:"):
            continue
        keep.append(a)
    # The warnings-as-errors switch and the CMake configuration define break the syntax tree.
    keep = [k for k in keep if not k.startswith("-DCMAKE_INTDIR")] + ["/WX-", "/clang:-ferror-limit=0"]
    keep += ["--driver-mode=cl", "/clang:-Wno-everything", "/clang:-fno-delayed-template-parsing"]
    index = cindex.Index.create()
    defs, refs = {}, []
    try:
        tu = index.parse(entry["file"], args=keep, options=cindex.TranslationUnit.PARSE_INCOMPLETE)
        walk(root, tu.cursor, defs, refs)
    except Exception as e:  # keep going and list the failure
        return entry["file"], defs, refs, str(e)
    errs = sum(1 for d in tu.diagnostics if d.severity >= 3)
    return entry["file"], defs, refs, f"errors={errs}"


def main():
    cc, root, out = sys.argv[1], sys.argv[2], sys.argv[3]
    jobs = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    entries = json.load(open(cc))
    todo, seen = [], set()
    for e in entries:
        r = rel(root, e["file"])
        if r and r.startswith(SCOPES) and r not in seen:
            seen.add(r)
            todo.append((root, e))
    alldefs, allrefs, log = {}, set(), []
    with ProcessPoolExecutor(jobs) as ex:
        for i, (f, defs, refs, note) in enumerate(ex.map(parse_one, todo, chunksize=4)):
            log.append((f, note))
            for u, d in defs.items():
                alldefs.setdefault(u, d)
            for r in refs:
                allrefs.add(r)
            if i % 50 == 0:
                print(i, len(todo), len(alldefs), len(allrefs), flush=True)
    json.dump({"defs": alldefs, "refs": sorted(allrefs), "log": log}, open(out, "w"))
    print("done", len(alldefs), "defs", len(allrefs), "refs")


if __name__ == "__main__":
    main()
