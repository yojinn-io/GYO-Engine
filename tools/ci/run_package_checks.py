"""Run declared installed checks without a shell or source-tree working directory."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ci_workspace import TemporaryDirectory

from package_contract import load_manifest, manifest_digest, required_checks


def run_checks(stage: Path, app: str, platform: str, revision: str, profile: str,
               logs: Path, *, gpu=False, driver="") -> bool:
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Package revision must be a full lowercase Git commit SHA")
    stage, logs = stage.resolve(strict=True), logs.resolve()
    logs.mkdir(parents=True, exist_ok=True)
    manifest = load_manifest(stage, app, platform)
    identity = dict(app=app, platform=platform, source_revision=revision, profile=profile,
                    manifest_sha256=manifest_digest(manifest))
    report_path = logs / "acceptance.json"
    report = {**identity, "checks": []}
    if gpu:
        if not report_path.is_file():
            raise ValueError("GPU acceptance requires the preceding installed startup report")
        report = json.loads(report_path.read_text(encoding="utf-8"))
        if any(report.get(key) != value for key, value in identity.items()):
            raise ValueError("Cannot merge acceptance evidence from a different package/profile")
    selected = [check for check in required_checks(manifest, profile) if check["gpu"] == gpu]
    if gpu and selected and not driver:
        raise ValueError("GPU acceptance requires --driver")
    replacing = {check["name"] for check in selected}
    report["checks"] = [check for check in report["checks"] if check["name"] not in replacing]
    substitutions = {
        "@PACKAGE_ROOT@": str(stage), "@EXECUTABLE@": str(stage / manifest["executable"]),
        "@LOG_ROOT@": str(logs), "@PYTHON@": sys.executable, "@PROFILE@": profile,
        "@GPU_SUITE@": "full" if profile == "release" else "quick", "@DRIVER@": driver,
    }

    def expand(argument):
        for key, value in substitutions.items():
            argument = argument.replace(key, value)
        return argument

    passed = True
    with TemporaryDirectory(prefix="gyo-installed-check-") as work:
        for check in selected:
            command = [expand(argument) for argument in check["command"]]
            environment = os.environ.copy()
            for entry in check["environment"]:
                key, value = entry.split("=", 1)
                environment[key] = expand(value)
            try:
                result = subprocess.run(command, cwd=work, env=environment, text=True,
                                        encoding="utf-8", errors="replace", stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, timeout=check["timeout"], check=False)
                code, output = result.returncode, result.stdout
            except subprocess.TimeoutExpired as error:
                partial = error.stdout or b""
                output = partial.decode("utf-8", errors="replace") if isinstance(partial, bytes) else partial
                code, output = -1, output + "\nPackage check timed out\n"
            except OSError as error:
                code, output = -2, str(error)
            (logs / (check["name"] + ".log")).write_text(output, encoding="utf-8")
            report["checks"].append(dict(name=check["name"], exit_code=code, passed=code == 0))
            passed = passed and code == 0
            print(f"[{app}/{check['name']}] exit={code}\n{output}", flush=True)
            # Preserve completed failures even when a later process cannot run.
            report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("app", "platform", "revision", "profile"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--stage", required=True, type=Path)
    parser.add_argument("--logs", required=True, type=Path)
    parser.add_argument("--gpu", action="store_true")
    parser.add_argument("--driver", default="")
    parser.add_argument("--inspect", action="store_true", help="Print has_gpu for GitHub outputs, without running checks")
    args = parser.parse_args()
    if args.inspect:
        manifest = load_manifest(args.stage, args.app, args.platform)
        print("has_gpu=" + str(any(check["gpu"] for check in required_checks(manifest, args.profile))).lower())
        return 0
    return 0 if run_checks(args.stage, args.app, args.platform, args.revision, args.profile,
                           args.logs, gpu=args.gpu, driver=args.driver) else 1


if __name__ == "__main__":
    raise SystemExit(main())
