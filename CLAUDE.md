# CLAUDE.md

> **MANDATORY:**
> You **MUST** read and follow [AGENTS.md](AGENTS.md) before taking any
> action, writing code or proposing a commit in this repository.

All operating rules, the commit message convention, the Git workflow and
`main` protection, data and legacy-migration safety, build/test commands
and the release rules live in one single place:

**[AGENTS.md](AGENTS.md)** and **[docs/commit-convention.md](docs/commit-convention.md)**

## Claude-specific notes

- When the user writes Vietnamese, reply in Vietnamese. Keep technical
  terms in English.
- Project documentation and UI text are English; UI strings go through
  the i18n system (see AGENTS.md section 6).
- Never commit or push without the user's explicit approval.
- Never report a build, test or release as successful without verifying
  the actual artifact or result.
- If a request conflicts with AGENTS.md, AGENTS.md wins - say so clearly
  to the user.
