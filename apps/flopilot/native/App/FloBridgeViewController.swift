// SPDX-License-Identifier: GPL-3.0-or-later
// Capacitor's view controller plus the app's own plugin (FloLive).
import UIKit
import Capacitor

class FloBridgeViewController: CAPBridgeViewController {
    override open func capacitorDidLoad() {
        bridge?.registerPluginInstance(FloLivePlugin())
    }
}
