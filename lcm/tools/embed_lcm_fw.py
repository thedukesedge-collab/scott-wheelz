#!/usr/bin/env python3
"""Re-Float Rink-Mixed: replaces {{LCM_FW_JSON}} in ui.qml.in with the LCM
update image (ALCM_VP_app.json from lcm_images.py), or null when the
package is built without it (make LCM_FW=0). Writes to stdout.

usage: embed_lcm_fw.py ui.qml.in [ALCM_VP_app.json]
"""
import json
import sys

qml = open(sys.argv[1]).read()
value = "null"
if len(sys.argv) > 2 and sys.argv[2]:
    fw = json.load(open(sys.argv[2]))
    value = json.dumps({k: fw[k] for k in ("size", "crc", "version", "b64")}, separators=(",", ":"))
if qml.count("{{LCM_FW_JSON}}") != 1:
    sys.exit("expected exactly one {{LCM_FW_JSON}} in " + sys.argv[1])
sys.stdout.write(qml.replace("{{LCM_FW_JSON}}", value))
