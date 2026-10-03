#!/usr/bin/env python3
"""Extract 'from -> to' edges between GYO-owned targets from a CMake graphviz dot file."""
import re, sys
text = open(sys.argv[1]).read()
labels = dict(re.findall(r'"(node\d+)"\s*\[\s*label\s*=\s*"([^"]+)"', text))
edges = set()
for a, b in re.findall(r'"(node\d+)"\s*->\s*"(node\d+)"', text):
    la, lb = labels.get(a, a), labels.get(b, b)
    if la.startswith(('gyo', 'GYO')) or lb.startswith(('gyo', 'GYO')):
        edges.add(f"{la} -> {lb}")
print('\n'.join(sorted(edges)))
