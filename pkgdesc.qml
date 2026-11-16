import QtQuick 2.15

Item {
    property string pkgName: "Rink-Float"
    property string pkgDescriptionMd: "package_README-gen.md"
    property string pkgLisp: "lisp/package.lisp"
    property string pkgQml: "ui.qml"
    property bool pkgQmlIsFullscreen: false
    property string pkgOutput: "rinkfloat.vescpkg"

    function isCompatible (fwRxParams) {
        if (fwRxParams.hwTypeStr().toLowerCase() != "vesc") {
            return false;
        }

        return true;
    }
}
