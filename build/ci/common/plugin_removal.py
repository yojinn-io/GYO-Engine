"""Remove a family of Plugins from a copy of the repository, and check a build of that copy fetched none of them.

Design Invariant SDL-2 (docs/architecture/target/v2/sdl.zh-Hant.md): with the SDL Plugins and their register rows
removed, the Engine, every Module and the headless tests still configure, build and pass, and SDL is neither
downloaded nor built. CI's core step runs:

  plugin_removal.py copy --repository . --destination <copy> --family sdl
  cmake -S <copy> --preset core -B <build> -DGYO_PLUGINS=ALL ...; cmake --build; ctest
  plugin_removal.py check --build <build> --family sdl

copy makes <copy> a git work tree of HEAD (the committed sources: tests that list files through git keep working)
and removes, staged, the family's rows of config/plugins.csv and their plugins/<name> directories, and every game
or tool whose CMake files name a removed Plugin (its directory, tests, acceptance programs, assets and register
row). A remaining register row that depends on a removed Plugin, or any other CMake file that still names one, is an
error: the copy could not configure, and the removal would not be what SDL-2 asks. check reads the build record
and the fetched sources.

A family is a name and the names that start with it and an underscore (sdl: sdl, sdl_window, sdl_gpu...).
Exit status 0 when done; 1 with one line per problem otherwise.
"""
import argparse
import csv
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

PLUGIN_REGISTER = "config/plugins.csv"
PRODUCT_KINDS = (("games", "config/games.csv"), ("tools", "config/tools.csv"))
# Where a product keeps its parts besides games/<name> or tools/<name>.
PRODUCT_PARTS = ("tests", "build/acceptance", "assets")
CMAKE_FILE = re.compile(r"(^|/)(CMakeLists\.txt|[^/]+\.cmake)$")


class RemovalError(Exception):
    pass


def in_family(name, family):
    return name == family or name.startswith(family + "_")


def read_register(path):
    text = path.read_text(encoding="utf-8")
    rows = list(csv.reader(io.StringIO(text)))
    if not rows:
        raise RemovalError(f"{path.name} has no header")
    return rows[0], rows[1:]


def write_register(path, header, rows):
    output = io.StringIO()
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow(header)
    writer.writerows(rows)
    path.write_text(output.getvalue(), encoding="utf-8")


def strip_comments(text):
    # CMake comments run from # to the end of the line; a # inside a quoted argument is rare in GYO's files and
    # would only hide a name, never invent one.
    return "\n".join(line.split("#", 1)[0] for line in text.splitlines())


def names_in(text, names):
    found = set()
    for name in names:
        if re.search(rf"(?<![A-Za-z0-9_]){re.escape(name)}(?![A-Za-z0-9_])", text):
            found.add(name)
    return found


def git(repository, *arguments):
    return subprocess.run(["git", "-C", str(repository), *arguments], check=True, capture_output=True,
                          text=True).stdout


def tracked_files(tree):
    return git(tree, "ls-files", "-z").split("\0")[:-1]


def remove_paths(tree, paths):
    existing = [path for path in paths if (tree / path).exists()]
    if existing:
        git(tree, "rm", "-r", "-q", "--", *existing)
        for path in existing:
            # Untracked leftovers (none in a fresh work tree) would keep a directory alive.
            if (tree / path).exists():
                shutil.rmtree(tree / path)


def remove_family(tree, family):
    """Removes the family from the work tree <tree>; returns the removed Plugins and products."""
    register = tree / PLUGIN_REGISTER
    header, rows = read_register(register)
    removed = [row[0] for row in rows if row and in_family(row[0], family)]
    if not removed:
        raise RemovalError(f"{PLUGIN_REGISTER} has no Plugin of the family '{family}'")
    kept = [row for row in rows if not (row and in_family(row[0], family))]
    depends = header.index("depends"), header.index("private_depends")
    problems = []
    for row in kept:
        needed = {name for column in depends for name in row[column].split(";") if name} & set(removed)
        if needed:
            problems.append(f"{PLUGIN_REGISTER}: '{row[0]}' depends on {', '.join(sorted(needed))}, which is removed")
    write_register(register, header, kept)
    git(tree, "add", "--", PLUGIN_REGISTER)
    remove_paths(tree, [f"plugins/{name}" for name in removed])

    products = []
    for root, register_path in PRODUCT_KINDS:
        path = tree / register_path
        if not path.exists():
            continue
        product_header, product_rows = read_register(path)
        dropped = []
        for row in product_rows:
            if not row:
                continue
            directory = tree / root / row[0]
            texts = [strip_comments(file.read_text(encoding="utf-8")) for file in directory.rglob("*")
                     if file.is_file() and CMAKE_FILE.search(file.as_posix())]
            if any(names_in(text, removed) for text in texts):
                dropped.append(row[0])
        if dropped:
            write_register(path, product_header, [row for row in product_rows if not row or row[0] not in dropped])
            git(tree, "add", "--", register_path)
            remove_paths(tree, [f"{part}/{name}" for name in dropped for part in (root, *PRODUCT_PARTS)])
            products.extend(f"{root}/{name}" for name in dropped)

    # The registers name Plugins only in their dependency columns, checked above; the CMake files anywhere.
    for name in tracked_files(tree):
        if CMAKE_FILE.search(name):
            text = strip_comments((tree / name).read_text(encoding="utf-8"))
            for found in sorted(names_in(text, removed)):
                problems.append(f"{name}: still names the removed Plugin '{found}'")
    if problems:
        raise RemovalError("\n".join(problems))
    return removed, products


def copy(repository, destination, family):
    repository = repository.resolve(strict=True)
    destination = destination.resolve()
    if destination.exists():
        # A previous run's work tree: drop it first, so the copy is HEAD again.
        subprocess.run(["git", "-C", str(repository), "worktree", "remove", "--force", str(destination)],
                       capture_output=True)
        if destination.exists():
            shutil.rmtree(destination)
        git(repository, "worktree", "prune")
    git(repository, "worktree", "add", "--detach", "--quiet", str(destination), "HEAD")
    return remove_family(destination, family)


def check(build, family):
    problems = []
    record = build / "gyo-build.json"
    try:
        plugins = json.loads(record.read_text(encoding="utf-8"))["plugins"]
    except (OSError, ValueError, KeyError) as error:
        return [f"{record}: no build record with a plugins list ({error})"]
    for name in plugins:
        if in_family(name, family):
            problems.append(f"{record}: the build added the Plugin '{name}'")
    deps = build / "_deps"
    if deps.is_dir():
        for entry in sorted(deps.iterdir()):
            if entry.name.lower().startswith(family):
                problems.append(f"{entry}: the build fetched a {family} dependency")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    copy_parser = commands.add_parser("copy")
    copy_parser.add_argument("--repository", type=Path, default=Path("."))
    copy_parser.add_argument("--destination", type=Path, required=True)
    copy_parser.add_argument("--family", required=True)
    check_parser = commands.add_parser("check")
    check_parser.add_argument("--build", type=Path, required=True)
    check_parser.add_argument("--family", required=True)
    args = parser.parse_args(argv)
    if args.command == "copy":
        try:
            removed, products = copy(args.repository, args.destination, args.family)
        except (RemovalError, subprocess.CalledProcessError) as error:
            detail = error.stderr if isinstance(error, subprocess.CalledProcessError) else str(error)
            for line in str(detail).strip().splitlines():
                print(f"Plugin removal: {line}")
            return 1
        print(f"Plugin removal: removed the Plugins {', '.join(removed)}"
              + (f" and the products {', '.join(products)}" if products else "") + f" in {args.destination}")
        return 0
    problems = check(args.build, args.family)
    for problem in problems:
        print(f"Plugin removal: {problem}")
    if problems:
        return 1
    print(f"Plugin removal: the build at {args.build} added and fetched nothing of the family '{args.family}'")
    return 0


if __name__ == "__main__":
    sys.exit(main())
