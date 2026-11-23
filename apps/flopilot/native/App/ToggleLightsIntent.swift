// SPDX-License-Identifier: GPL-3.0-or-later
// Lock-screen light button (iOS 17+). Runs in the app, which sends Refloat's
// LIGHTS_CONTROL command to the board.
import AppIntents
import Foundation

@available(iOS 17.0, *)
struct ToggleLightsIntent: LiveActivityIntent {
    static var title: LocalizedStringResource = "Toggle board lights"
    static var isDiscoverable: Bool = false

    init() {}

    func perform() async throws -> some IntentResult {
        await MainActor.run {
            NotificationCenter.default.post(name: Notification.Name("FloToggleLights"), object: nil)
        }
        return .result()
    }
}
