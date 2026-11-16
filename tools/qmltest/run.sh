#!/bin/bash
# Rink-Float: load ui.qml in qmlscene with stub VESC Tool modules and print
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
    width: 395; height: 703; visible: true
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
            if (l.item) for (var k = 0; k < l.item.children.length; k++) {
                var ch = l.item.children[k];
                if (ch.children.length > 0 && /Page|SwipeView/.test("" + ch.children[0])) ch.visible = false;
            }
            l.item && l.item.parent && l.item.parent.grabToImage(function(r) { r.saveToFile("/tmp/shot.png"); });
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
for c in "$@"; do
  for frac in 0 1; do
    echo "=================== $c (sheetFraction=$frac)"
    mkdir -p ~/.config/QtProject
    printf "[scottWheelz]\nsheetFraction=$frac\nrailStyle=$frac\n" > ~/.config/QtProject/qmlscene.conf
    git show $c:ui.qml.in | sed "s/{{PACKAGE_NAME}}/Rink-Float/g; s/{{VERSION}}/test/g" | python3 rjsmin.py > /tmp/ui.qml
    QML2_IMPORT_PATH=$stubs QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software timeout 30 \
      /usr/lib/qt5/bin/qmlscene /tmp/wrapper.qml 2>&1 \
      | grep -v "Cannot read property\|is not a function\|of null\|of undefined\|Unable to assign \[undefined\]\|is not defined" | head -120
    mv /tmp/shot.png /tmp/shots/${c}_sheet$frac.png 2>/dev/null || echo "no screenshot"
  done
done
