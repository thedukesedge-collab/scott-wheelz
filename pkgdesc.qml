// Modified for SLCM for Refloat (part of Refloat Glow), 2026. See SLCM_NOTES.md.
import QtQuick 2.15

Item {
    property string pkgName: "SLCM for Refloat"
    property string pkgDescriptionMd: "package_README-gen.md"
    property string pkgLisp: "lisp/package.lisp"
    property string pkgQml: "ui.qml"
    property bool pkgQmlIsFullscreen: false
    property string pkgOutput: "slcm-for-refloat.vescpkg"

    function isCompatible (fwRxParams) {
        if (fwRxParams.hwTypeStr().toLowerCase() != "vesc") {
            return false;
        }

        return true;
    }
}
