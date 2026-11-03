// Pi emulation settings, stored in the app's user defaults.

import SwiftUI
import UniformTypeIdentifiers

enum SettingsKey {
    static let piEnabled = "piEnabled"
    static let piImagePath = "piImagePath"
}

struct SettingsView: View {
    @AppStorage(SettingsKey.piEnabled) private var piEnabled = false
    @AppStorage(SettingsKey.piImagePath) private var piImagePath = ""
    @State private var choosingImage = false

    var body: some View {
        Form {
            Section {
                Toggle("Pi emulation", isOn: $piEnabled)
                LabeledContent("SD image") {
                    HStack {
                        Text(piImagePath.isEmpty ? "None" : (piImagePath as NSString).lastPathComponent)
                            .foregroundStyle(piImagePath.isEmpty ? .secondary : .primary)
                            .help(piImagePath)
                            .lineLimit(1)
                            .truncationMode(.middle)
                        Button("Choose…") { choosingImage = true }
                    }
                }
                .disabled(!piEnabled)
            } footer: {
                Text("When Pi emulation is on, NextPi runs on the Next's Pi UART from this SD image. When it is off, the Pi port is empty.")
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
        .frame(width: 460)
        .fileImporter(isPresented: $choosingImage, allowedContentTypes: [.diskImage, .data]) { result in
            if case .success(let url) = result {
                piImagePath = url.path
            }
        }
    }
}
