// SPDX-License-Identifier: GPL-3.0-or-later
// FloPilot lock-screen ride card + Dynamic Island (Neon Future look).
import ActivityKit
import AppIntents
import SwiftUI
import WidgetKit

enum Neon {
    static let green = Color(red: 0.22, green: 1.0, blue: 0.48)
    static let purple = Color(red: 0.69, green: 0.30, blue: 1.0)
    static let amber = Color(red: 1.0, green: 0.82, blue: 0.25)
    static let dim = Color(red: 0.72, green: 0.62, blue: 0.90)
    static let bg = Color(red: 0.04, green: 0.02, blue: 0.10)
}

@main
struct FloPilotWidgets: WidgetBundle {
    var body: some Widget {
        FloRideLive()
    }
}

struct FloRideLive: Widget {
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: FloRideAttributes.self) { ctx in
            LockView(ctx: ctx)
                .activityBackgroundTint(Neon.bg)
                .activitySystemActionForegroundColor(Neon.green)
        } dynamicIsland: { ctx in
            DynamicIsland {
                DynamicIslandExpandedRegion(.leading) {
                    ItemView(item: ctx.state.items.first, big: true)
                }
                DynamicIslandExpandedRegion(.trailing) {
                    LightButton(ctx: ctx)
                }
                DynamicIslandExpandedRegion(.bottom) {
                    HStack {
                        ForEach(Array(ctx.state.items.dropFirst().enumerated()), id: \.offset) { pair in
                            ItemView(item: pair.element, big: false).frame(maxWidth: .infinity, alignment: .leading)
                        }
                    }
                }
            } compactLeading: {
                Text(ctx.state.items.first?.value ?? "-").bold().foregroundColor(Neon.green)
            } compactTrailing: {
                Text(ctx.state.items.count > 1 ? ctx.state.items[1].value + ctx.state.items[1].unit : "")
                    .foregroundColor(Neon.purple)
            } minimal: {
                Text(ctx.state.items.first?.value ?? "-").foregroundColor(Neon.green)
            }
        }
    }
}

struct LockView: View {
    let ctx: ActivityViewContext<FloRideAttributes>

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(ctx.attributes.board.uppercased()).font(.caption.bold()).foregroundColor(Neon.green)
                Spacer()
                Text(ctx.state.status).font(.caption2.bold())
                    .foregroundColor(ctx.state.live ? Neon.green : Neon.amber)
            }
            HStack(alignment: .center, spacing: 10) {
                ItemView(item: ctx.state.items.first, big: true)
                Spacer(minLength: 4)
                LightButton(ctx: ctx)
            }
            if ctx.state.items.count > 1 {
                HStack {
                    ForEach(Array(ctx.state.items.dropFirst().enumerated()), id: \.offset) { pair in
                        ItemView(item: pair.element, big: false).frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
            }
        }
        .padding(14)
    }
}

struct ItemView: View {
    let item: FloRideAttributes.Item?
    let big: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline, spacing: 2) {
                Text(item?.value ?? "-")
                    .font(.system(size: big ? 34 : 18, weight: .bold, design: .rounded))
                    .monospacedDigit()
                    .foregroundColor(Neon.green)
                Text(item?.unit ?? "")
                    .font(.system(size: big ? 14 : 10, weight: .semibold))
                    .foregroundColor(Neon.purple)
            }
            Text(item?.label ?? "").font(.system(size: 9, weight: .bold)).foregroundColor(Neon.dim)
        }
    }
}

struct LightButton: View {
    let ctx: ActivityViewContext<FloRideAttributes>

    var body: some View {
        if ctx.attributes.lightLabel.isEmpty {
            EmptyView()
        } else if #available(iOS 17.0, *) {
            Button(intent: ToggleLightsIntent()) {
                VStack(spacing: 2) {
                    Image(systemName: ctx.state.lightsOn ? "lightbulb.fill" : "lightbulb")
                        .font(.system(size: 20, weight: .bold))
                    Text(ctx.attributes.lightLabel + (ctx.state.lightsOn ? " ON" : " OFF"))
                        .font(.system(size: 9, weight: .bold))
                }
                .foregroundColor(ctx.state.lightsOn ? Neon.green : Neon.dim)
                .padding(.horizontal, 8)
                .padding(.vertical, 6)
            }
            .buttonStyle(.plain)
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(ctx.state.lightsOn ? Neon.green : Neon.purple, lineWidth: 1.5))
        } else {
            EmptyView()
        }
    }
}
