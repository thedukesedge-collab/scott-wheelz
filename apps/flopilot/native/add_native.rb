# SPDX-License-Identifier: GPL-3.0-or-later
# Adds FloPilot's native parts to the Capacitor iOS project (CI, after
# `npx cap sync ios`): the FloLive plugin + lock-screen widget extension.
# Usage: ruby add_native.rb ios/App/App.xcodeproj
require 'xcodeproj'

p = Xcodeproj::Project.open(ARGV[0])
app = p.targets.find { |t| t.name == 'App' } or abort 'App target not found'
app_group = p.main_group['App'] or abort 'App group not found'

app_files = %w[FloRideAttributes.swift ToggleLightsIntent.swift FloLivePlugin.swift FloBridgeViewController.swift]
refs = app_files.to_h { |f| [f, app_group.new_reference(f)] }
app.add_file_references(refs.values)
app.build_configurations.each { |c| c.build_settings['IPHONEOS_DEPLOYMENT_TARGET'] = '15.0' }

w = p.new_target(:app_extension, 'FloPilotWidget', :ios, '16.2', nil, :swift)
wg = p.main_group.new_group('FloPilotWidget', 'FloPilotWidget')
w.add_file_references([wg.new_reference('FloPilotWidget.swift'), refs['FloRideAttributes.swift'], refs['ToggleLightsIntent.swift']])
wg.new_reference('Info.plist')
w.build_configurations.each do |c|
  s = c.build_settings
  s['PRODUCT_BUNDLE_IDENTIFIER'] = 'com.flopilot.app.widget'
  s['PRODUCT_NAME'] = '$(TARGET_NAME)'
  s['INFOPLIST_FILE'] = 'FloPilotWidget/Info.plist'
  s['GENERATE_INFOPLIST_FILE'] = 'NO'
  s['SWIFT_VERSION'] = '5.0'
  s['TARGETED_DEVICE_FAMILY'] = '1,2'
  s['IPHONEOS_DEPLOYMENT_TARGET'] = '16.2'
  s['MARKETING_VERSION'] = '1.0'
  s['SKIP_INSTALL'] = 'YES'
  s['APPLICATION_EXTENSION_API_ONLY'] = 'YES'
  s['CODE_SIGN_STYLE'] = 'Automatic'
  s['LD_RUNPATH_SEARCH_PATHS'] = ['$(inherited)', '@executable_path/Frameworks', '@executable_path/../../Frameworks']
end

app.add_dependency(w)
embed = p.new(Xcodeproj::Project::Object::PBXCopyFilesBuildPhase)
embed.name = 'Embed Foundation Extensions'
embed.symbol_dst_subfolder_spec = :plug_ins
bf = embed.add_file_reference(w.product_reference, true)
bf.settings = { 'ATTRIBUTES' => ['RemoveHeadersOnCopy'] }
idx = app.build_phases.index { |ph| ph.is_a?(Xcodeproj::Project::Object::PBXFrameworksBuildPhase) } || 0
app.build_phases.insert(idx + 1, embed)

p.save
puts 'FloPilot native parts added'
