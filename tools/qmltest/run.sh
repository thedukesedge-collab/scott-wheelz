#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Part of Refloat Glow, 2026. See FORK_NOTES.md.
# Refloat Glow: load ui.qml in qmlscene with stub VESC Tool modules and print
# every QML warning/error. Usage: tools/qmltest/run.sh <commit>...
set -u
here=$(cd "$(dirname "$0")" && pwd)
stubs=/tmp/qmlstubs
rm -rf $stubs && mkdir -p $stubs/Vedder/vesc
for m in vescinterface codeloader commands configparams utility; do
  mkdir -p $stubs/Vedder/vesc/$m
done
cat > $stubs/Vedder/vesc/commands/qmldir <<'Q'
module Vedder.vesc.commands
Commands 1.0 Commands.qml
Q
cat > $stubs/Vedder/vesc/commands/Commands.qml <<'Q'
import QtQuick 2.15
QtObject {
    signal customAppDataReceived(var data)
    signal valuesSetupReceived(var values, var mask)
    signal valuesImuReceived(var values, var mask)
    function getValuesSetup() {}
    function getValuesSetupSelective(m) {}
    function sendCustomAppData(d) {}
    function getMcconf() {}
    function setMcconf(c) {}
    function lispSendReplCmd(c) {}
    function getImuData(m) {}
    function emitEmptySetupValues() {}
}
Q
cat > $stubs/Vedder/vesc/configparams/qmldir <<'Q'
module Vedder.vesc.configparams
ConfigParams 1.0 ConfigParams.qml
Q
cat > $stubs/Vedder/vesc/configparams/ConfigParams.qml <<'Q'
import QtQuick 2.15
QtObject {
    signal paramChangedDouble(var src, string name, real newParam)
    signal paramChangedInt(var src, string name, int newParam)
    signal paramChangedEnum(var src, string name, int newParam)
    signal paramChangedBool(var src, string name, bool newParam)
    signal updated()
    function getParamInt(n) { return 20 }
    function getParamDouble(n) { return 10 }
    function getParamEnum(n, d) { return 0 }
    function getParamBool(n) { return false }
    function getParamQString(n) { return "" }
    function isParamDouble(n) { return true }
    function isParamInt(n) { return false }
    function isParamEnum(n) { return false }
    function isParamBool(n) { return false }
    function isParamQString(n) { return false }
    function isParamBitfield(n) { return false }
    function getParamOrder() { return [] }
    function getXmlConfig() { return "" }
}
Q
cat > $stubs/Vedder/vesc/codeloader/qmldir <<'Q'
module Vedder.vesc.codeloader
CodeLoader 1.0 CodeLoader.qml
Q
cat > $stubs/Vedder/vesc/codeloader/CodeLoader.qml <<'Q'
import QtQuick 2.15
QtObject {
    property var vesc
    function reloadPackageArchive() { return [] }
    function getPackageFromPath(p) { return {} }
}
Q
cat > $stubs/Vedder/vesc/utility/qmldir <<'Q'
module Vedder.vesc.utility
singleton Utility 1.0 Utility.qml
Q
cat > $stubs/Vedder/vesc/utility/Utility.qml <<'Q'
pragma Singleton
import QtQuick 2.15
QtObject {
    function getAppHexColor(n) { return "#888888" }
    function isDarkMode() { return true }
    function requestFilePermission() { return true }
}
Q
cat > $stubs/Vedder/vesc/vescinterface/qmldir <<'Q'
module Vedder.vesc.vescinterface
singleton VescIf 1.0 VescIf.qml
Q
cat > $stubs/Vedder/vesc/vescinterface/VescIf.qml <<'Q'
pragma Singleton
import QtQuick 2.15
import Vedder.vesc.commands 1.0
import Vedder.vesc.configparams 1.0
QtObject {
    signal customConfigLoadDone()
    signal statusMessage(string msg, bool isGood)
    property Commands cmds: Commands {}
    property ConfigParams cfg: ConfigParams {}
    function commands() { return cmds }
    function customConfig(i) { return cfg }
    function mcConfig() { return cfg }
    function appConfig() { return cfg }
    function customConfigsLoaded() { return true }
    function useImperialUnits() { return true }
    function emitStatusMessage(m, g) {}
    function emitMessageDialog(t, m, g, r) {}
    function getLastBleAddr() { return "" }
    function storeSettings() {}
    function storeBleName(a, n) {}
    function getBleName(a) { return "" }
    function getLastFwRxParams() { return {} }
}
Q
cat > /tmp/wrapper.qml <<'Q'
import QtQuick 2.15
import QtQuick.Controls 2.15
ApplicationWindow {
    width: 393; height: 852  // a typical phone (393 x 852); visible: true
    Timer { id: quitTimer; interval: 500; onTriggered: Qt.quit() }
    Loader {
        id: l
        anchors.fill: parent
        source: "file:///tmp/ui.qml"
        onStatusChanged: {
            if (status == Loader.Error) console.log("LOADER ERROR: " + sourceComponent);
            if (status == Loader.Ready) console.log("LOADER READY, root size " + item.width + "x" + item.height);
        }
    }
    Timer {
        interval: 3000; running: true
        onTriggered: {
            // hide first-run dialogs / hint overlays so the screen is visible
            var root = l.parent; while (root.parent) root = root.parent;
            root.grabToImage(function(r) { r.saveToFile("/tmp/shot.png"); });
            function dump(it, depth) {
                if (depth > 4 || !it) return;
                for (var i = 0; i < it.children.length; i++) {
                    var c = it.children[i];
                    if (c.objectName || depth <= 2)
                        console.log("  ".repeat(depth) + c + " y=" + Math.round(c.y) + " h=" + Math.round(c.height) + " w=" + Math.round(c.width) + " visible=" + c.visible);
                    dump(c, depth + 1);
                }
            }
            if (l.item) dump(l.item, 0);
            quitTimer.start();
        }
    }
}
Q
cd "$here/../.."
mkdir -p /tmp/shots
# shots: 0 home, 1-3 MIXER (BLANK / NOTES / 123), 4 LEDS > HEAD/TAIL LIGHTS
# SIMPLE, 5 the same ADVANCED, 6 LEDS > STATUS BAR (Refloat LED view)
for c in "$@"; do
  for shot in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17; do
    echo "=================== $c (shot $shot)"
    mkdir -p ~/.config/QtProject
    printf "[uiHints]\nstatusText=true\nplotCursor=true\ndrGeneral=true\ndrStartStop=true\ndrDownload=true\n" > ~/.config/QtProject/qmlscene.conf
    case $shot in
      0) act="" ;;
      1|2|3) act="bottomSheet.open(); bottomStackTabs.currentIndex = 2; swSettings.mixerLabels = $((shot - 1));" ;;
      4|5) act="ledSender.rgbAvail = true; ledSender.rfStatusAvail = true; ledSender.rgbLoaded = true; ledSender.rgb = [{mode: $([ $shot = 5 ] && echo 2 || echo 0), c1: 1, c2: 0, speed: 1, bri: 1}, {mode: 0, c1: 4, c2: 0, speed: 1, bri: 1}, {mode: 1, c1: 1, c2: 0, speed: 1, bri: 0.5}, {mode: 0, c1: 4, c2: 0, speed: 1, bri: 0.5}]; swSettings.rgbAdvanced = $([ $shot = 5 ] && echo true || echo false); bottomSheet.open(); bottomStackTabs.currentIndex = 5; if (ledWindow.section !== 'head' || !ledWindow.visible) ledWindow.show('head');" ;;
      6) act="ledSender.rgbAvail = true; ledSender.rfStatusAvail = true; bottomSheet.open(); bottomStackTabs.currentIndex = 5; ledWindow.show('bar');" ;;
      7) act="ledSender.rgbAvail = true; ledSender.rfStatusAvail = true; bottomSheet.open(); bottomStackTabs.currentIndex = 5; ledWindow.close(); bottomSheet.maxHeight = mainItem.height * 0.92;" ;;
      8) act="snoozeHelp.open();" ;;
      9) act="fatalHelp.open();" ;;
      10) act="disclaimerDialog.open();" ;;
      11) act="if (!rinkTour.visible) { rinkTour.open(); tourSwipe.currentIndex = 5; }" ;;
      12) act="if (!rinkTour.visible) { rinkTour.open(); tourSwipe.currentIndex = 12; }" ;;
      15) act="pushbackCfg.loaded = true; pushbackWindow.advanced = true; pushbackWindow.open();" ;;
      16) act="settingsDialog.show(2);" ;;
      17) act="if (!rinkTour.visible) { rinkTour.open(); tourSwipe.currentIndex = 1; var n = 0, drops = 0, last = false; var t = Qt.createQmlObject('import QtQuick 2.0; Timer { interval: 16; repeat: true }', rinkTour); t.triggered.connect(function () { n++; if (n % 12 === 0 && tourSwipe.currentIndex < 13) tourSwipe.incrementCurrentIndex(); if (n > 20 && last && !rinkTour.hasSpot) drops++; last = rinkTour.hasSpot; if (n === 12 * 13 + 30) console.log('TOURDROPS ' + drops + ' at page ' + tourSwipe.currentIndex); }); t.start(); }" ;;
      13) act="if (!rinkTour.visible) { rinkTour.open(); tourSwipe.currentIndex = 4; }" ;;
      14) act="if (!rinkTour.visible) { rinkTour.open(); tourSwipe.currentIndex = 13; }" ;;
    esac
    git show $c:ui.qml.in | sed "s/{{PACKAGE_NAME}}/Refloat Glow/g; s/{{VERSION}}/test/g; s/{{LCM_FW_JSON}}/null/g" \
      | sed "s/category: \"scottWheelz\"/category: \"scottWheelzTest$shot\"/" \
      | ACT="$act" python3 -c "
import os, sys
s = sys.stdin.read()
i = s.rstrip().rfind('}')
t = '    Timer { interval: 400; repeat: true; running: true; onTriggered: { try { if (typeof mainItem.startNoticeDone !== \"undefined\") mainItem.startNoticeDone = true; if (typeof disclaimerDialog !== \"undefined\") disclaimerDialog.close(); ' + ('' if 'rinkTour' in os.environ['ACT'] else 'if (typeof rinkTour !== \"undefined\" && rinkTour.visible) rinkTour.close(); ') + '} catch (e) {} ' + os.environ['ACT'] + ' } }\n'
sys.stdout.write(s[:i] + t + s[i:])" \
      | python3 rjsmin.py > /tmp/ui.qml
    QML2_IMPORT_PATH=$stubs QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software timeout 30 \
      /usr/lib/qt5/bin/qmlscene /tmp/wrapper.qml 2>&1 \
      | grep -v "Cannot read property\|is not a function\|of null\|of undefined\|Unable to assign \[undefined\]\|is not defined" | head -120
    mv /tmp/shot.png /tmp/shots/${c}_sheet$shot.png 2>/dev/null || echo "no screenshot"
  done
done
