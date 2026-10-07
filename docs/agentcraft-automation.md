# AgentCraft development queue

Create a GitHub issue with a concrete outcome and acceptance criteria, then add
the `agent-ready` label. The AgentCraft service on the Proxmox host polls every
two minutes and sends the issue to the Foreman on `gs1`.

## Documentation issue quickstart

Copy this example into a GitHub issue:

```text
Title: Document the Windows build workflow in CONTRIBUTING.md

Body:
Add a short Windows build verification section to CONTRIBUTING.md so contributors
can find the compiler/package gate before requesting review.

Scope: Change only CONTRIBUTING.md; add no more than 20 lines. Do not change code,
workflows, build files, assets, secrets or orchestration policy.

Acceptance criteria:
- Name the Windows build workflow and link to .github/workflows/build.yml.
- Explain that a passing build does not establish multiplayer gameplay correctness.
- Verify the relative workflow link resolves to an existing file.
- Run git diff --check and report the changed-file list and added-line count.
```

Applying `agent-ready` admits the issue to the queue. The queue waits for the
previous pull request to be merged or closed before admitting the next issue.
Code pull requests require human review; documentation pull requests follow the
automatic-merge checks described below.

Each goal uses `agentcraft/goals/gN` as its integration branch. Workers keep their
individual worktrees, run local checks, and send completed tasks to lead review.
The Foreman accepts an explicit lead verdict only after checking the committed
merge result for formatting, secrets, binaries, file size and diff limits.
The host service publishes a pull request when the goal finishes.

`main` changes through GitHub pull requests. Small documentation-only pull requests
can merge automatically after the exact candidate has successful `build` and
`checks` jobs. Code, build files, workflows, agent instructions and releases need
human review. A missing, failing, pending or changed-head check prevents automatic
merge. There is no force push or administrator bypass.

The queue admits one issue goal at a time and at most four per New York calendar
day. A goal has a two-hour wall-clock limit; a timed-out goal pauses for review.
Agent turns also have step limits. The idle service polls GitHub without model
calls. Its durable state prevents duplicate issue intake and recovers publication
after a restart.

The current project builds for Windows x64. The Linux agent host can run static
checks, but the `Windows build` GitHub workflow is the compiler/package gate.
The repository does not yet have game unit tests; passing static checks or a build
does not establish multiplayer gameplay correctness. Gameplay acceptance and
release publication require separate testing and review.

Inputs in `/mnt/ampExt01/instances/AgentCraftForeman01/reference-files/` are readable
from the agent runtime but remain outside Git. Do not copy the supplied binary ZIP
or private credentials into commits.

Operations instructions live on the Proxmox host under
`/home/h3x/cld/agentcraft-automation/`. Reports are in
`/var/lib/agentcraft-automation/reports/` and are generated at 07:00
`America/New_York` and include work, pull requests, check results and blocked items.
See that directory's operations guide for service control, logs and recovery.
