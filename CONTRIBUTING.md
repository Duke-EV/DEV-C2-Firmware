# Contributing to DEV C2 Firmware

Thanks for helping improve the DEV firmware stack. This repository contains multiple PlatformIO projects for vehicle boards plus shared libraries used across them. To keep the codebase maintainable, we follow a simple workflow: work on a dedicated feature branch, keep changes focused, and open a pull request against `main` once the affected project builds and tests cleanly.

## Repository layout

- `boards/` contains each board-specific firmware project. Each subdirectory is a separate PlatformIO project.
- `lib/libcannetwork/` contains the shared CAN network implementation and vehicle-facing message definitions.
- `docs/` contains project documentation.
- `test-projects/` contains helper/test projects for validation and debugging.

This repo is intentionally split so that board applications stay thin and shared logic lives in the common library.

## Working conventions

### Feature branches

All work should be done on a feature branch, not directly on `main`.

Use branch names like:

- `feature/can-heartbeat-monitor`
- `feature/throttle-safety-checks`
- `fix/peripherals-i2c-timeout`
- `docs/can-network-overview`

Keep branches scoped to one topic or fix. Do not combine unrelated changes in the same branch.

### Small, reviewable changes

Prefer focused changes that are easy to understand and test. Large refactors should be broken into smaller PRs when possible.

Before opening a PR:

- Pull the latest `main`
- Create a branch from `main`
- Make your changes
- Validate the affected PlatformIO project(s)
- Open a PR with a clear summary and testing notes

## Code ownership and architecture

The repository is built around a shared CAN network layer.

Important rules for contributors:

- Any code related to CAN communication belongs in `lib/libcannetwork`.
- This includes reading/writing to the network, periodic timers, and any ISRs tied to the vehicle network.
- Board applications should not implement CAN network logic directly.
- Board applications should call `init_network()` during setup and use the shared `g_vehicle` instance for network data.

This keeps the firmware portable across the board projects and ensures common network behavior is consistent.

## Coding expectations

- Prefer simple, readable C++ over advanced patterns that are not needed.
- Do not introduce forbidden patterns like `constexpr`, `std::atomic`, or heavy template metaprogramming for normal firmware logic.
- Use the simplest primitives that satisfy the behavior.
- If a change affects the CAN layer, check whether it belongs in the shared library instead of one board project.
- Keep board-specific logic limited to that board's responsibilities.

## Building and testing

Each board project is built separately with PlatformIO.

Typical workflow:

1. Open the relevant board directory in `boards/`
2. Run the build for that project
3. Run any project-specific tests if they exist
4. If the change affects shared CAN logic, validate the relevant boards that depend on it

Example commands:

- `cd boards/throttle && pio run`
- `cd boards/peripherals && pio run`
- `cd boards/communications && pio test`

If a board does not have tests, a clean build is the minimum expectation.

If you are changing common code in `lib/libcannetwork`, test at least the board projects most affected by that change, or explain why a narrower validation was sufficient.

## Pull request process

When you are ready to open a PR:

1. Make sure your branch is up to date with `main`
2. Keep the PR focused on one topic
3. Include a short summary of what changed and why
4. List the build or test commands you ran
5. Note any follow-up work or known limitations

A good PR description usually includes:

- Problem or issue being addressed
- Files changed
- Why the change is needed
- Validation performed

## Issue reporting

If you are reporting a bug or requesting a feature:

- Check whether the issue already exists
- Open a clear issue with reproduction steps when possible
- Include the board/project affected
- Include relevant hardware or software context
- Mention whether the issue is in a board project or in the shared CAN library

## Commit guidance

Use clear, descriptive commit messages. Prefer a conventional style such as:

- `feat: add throttle fault state handling`
- `fix: correct heartbeat timeout logic`
- `docs: update CAN library overview`
- `test: add communication board validation`

Keep commits focused and readable.

## Final reminder

The project is meant to be collaborative and educational. Please keep changes understandable, scoped, and in line with the shared architecture. A clean feature branch, a tight PR, and verified builds go a long way toward maintaining a healthy firmware repo.