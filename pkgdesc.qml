// Part of the Refloat VESC package (GPL-3.0-or-later, see LICENSE).
// Modified by Refloat Glow, 2026. See FORK_NOTES.md.

import QtQuick 2.15

Item {
    property string pkgName: "Refloat Glow"
    property string pkgDescriptionMd: "package_README-gen.md"
    property string pkgLisp: "lisp/package.lisp"
    property string pkgQml: "ui.qml"
    property bool pkgQmlIsFullscreen: false
    property string pkgOutput: "refloatrm.vescpkg"

    function isCompatible (fwRxParams) {
        if (fwRxParams.hwTypeStr().toLowerCase() != "vesc") {
            return false;
        }

        return true;
    }
}
