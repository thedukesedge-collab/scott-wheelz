#!/usr/bin/env python3
"""Refloat Glow: replaces {{LCM_FW_JSON}} in ui.qml.in.

With an image (make LCM_FW=1): the SLCM update image (ALCM_VP_app.json from
lcm_images.py) - {size, crc, version, b64}. Without (the default since
rf70): just {version} - the SLCM build that belongs to this package, so
CHECK LCM can say up to date / older / newer; flashing then uses a .hex the
rider picks. Writes to stdout.

usage: embed_lcm_fw.py ui.qml.in [ALCM_VP_app.json or ""] [version]
"""
import json
import sys

qml = open(sys.argv[1]).read()
value = "null"
if len(sys.argv) > 2 and sys.argv[2]:
    fw = json.load(open(sys.argv[2]))
    value = json.dumps({k: fw[k] for k in ("size", "crc", "version", "b64")}, separators=(",", ":"))
elif len(sys.argv) > 3 and sys.argv[3]:
    value = json.dumps({"version": sys.argv[3]}, separators=(",", ":"))
if qml.count("{{LCM_FW_JSON}}") != 1:
    sys.exit("expected exactly one {{LCM_FW_JSON}} in " + sys.argv[1])
sys.stdout.write(qml.replace("{{LCM_FW_JSON}}", value))
