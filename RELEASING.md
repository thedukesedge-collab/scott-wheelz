# Releasing Refloat Glow

Riders only get **tagged releases**. Nothing pushed to `main` reaches a rider
by itself.

| What | Where | Who it's for |
|---|---|---|
| Push to `main` | `dev-build` (package) and `lcm-dev` (SLCM) pre-releases | Bench testing only, wheel off the ground |
| Tag `v<version>`, e.g. `v1.2.3-rf77` | Release `v1.2.3-rf77` (marked Latest) and `lcm-latest` | Riders |

`lcm-latest` is the rider channel for the light module firmware: the tag
flow (lcm.yml) replaces its files on every rider release. The copy there now
(from 1805792, Nov 17) predates the tag flow and has not been through the
bench checklist; it will be replaced by the first rider release.

Until the first `v1.2.3-rfN` tag exists, the rider link below and the app's
UPDATES check have no release to point to (GitHub answers 404).

Every tagged release stays on GitHub, so the previous build is always one tap
away if a new one misbehaves.

Rider download link (always the newest tagged release):
`https://github.com/thedukesedge-collab/scott-wheelz/releases/latest/download/refloatglow.vescpkg`

## Steps

1. Bump `version` (e.g. `1.2.3-rf77`), add the FORK_NOTES.md entry, push to
   `main`. Wait for CI (package build, stack report, LCM build and unit
   tests) to go green.
2. Install the `dev-build` package (and, if the LCM changed, flash the
   `lcm-dev` SLCM) on a board and run the bench checklist below.
3. Only if every item passes, tag the same commit and push the tag:

       git tag v1.2.3-rf77 <commit>
       git push origin v1.2.3-rf77

   CI checks that the tag matches the `version` file, builds again, and
   publishes the rider release. If it doesn't match, nothing is published.
4. If a release turns out bad: tell riders to install the previous release
   (Releases page), fix on `main`, and go through the steps again with a new
   version. Never move or reuse a tag.

## Bench checklist (wheel off the ground, board on a stand)

Package:
- [ ] Package installs, UI loads, version shown matches.
- [ ] Refloat Cfg reads back the same values as before the install.
- [ ] Footpads: engage with both pads, disengage on release, half-switch fault works.
- [ ] Pitch fault: tilt past the fault angle, the motor stops.
- [ ] Horn while engaged: beep only, no change in the motor.
- [ ] Change a light setting in the LEDS tab while engaged: no hiccup in the
      motor; the setting is saved only after stepping off (check after a
      power cycle).
- [ ] Flywheel mode on and off from the app: turning it off stops the board.
- [ ] Power-cycle: settings (horn, lights, battery curve) survive.
- [ ] LEDS › UPDATES: CHECK FOR UPDATES shows the previous release correctly
      (package / LCM versions); DOWNLOAD UPDATE + FLASH UPDATE reach the
      normal flash flow. After tagging, check again: the new release shows
      as up to date.

Light module (only if `lcm/` changed):
- [ ] Update from the app completes; lights come back.
- [ ] While the update runs, stepping on the pads does NOT engage and beeps.
- [ ] Cancel an update mid-way (close the app): the board does not engage,
      lights stay dark, power stays ON (no timed power-off); holding the
      button 2 s powers off; next power-up the app can finish the update.
- [ ] Unplug the LCM-VESC link (or restart the VESC) while idle: no fault
      beeping, the board does not power itself off, lights recover when the
      link is back.
- [ ] Idle auto-shutdown still works with the link up (leave it ~10 min).

## Licensing in every release
- The release notes say Refloat Glow is an unofficial Refloat modification
  under GPL-3.0-or-later, link the exact source commit, and point to
  THIRD_PARTY_NOTICES.md (built into the workflows and tools/release_notes.md).
- Before tagging, check THIRD_PARTY_NOTICES.md: nothing new without a
  license, and no NEEDS REVIEW item that affects the files being released.

## Working with upstream Refloat
Our `main` contains Refloat's full history: upstream v1.2.3 (800a46b) was
joined in with a merge that changed no files (`git merge -s ours`), so
lukash's commits and authorship show in `git log`, and Git knows exactly
which upstream version we are based on.

One-time setup in a clone:

    git remote add upstream https://github.com/lukash/refloat
    git config remote.upstream.tagOpt --no-tags
    git fetch upstream

- **Never push upstream's tags to this repository** (`git push --tags`, or
  pushing `v1.2.3` etc.). A tag like `v1.3.1` would start the old upstream
  `create_release.yml` and our rider release job. Push branches by name,
  e.g. `git push origin main`.
- See what upstream changed since our base:
  `git log --oneline v1.2.3..upstream/main` (or `..<tag>` after
  `git fetch upstream tag <tag>`).
- Bring in upstream changes on a branch (e.g. `upstream-1.3.1`), never
  straight on `main`: `git merge <upstream tag>`, resolve, then run the full
  CI and the bench checklist. Safety-critical files (balance, motor,
  footpad, state) take upstream's version unless there is a reviewed reason.
- Contributing back to Refloat: use a separate, clean fork of
  lukash/refloat (GitHub "Fork"), branch from upstream `main`, port one
  isolated change per pull request, open an issue first to ask if it's
  wanted, follow Refloat's CONTRIBUTING.md, and say it was AI-assisted.
