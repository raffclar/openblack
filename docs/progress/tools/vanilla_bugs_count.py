import os, re, sys
# Counts bug rows per vanilla_bugs file (and soft-lock rows) and rewrites each file's count line.
root = sys.argv[1]
res = {}
for f in sorted(os.listdir(root)):
    if not f.endswith(".md") or f == "README.md":
        continue
    p = os.path.join(root, f)
    text = open(p, encoding="utf-8").read()
    section, total, soft = None, 0, 0
    for line in text.splitlines():
        if line.startswith("## "):
            section = line[3:].strip()
        elif line.startswith("| ") and not line.startswith("| Bug |"):
            total += 1
            if section and section.startswith("Soft-locks"):
                soft += 1
    text = re.sub(r"\*\*Bugs: .*\*\*", f"**Bugs: {total} (soft-locks and lost progress: {soft})**", text, count=1)
    open(p, "w", encoding="utf-8", newline="\n").write(text)
    res[f] = (total, soft)
for f, (t, s) in res.items():
    print(f, t, s)
print("total", sum(t for t, _ in res.values()), sum(s for _, s in res.values()))
