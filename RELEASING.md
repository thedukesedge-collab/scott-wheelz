# Releasing Re-Float Rink-Mixed

Riders only get **tagged releases**. Nothing pushed to `main` reaches a rider
by itself.

| What | Where | Who it's for |
|---|---|---|
| Push to `main` | `dev-build` (package) and `lcm-dev` (SLCM) pre-releases | Bench testing only, wheel off the ground |
| Tag `v<version>`, e.g. `v1.2.3-rf77` | Release `v1.2.3-rf77` (marked Latest) and `lcm-latest` | Riders |

Every tagged release stays on GitHub, so the previous build is always one tap
away if a new one misbehaves.

Rider download link (always the newest tagged release):
`https://github.com/thedukesedge-collab/scott-wheelz/releases/latest/download/refloatrm.vescpkg`

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
- [ ] UPDATES tab: CHECK FOR UPDATES shows the previous release correctly
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
