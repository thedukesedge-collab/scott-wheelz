# Third-party notices

Refloat Glow is licensed under the GNU GPL v3.0 or later ([LICENSE](LICENSE)).
It contains or builds with the third-party code listed below. Each part keeps
its own copyright and license. "Status" is our own review status, not legal
advice. Anything marked **NEEDS REVIEW** has not been fully checked yet.

## VESC package (`refloatglow.vescpkg`)

| Component | Where | Copyright | License | Status |
|---|---|---|---|---|
| Refloat v1.2.3 | everything not listed below | Lukáš Hrázký, Mitch Lustig, Dado Mista, Nico Aleman, Benjamin Vedder and contributors | GPL-3.0-or-later | OK (same license) |
| VESC package library | `vesc_pkg_lib/` | Benjamin Vedder and contributors | GPL-3.0-or-later (as shipped with Refloat) | OK |

## Light module firmware (`SLCM.hex`, `ALCM_VP.hex`, `ALCM_VP_app.bin`)

| Component | Where | Copyright | License | Status |
|---|---|---|---|---|
| ALCM v0.1.1 | `lcm/alcm/` | Mitchell White | GPL-3.0 ([lcm/alcm/LICENSE](lcm/alcm/LICENSE)) | OK |
| Refloat Glow bootloader | `lcm/bootloader/` | Refloat Glow | GPL-3.0-or-later | OK |
| ARM CMSIS | `lcm/alcm/Library/.../CMSIS/` | ARM Limited | BSD-3-Clause, notice below | OK, GPL-compatible. The notice must go with binary releases. |
| HK32F030M standard peripheral library | `lcm/alcm/Library/HK32F030Mxx_Library_V1.1.6/HK32F030M/HK32F030M_Lib/` | Shenzhen Hangshun Chip Technology (HK) | **No license text in the files** | **NEEDS REVIEW.** It is shipped the same way by upstream ALCM. Its redistribution terms are not confirmed. |

## LCM Flasher (`LCM_Flasher.exe`, built by `.github/workflows/lcm_flasher.yml`)

The script `lcm/flasher/LCM_Flasher.pyw` is GPL-3.0-or-later. The Windows
`.exe` is a PyInstaller bundle that also contains Python and its standard
library (PSF License), Tcl/Tk (BSD-style), pyOCD (Apache-2.0) and pyOCD's
dependencies, including pyusb (BSD) and libusb (LGPL-2.1).
**Status: NEEDS REVIEW**: the exact bundled list and its notices have not
been collected yet.

## Phone app (branch `app-android`, `apps/rinkmixed-android`)

The app's own code is GPL-3.0-or-later. It is built with Capacitor
(`@capacitor/core`, `@capacitor/android`, `@capacitor/ios`; MIT) and
`@capacitor-community/bluetooth-le` (MIT), plus their transitive dependencies.
**Status: NEEDS REVIEW**: the transitive dependency licenses have not been
collected yet.

---

### ARM CMSIS license (BSD-3-Clause)

```
Copyright (c) 2009 - 2015 ARM LIMITED (CMSIS core headers)
Copyright (C) 2010-2015 ARM Limited. All rights reserved. (CMSIS DSP headers)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
  - Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
  - Neither the name of ARM LIMITED nor the names of its contributors
    may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```
