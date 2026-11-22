REFLOAT GLOW

Permanent Project Instructions & Development Standards

The official name of this project is Refloat Glow (renamed by the user on 2026-11-21; it was Refloat rink-mixed, abbreviated refloatRM, and before that “Scott Wheels” / Scott Wheelz).

This replaces all previous project names. Download file names (`refloatrm.vescpkg`, `refloatrm-dev.vescpkg`) keep the old abbreviation so existing links and the in-app update check keep working.

These instructions apply to all future development, modifications, firmware builds, documentation, testing, and releases.

1. Project Identity & Attribution

Refloat Glow is an independently developed modification of the open-source Refloat firmware.

The goal of Refloat Glow is to expand Refloat’s functionality while respecting the original developers, preserving its core functionality, and maintaining a safe and reliable riding experience.

For all future builds:

* Use “Refloat Glow” as the project name (no abbreviation).
* Preserve original Refloat author credits, copyright notices, and license information.
* Clearly distinguish original Refloat functionality from Refloat Glow modifications.
* Do not imply that Refloat Glow is officially supported or endorsed by the original Refloat developers.
* Do not remove credit from any upstream contributors.
* Maintain a changelog documenting meaningful modifications.
* Preserve existing functionality unless changes are explicitly authorized.

2. Open-Source Licensing Compliance

The upstream Refloat project is distributed under GNU GPL v3.0.

All future development must comply with the applicable license requirements.

Mandatory requirements:

1. Preserve upstream copyright and licensing notices.
2. Include the applicable GPL-3.0 license text with distributed releases.
3. Clearly identify and document modifications.
4. Make the complete corresponding source code available when distributing modified firmware, as required by the GPL.
5. Retain all legally required attribution.
6. Review third-party dependencies for license compatibility.
7. Review the custom LCM firmware and bootloader independently, as they may have separate licensing requirements.
8. Never introduce proprietary or incompatible code without identifying the licensing implications.
9. Do not claim exclusive ownership of upstream code.
10. Do not assume that the GPL grants trademark rights or permission to imply official affiliation.

Before each public release, verify the actual applicable licenses against the source files and dependencies. Never assume compliance based solely on previous builds.

If a potential licensing issue is identified, flag it immediately and prevent the build from being classified as release-ready until resolved.

3. Firmware Safety & Reliability

Refloat Glow controls self-balancing electric vehicles. Rider safety is the highest development priority.

Mandatory safety rules:

* Preserve Refloat’s existing safety-critical protections unless explicitly approved changes have undergone appropriate review.
* Avoid unnecessary modifications to balance-control algorithms, motor-control logic, watchdogs, fault handling, thermal protections, and battery protections.
* Ensure additional features do not interfere with real-time motor-control execution.
* Review memory allocation, stack usage, execution timing, and interrupt behavior when relevant.
* Prevent lighting, animations, haptic effects, and communications from blocking safety-critical processes.
* Implement appropriate timeout and failure behavior for peripheral communication.
* Review all changes for potential unintended motor commands or unsafe behavior.
* Identify hardware-specific compatibility concerns.
* Never silently modify critical safety parameters.

Special attention must be given to:

* Custom LCM firmware and bootloader
* RGB lighting controls
* Controller-to-LCM communication
* Haptic feedback and horn features
* User-configurable settings
* Firmware updating and recovery

Any feature capable of influencing motor behavior must receive additional scrutiny.

Successful compilation is not proof of operational safety.

4. Development Process

Before implementing any new feature:

1. Identify which files and components will be affected.
2. Determine whether safety-critical functionality could be impacted.
3. Identify relevant licensing considerations.
4. Choose the least invasive implementation.
5. Preserve backwards compatibility when reasonably possible.
6. Document the intended behavior and potential failure modes.

After implementation:

1. Review the actual code changes.
2. Run available static analysis and compilation checks.
3. Examine relevant warnings.
4. Check memory usage and timing implications where possible.
5. Verify that unrelated functionality has not been unintentionally changed.
6. Document which tests were performed and their results.
7. Identify additional hardware testing requirements.

Do not invent successful test results or claim to have tested hardware that is not connected.

When information is missing, classify the relevant check as NOT VERIFIED.

5. Mandatory Build Sign-Off

At the conclusion of EVERY firmware build, modification, or development session, provide the following report:

REFLOAT GLOW — BUILD SIGN-OFF

Project information

* Build version:
* Build date:
* Upstream Refloat version:
* Target hardware:
* Source commit / revision:

Development summary

* Features added:
* Features modified:
* Bugs fixed:
* Files changed:
* Potential compatibility issues:

Licensing compliance

* Original attribution preserved: VERIFIED / NOT VERIFIED / NEEDS REVIEW
* GPL requirements reviewed: VERIFIED / NOT VERIFIED / NEEDS REVIEW
* Third-party licenses reviewed: VERIFIED / NOT VERIFIED / NEEDS REVIEW
* Corresponding source prepared for release: VERIFIED / NOT VERIFIED
* Licensing concerns:

Safety and reliability

* Safety-critical functionality affected:
* Code review completed:
* Compilation status:
* Static analysis status:
* Hardware testing status:
* Remaining safety concerns:

Final assessment

* Known issues:
* Outstanding requirements:
* Release status: DEVELOPMENT ONLY / READY FOR INDEPENDENT REVIEW / RELEASE CANDIDATE

Do not label any item VERIFIED without supporting evidence.

6. Mandatory Disclaimer

Include the following statement at the conclusion of every build sign-off:

“Refloat Glow is an independently developed modification of the open-source Refloat firmware.

Original Refloat developers and contributors retain their respective copyright and attribution rights. Refloat Glow is not an official Refloat release and is not endorsed by or affiliated with its original developers unless expressly stated.

This project is intended to comply with the GNU GPL v3.0 license and all applicable third-party licenses. Distribution of modified firmware must include required notices and access to corresponding source code in accordance with those licenses.

WARNING: This firmware operates on self-balancing electric vehicles capable of causing serious injury. Modified firmware may introduce unexpected behavior, including loss of balance, unintended acceleration, or loss of motor control.

AI-assisted development, successful compilation, and limited testing do not guarantee operational safety. Independent review and appropriate controlled hardware testing are strongly recommended before rider use.

This software is provided without warranty to the extent permitted by applicable law.”

7. Permanent Enforcement

These standards must remain active throughout all future Refloat Glow development.

Do not silently remove, bypass, or weaken these requirements.

If any future request conflicts with licensing obligations, established safety protections, or the integrity of the project, identify the conflict and recommend a compliant, safer alternative.

Maintain these instructions in the project’s persistent configuration, preferably CLAUDE.md, and reference them throughout development.

Your first action after receiving this prompt:

1. Review the current project structure and identify all existing references to “Scott Wheels.”
2. Update project-facing references to “Refloat Glow” where appropriate, without modifying historical attribution, third-party notices, or upstream copyright records.
3. Identify the upstream Refloat revision and licensing files.
4. Review existing build sign-off requirements and incorporate the new sections without deleting unrelated requirements.
5. Report any compliance gaps or safety concerns.
6. Summarize all changes before proceeding with further feature development.

Do not begin changing safety-critical firmware behavior as part of this naming and compliance update.

---

## Project facts (keep current)

- Name: **Refloat Glow** (since v1.2.3-rf147; package_name, UI title
  "REFLOAT — GLOW"). Earlier names: "Scott Wheelz" (until v1.2.3-rf1),
  Rink-Float, Re-RinkFloat, Re-Float Rink-Mixed, Refloat rink-mixed (refloatRM).
  The user asked to drop "rink" from the name. Old names stay in FORK_NOTES.md
  history and in old code comments.
- Upstream: Refloat **v1.2.3**, commit **800a46b** (https://github.com/lukash/refloat),
  GPL-3.0-or-later (`LICENSE`). Refloat author: Lukáš Hrázký (lukash); Float
  authors: Mitch Lustig, Dado Mista, Nico Aleman, based on work by Benjamin Vedder.
- Kept on purpose (renaming would break things, not project-facing):
  - GitHub repo `thedukesedge-collab/scott-wheelz` (download / update URLs).
  - QML Settings category `scottWheelz` (renaming wipes every user's saved app
    settings).
  - Historical entries in FORK_NOTES.md.
  - Download file names `refloatrm.vescpkg` / `refloatrm-dev.vescpkg` (links,
    in-app update check).
- Git history: upstream Refloat history through v1.2.3 is joined into `main`
  (merge -s ours, rf144). Remote `upstream` = lukash/refloat with
  `tagOpt --no-tags`. NEVER push upstream tags (v1.2.3, v1.3.x...) here: they
  would trigger release workflows. See RELEASING.md "Working with upstream".
- Third-party code and its review status: THIRD_PARTY_NOTICES.md.
- Changelog: FORK_NOTES.md (one entry per release, `v1.2.3-rfN`); version in
  `version`. Releases: RELEASING.md (tagged releases only after review).
- Files changed from upstream carry the line
  `Modified by Refloat Glow, 2026. See FORK_NOTES.md.`
  (GPL-3.0 section 5a). New files: SPDX / GPL-3.0-or-later header.
- Safety-critical upstream code NOT modified (verify with
  `git diff v1.2.3 -- src/<file>`): balance_filter, motor_control, pid, atr,
  torque_tilt, turn_tilt, booster, footpad sensor logic. Modified and needing
  extra scrutiny: main.c (horn / haptic tones, LCM, beeper, IMU timeout hook),
  imu.c / state.c (IMU timeout + flywheel-off backported from upstream 1.3),
  remote.c (4 deg remote tilt cap), haptic_feedback.c (motor tones).
- Checks available here: CI (`.github/workflows`): package build + clang-format,
  QML lint (`tools/qmltest`), LCM VP unit test (`tools/lcm_vp_test/run.sh`),
  app protocol tests (`apps/rinkmixed-android`, branch `app-android`).
  No hardware is connected to this environment: hardware tests are always
  NOT VERIFIED unless the user reports them.

## Standing working rules (from the user)

- After every update send only: the simulator link, the dev .vescpkg link and a
  brief description of the changes. No screenshots (user, 2026-11-21).
- Neon Future theme for everything new; short, simple UI wording.
- Terms (user, 2026-11-22): "SUW" = Swipe-Up Window, the bottom panel with
  TUNES / CONTROL / MIXER / DATA / BOARD STYLE / LEDS (`bottomSheet`).
  "Onboarding Tour" = the multi-slide guided walkthrough (`rinkTour`, BACK /
  NEXT / SKIP, dots, Replay Tour). Never "start-up wizard".
- Onboarding Tour self-check after EVERY revision (user, 2026-11-22): check
  each tour page (rinkTour pages/steps in ui.qml.in, TOUR/TOUR_STEPS in the
  simulator) against the change: names, locations (tab / window / Settings),
  wording, what it spotlights, and the little pictures (e.g. the UPDATES and
  RIDE SETTINGS mock-ups must match the real UI's look and text). Keep the
  tour as it is otherwise; only update it for new features or for
  accuracy. Say in the update note that the tour was checked and what changed.
- Piggyback on Refloat's own settings instead of reinventing them.
- Don't touch the balance code; config writes only when the board is stopped.
- New LED features only in the Refloat (controller-driven LED) view; the SLCM
  side stays as it is, tied to the latest SLCM firmware, unless the user says so.
- No copyrighted song melodies in the package (public-domain or original only).
- The user takes no credit and makes no money from this project.
