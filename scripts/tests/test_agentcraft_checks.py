"""Exercise the PR scanner against real committed Git objects."""
import pathlib
import subprocess
import sys
import tempfile
import unittest


SCANNER = pathlib.Path(__file__).resolve().parents[1] / "agentcraft_checks.py"


class AgentCraftChecksTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="agentcraft-checks-test-")
        self.addCleanup(self.temp.cleanup)
        self.repo = pathlib.Path(self.temp.name)
        self.git("init", "-q", "-b", "main")
        self.git("config", "core.autocrlf", "false")
        self.write("README.md", "Initial documentation.\n")
        self.commit()
        self.base = self.git("rev-parse", "HEAD").stdout.strip()

    def git(self, *args):
        return subprocess.run(
            ["git", "-c", "core.hooksPath=/dev/null", *args],
            cwd=self.repo,
            text=True,
            capture_output=True,
            check=True,
        )

    def write(self, name, content):
        target = self.repo / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content.encode("utf-8") if isinstance(content, str) else content)

    def commit(self):
        self.git("add", "-A")
        self.git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-qm", "fixture")

    def scan(self):
        self.commit()
        return subprocess.run(
            [sys.executable, str(SCANNER), "--base", self.base, "--head", "HEAD"],
            cwd=self.repo,
            text=True,
            capture_output=True,
            check=False,
        )

    def assertRejected(self, result, reason):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(reason, result.stderr + result.stdout)

    def test_valid_documentation_and_crlf_text_are_accepted(self):
        self.write("docs/design\tnew.md", "Documentação and ordinary source notes.\n")
        self.write("docs/windows.md", b"Windows notes.\r\nA second line.\r\n")
        result = self.scan()
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertIn("PASS:", result.stdout)
        self.assertIn("2 files, 3 lines", result.stdout)

    def test_new_binary_is_rejected(self):
        self.write("data.dat", b"\x00\xff\x01\x02")
        self.assertRejected(self.scan(), "binary")

    def test_symlink_is_rejected_without_following_its_target(self):
        (self.repo / "outside.txt").symlink_to("/etc/passwd")
        self.assertRejected(self.scan(), "symlink")

    def test_private_environment_path_is_rejected(self):
        self.write("config/.env.production", "SETTING=ordinary\n")
        self.assertRejected(self.scan(), "private environment file")

    def test_changed_line_limit_is_enforced(self):
        self.write("large.txt", "one\n" * 1201)
        self.assertRejected(self.scan(), "more than 1200 changed lines")

    def test_changed_file_limit_is_enforced(self):
        for index in range(16):
            self.write(f"notes/{index}.md", "one\n")
        self.assertRejected(self.scan(), "more than 15 changed files")

    def test_trailing_whitespace_is_rejected(self):
        self.write("source.txt", "ordinary text   \n")
        self.assertRejected(self.scan(), "trailing whitespace")

    def test_unresolved_conflict_marker_is_rejected(self):
        self.write("source.txt", "<<<<<<< HEAD\nours\n=======\ntheirs\n>>>>>>> topic\n")
        self.assertRejected(self.scan(), "leftover conflict marker")


if __name__ == "__main__":
    unittest.main()
