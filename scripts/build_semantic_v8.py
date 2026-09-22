#!/usr/bin/env python3
"""Build the experimental semantic-v8 NVIDIA modules; never install or load them."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

from package_common import ROOT, NVIDIA_BASE, KERNEL

PATCH = ROOT / "patches/nvidia-gsp-semantic-tgp-v8.patch"
MODULES = ("nvidia", "nvidia-modeset", "nvidia-uvm", "nvidia-drm", "nvidia-peermem")
MARKER = "semantic-tgp-v8r5-20260922"


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def prepare_source(source):
    if not source.exists():
        source.parent.mkdir(parents=True, exist_ok=True)
        run("git", "clone", "https://github.com/NVIDIA/open-gpu-kernel-modules.git", str(source))
        run("git", "checkout", "--detach", NVIDIA_BASE, cwd=source)
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    if head != NVIDIA_BASE:
        raise RuntimeError(f"Source HEAD must be {NVIDIA_BASE}; found {head}")
    if subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard"], cwd=source).strip():
        raise RuntimeError("Untracked source files present; use a clean checkout")
    diff = subprocess.check_output(["git", "diff", "HEAD", "--binary"], cwd=source)
    if not diff:
        run("git", "apply", "--check", str(PATCH), cwd=source)
        run("git", "apply", "--index", str(PATCH), cwd=source)
    with tempfile.TemporaryDirectory(prefix="semantic-v8-index-") as tmp:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(tmp) / "index"))
        subprocess.run(["git", "read-tree", NVIDIA_BASE], cwd=source, env=env, check=True)
        subprocess.run(["git", "apply", "--cached", str(PATCH)], cwd=source, env=env, check=True)
        expected = subprocess.check_output(["git", "write-tree"], cwd=source, env=env, text=True).strip()
    if subprocess.run(["git", "diff", "--quiet", expected, "--"], cwd=source).returncode:
        raise RuntimeError("Checkout differs from the exact semantic-v8 patch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "build/nvidia-semantic-v8")
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 12))
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    source = args.source.resolve()
    prepare_source(source)
    if args.prepare_only:
        return
    if not Path("/usr/lib/modules", KERNEL, "build").is_dir():
        raise RuntimeError(f"Install matching {KERNEL} kernel headers before building")
    run("make", f"-j{args.jobs}", "CC=clang", "LD=ld.lld", f"KERNEL_UNAME={KERNEL}", "modules", cwd=source)
    out = ROOT / "build/semantic-v8"
    out.mkdir(parents=True, exist_ok=True)
    run("cc", "-O2", "-Wall", "-Wextra", "-Werror", "-rdynamic",
        "-I" + str(source / "src/common/sdk/nvidia/inc"),
        "-I" + str(source / "src/nvidia/arch/nvalloc/common/inc"),
        "-I" + str(source / "src/nvidia/interface"),
        str(ROOT / "src/runtime/query_semantic_power.c"),
        str(ROOT / "src/runtime/semantic_boot.c"),
        "-ldl", "-lsystemd", "-o", str(out / "mechrevo-semantic-tgp"))
    marker = subprocess.check_output(
        ["modinfo", "-F", "gsp_read_probe", str(source / "kernel-open/nvidia.ko")], text=True
    ).strip()
    if marker != MARKER:
        raise RuntimeError(f"Unexpected module marker: {marker}")
    manifest = {
        "status": "built_not_installed",
        "nvidia_base": NVIDIA_BASE,
        "kernel": KERNEL,
        "marker": MARKER,
        "source": str(source / "kernel-open"),
        "patch_sha256": sha(PATCH),
        "module_hashes": {name + ".ko": sha(source / "kernel-open" / (name + ".ko")) for name in MODULES},
        "runner_sha256": sha(out / "mechrevo-semantic-tgp"),
    }
    (out / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
