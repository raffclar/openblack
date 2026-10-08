import re
import sys

# Recounts the rows of progress files and rewrites their Progress line.
for path in sys.argv[1:]:
    text = open(path, encoding="utf-8").read()
    counts = {"done": 0, "partial": 0, "todo": 0, "n/a": 0}
    for line in text.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 3 and cells[1] in counts:
            counts[cells[1]] += 1
    total = counts["done"] + counts["partial"] + counts["todo"]
    score = round(100 * (counts["done"] + 0.5 * counts["partial"]) / total) if total else 0
    line = f"**Progress: {counts['done']}/{total} done, {counts['partial']} partial — {score}%**"
    text = re.sub(r"\*\*Progress: .*\*\*", line, text, count=1)
    open(path, "w", encoding="utf-8", newline="\n").write(text)
    print(path, "rows", sum(counts.values()), counts, line)
