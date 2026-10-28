"""Exit 0 when every conflict block in the file only adds #include lines on both sides (base empty), so a union is
safe; exit 1 otherwise. usage: includes_only.py <file>   (diff3 conflict style)"""
import sys

lines = open(sys.argv[1], encoding="utf-8", newline="").read().splitlines()
part = None
seen = False
for line in lines:
    if line.startswith("<<<<<<< "):
        part, seen = "ours", True
    elif line.startswith("||||||| "):
        part = "base"
    elif line.startswith("======="):
        part = "theirs"
    elif line.startswith(">>>>>>> "):
        part = None
    elif part is not None:
        text = line.strip()
        if part == "base" and text:
            sys.exit(1)
        if text and not text.startswith("#include"):
            sys.exit(1)
sys.exit(0 if seen else 1)
