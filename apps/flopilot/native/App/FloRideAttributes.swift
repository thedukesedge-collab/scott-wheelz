// SPDX-License-Identifier: GPL-3.0-or-later
// FloPilot lock-screen ride card (Live Activity): the data shared by the app
// (starts / updates it) and the widget extension (draws it).
import Foundation
#if canImport(ActivityKit)
import ActivityKit

@available(iOS 16.1, *)
struct FloRideAttributes: ActivityAttributes {
    struct Item: Codable, Hashable {
        var label: String
        var value: String
        var unit: String
    }

    struct ContentState: Codable, Hashable {
        var items: [Item]      // up to 4, the first one is shown big
        var status: String     // LIVE / RECONNECTING
        var live: Bool
        var lightsOn: Bool
    }

    var board: String
    var lightLabel: String     // "" = no light button
}
#endif
