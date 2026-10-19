import QtQuick
Palette {
    id: root
    property SystemPalette activeColors: SystemPalette { colorGroup: SystemPalette.Active }
    property SystemPalette inactiveColors: SystemPalette { colorGroup: SystemPalette.Inactive }
    property SystemPalette disabledColors: SystemPalette { colorGroup: SystemPalette.Disabled }
    active {
        window: root.activeColors.window
        windowText: root.activeColors.windowText
        base: root.activeColors.base
        alternateBase: root.activeColors.alternateBase
        text: root.activeColors.text
        button: root.activeColors.button
        buttonText: root.activeColors.buttonText
        highlight: root.activeColors.highlight
        highlightedText: root.activeColors.highlightedText
        placeholderText: root.activeColors.placeholderText
        light: root.activeColors.light
        midlight: root.activeColors.midlight
        dark: root.activeColors.dark
        mid: root.activeColors.mid
        shadow: root.activeColors.shadow
        accent: root.activeColors.accent
    }
    inactive {
        window: root.inactiveColors.window
        windowText: root.inactiveColors.windowText
        base: root.inactiveColors.base
        alternateBase: root.inactiveColors.alternateBase
        text: root.inactiveColors.text
        button: root.inactiveColors.button
        buttonText: root.inactiveColors.buttonText
        highlight: root.inactiveColors.highlight
        highlightedText: root.inactiveColors.highlightedText
        placeholderText: root.inactiveColors.placeholderText
        light: root.inactiveColors.light
        midlight: root.inactiveColors.midlight
        dark: root.inactiveColors.dark
        mid: root.inactiveColors.mid
        shadow: root.inactiveColors.shadow
        accent: root.inactiveColors.accent
    }
    disabled {
        window: root.disabledColors.window
        windowText: root.disabledColors.windowText
        base: root.disabledColors.base
        alternateBase: root.disabledColors.alternateBase
        text: root.disabledColors.text
        button: root.disabledColors.button
        buttonText: root.disabledColors.buttonText
        highlight: root.disabledColors.highlight
        highlightedText: root.disabledColors.highlightedText
        placeholderText: root.disabledColors.placeholderText
        light: root.disabledColors.light
        midlight: root.disabledColors.midlight
        dark: root.disabledColors.dark
        mid: root.disabledColors.mid
        shadow: root.disabledColors.shadow
        accent: root.disabledColors.accent
    }
}
