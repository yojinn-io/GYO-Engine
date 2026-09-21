"""Compile two copied public fixtures with CI, tests and Editor sources absent."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests/common/fixtures/copy_game"


def mirror_sources(repository, destination):
    destination.mkdir(parents=True, exist_ok=False)
    inputs = subprocess.check_output(["git", "-C", str(repository), "ls-files", "--cached", "--others", "--exclude-standard", "-z"])
    copied = []
    for name in sorted(set(inputs.decode("utf-8").split("\0"))):
        path = Path(name)
        include = name in ("CMakeLists.txt", "CMakePresets.json") or name.startswith(("engine/", "third_party/", "build/cmake/")) or name in ("build/assemble_runtime.py", "build/workspace.py")
        original = repository / path
        if not include or not original.is_file() or original.is_symlink():
            continue
        target = destination / path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
        copied.append(name)
    return copied


def create_copy(source, fixture=FIXTURE, names=("sample_game", "variant_game")):
    for name in names:
        shutil.copytree(fixture / "app", source / "apps" / name)
        shutil.copytree(fixture / "assets", source / "assets" / name)
        (source / "assets" / name / "marker.txt").write_text(name + "\n", encoding="utf-8")
    registry = source / "engine/config/projects.csv"
    registry.parent.mkdir(parents=True, exist_ok=True)
    registry.write_text("name,description,version,enabled,windows,linux,macos\n" +
                        "".join(f"{name},Copy fixture,,1,1,1,1\n" for name in names), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=ROOT)
    parser.add_argument("--workdir", type=Path, required=True)
    parser.add_argument("--logs", type=Path)
    parser.add_argument("--reuse-cache", type=Path)
    parser.add_argument("--parallel", type=int, default=2)
    args = parser.parse_args()
    repository, work = args.repository.resolve(strict=True), args.workdir.resolve()
    if work == repository or repository.is_relative_to(work):
        parser.error("Scratch workspace must not contain the repository")
    work.mkdir(parents=True, exist_ok=False)
    logs = (args.logs or work / "logs").resolve()
    logs.mkdir(parents=True, exist_ok=True)
    source = work / "source"
    mirror_sources(repository, source)
    create_copy(source, repository / "tests/common/fixtures/copy_game")
    options = ["-DCMAKE_BUILD_TYPE=Release", "-DGYO_BUILD_UI_EDITOR=OFF"]
    if args.reuse_cache:
        for line in args.reuse_cache.read_text(encoding="utf-8-sig").splitlines():
            if ":" not in line or "=" not in line:
                continue
            name, value = line.split("=",1)
            key = name.split(":",1)[0]
            if key in ("CMAKE_C_COMPILER","CMAKE_CXX_COMPILER","CMAKE_MAKE_PROGRAM") or key.startswith("FETCHCONTENT_SOURCE_DIR_"):
                if value:
                    options.append(f"-D{key}={value}")
        for path in (args.reuse_cache.parent / "_deps").glob("*-src"):
            options.append(f"-DFETCHCONTENT_SOURCE_DIR_{path.name[:-4].upper()}={path.resolve()}")
    binary = work / "build"
    def run(name, command):
        command = [str(value) for value in command]
        log = logs / (name + ".log")
        print(f"[{name}] {subprocess.list2cmdline(command)}", flush=True)
        with log.open("w", encoding="utf-8") as output:
            result = subprocess.run(command, cwd=work, stdout=output, stderr=subprocess.STDOUT, timeout=1800)
        text = log.read_text(encoding="utf-8", errors="replace")
        if result.returncode:
            raise RuntimeError(f"{name} failed; see {log}: {text[-10000:]}")
        return text
    run("configure", ["cmake","-S",source,"-B",binary,"-G","Ninja",*options])
    run("build", ["cmake","--build",binary,"--parallel",args.parallel])
    stage = work / "installed"
    run("install", ["cmake","--install",binary,"--prefix",stage])
    for name in ("sample_game","variant_game"):
        suffix = ".exe" if sys.platform == "win32" else ""
        output = run(name, [stage / "bin" / ("gyo_" + name + suffix)])
        if f"{name}:{name}" not in output:
            raise RuntimeError(f"Wrong identity or crossed assets for {name}: {output}")
    if any((source / path).exists() for path in ("tests","tools","build/ci","build/acceptance")):
        raise RuntimeError("Fixture unexpectedly retained optional development sources")
    (logs / "integration-result.json").write_text(json.dumps({"passed":True,"products":["sample_game","variant_game"]}), encoding="utf-8")


if __name__ == "__main__":
    main()
