// SPDX-License-Identifier: GPL-3.0-or-later
// FloLive: start / update / end the lock-screen ride card from the app's
// JavaScript (src/app.js), and pass the light button back to it.
import Foundation
import Capacitor
#if canImport(ActivityKit)
import ActivityKit
#endif

@objc(FloLivePlugin)
public class FloLivePlugin: CAPPlugin, CAPBridgedPlugin {
    public let identifier = "FloLivePlugin"
    public let jsName = "FloLive"
    public let pluginMethods: [CAPPluginMethod] = [
        CAPPluginMethod(name: "supported", returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "start", returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "update", returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "end", returnType: CAPPluginReturnPromise),
    ]
    private var observer: NSObjectProtocol?

    override public func load() {
        observer = NotificationCenter.default.addObserver(
            forName: Notification.Name("FloToggleLights"), object: nil, queue: .main
        ) { [weak self] _ in
            self?.notifyListeners("lightToggle", data: [:], retainUntilConsumed: true)
        }
    }

    @objc func supported(_ call: CAPPluginCall) {
        var ok = false
        var button = false
        if #available(iOS 16.2, *) { ok = ActivityAuthorizationInfo().areActivitiesEnabled }
        if #available(iOS 17.0, *) { button = true }
        call.resolve(["supported": ok, "button": button])
    }

    @available(iOS 16.2, *)
    private func content(_ call: CAPPluginCall) -> ActivityContent<FloRideAttributes.ContentState> {
        let raw = call.getArray("items", JSObject.self) ?? []
        let items = raw.prefix(4).map {
            FloRideAttributes.Item(
                label: $0["label"] as? String ?? "",
                value: $0["value"] as? String ?? "",
                unit: $0["unit"] as? String ?? "")
        }
        let state = FloRideAttributes.ContentState(
            items: Array(items),
            status: call.getString("status") ?? "",
            live: call.getBool("live") ?? true,
            lightsOn: call.getBool("lightsOn") ?? false)
        return ActivityContent(state: state, staleDate: Date().addingTimeInterval(120))
    }

    @objc func start(_ call: CAPPluginCall) {
        guard #available(iOS 16.2, *) else { call.resolve(["started": false]); return }
        let attrs = FloRideAttributes(
            board: call.getString("board") ?? "Board",
            lightLabel: call.getString("lightLabel") ?? "")
        let c = content(call)
        Task {
            for a in Activity<FloRideAttributes>.activities { await a.end(nil, dismissalPolicy: .immediate) }
            do {
                _ = try Activity.request(attributes: attrs, content: c, pushType: nil)
                call.resolve(["started": true])
            } catch {
                call.reject(error.localizedDescription)
            }
        }
    }

    @objc func update(_ call: CAPPluginCall) {
        guard #available(iOS 16.2, *) else { call.resolve(); return }
        let c = content(call)
        Task {
            for a in Activity<FloRideAttributes>.activities { await a.update(c) }
            call.resolve(["active": !Activity<FloRideAttributes>.activities.isEmpty])
        }
    }

    @objc func end(_ call: CAPPluginCall) {
        guard #available(iOS 16.2, *) else { call.resolve(); return }
        Task {
            for a in Activity<FloRideAttributes>.activities { await a.end(nil, dismissalPolicy: .immediate) }
            call.resolve()
        }
    }
}
