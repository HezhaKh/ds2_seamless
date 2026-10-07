#!/usr/bin/env python3
"""Deterministic PR checks for the AgentCraft queue (no game execution)."""
import argparse
import re
import subprocess
import sys

FORBIDDEN = re.compile(r"\.(exe|dll|so|dylib|zip|7z|rar|jar|pdb|obj|o|lib|a|key|pem|pfx|p12)$", re.I)


def git(*args):
    return subprocess.check_output(["git", "-c", "core.hooksPath=/dev/null", *args])


def validate(base, head):
    hashes = [git("rev-parse", "--verify", "--end-of-options", ref + "^{commit}").decode().strip() for ref in (base, head)]
    if any(not re.fullmatch(r"[a-f0-9]{40,64}", sha) for sha in hashes):
        raise ValueError("invalid commit")
    subprocess.run(["git", "-c", "core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol", "diff", "--check", "--no-ext-diff", "--no-textconv", *hashes, "--"], check=True)
    diff = git("diff", "--numstat", "--no-renames", "--no-ext-diff", "--no-textconv", "-z", *hashes, "--")
    records = [record for record in diff.split(b"\0") if record]
    if len(records) > 15:
        raise ValueError("more than 15 changed files; split the change")
    lines = 0
    for record in records:
        added, deleted, name = record.split(b"\t", 2)
        name = name.decode("utf-8", errors="strict")
        if added == b"-" or deleted == b"-" or FORBIDDEN.search(name):
            raise ValueError("binary, archive or credential path: " + name)
        lines += int(added) + int(deleted)
        components = name.lower().split("/")
        leaf = components[-1]
        if any(p in (".ssh", ".aws", ".gnupg") for p in components):
            raise ValueError("credential directory: " + name)
        if (leaf == ".env" or leaf.startswith(".env.")) and not leaf.endswith((".example", ".sample", ".template")):
            raise ValueError("private environment file: " + name)
        tree = git("ls-tree", "-z", hashes[1], "--", name)
        if not tree:
            continue
        mode, kind, obj = tree.split(b"\t", 1)[0].split()
        if mode not in (b"100644", b"100755") or kind != b"blob":
            raise ValueError("symlink or submodule: " + name)
        if int(git("cat-file", "-s", obj.decode())) > 1024 * 1024:
            raise ValueError("file exceeds 1 MiB: " + name)
        content = git("cat-file", "blob", obj.decode())
        content.decode("utf-8", errors="strict")
        if b"\0" in content:
            raise ValueError("binary content: " + name)
    if lines > 1200:
        raise ValueError("more than 1200 changed lines; split the change")
    print(f"PASS: format, paths, text files and diff limits ({len(records)} files, {lines} lines)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", default="HEAD")
    args = parser.parse_args()
    if args.base == "0" * 40:
        raise ValueError("base commit is unavailable; explicit review required")
    validate(args.base, args.head)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, UnicodeError, subprocess.CalledProcessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
