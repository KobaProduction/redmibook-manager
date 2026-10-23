# Third-party software

Redmibook Manager application source is MIT licensed. Its packages dynamically link
unmodified Qt 6.10.2 (Qt Base, Declarative/Quick, and runtime plugins). Qt modules used
here are distributed under LGPL-3.0 with the applicable third-party licenses.
Unused Qt Lottie/vector-image-format plugins are excluded from all graphical-host
deployment passes; a package check rejects accidental Qt Lottie inclusion.
Replacement of those shared libraries and reverse engineering for debugging their
modifications are permitted under the applicable licenses.

Qt source, build instructions, component copyrights and licenses:

- https://download.qt.io/archive/qt/6.10/6.10.2/single/
- https://code.qt.io/cgit/qt/qtbase.git/tree/?h=v6.10.2
- https://code.qt.io/cgit/qt/qtdeclarative.git/tree/?h=v6.10.2
- https://code.qt.io/cgit/qt/qtsvg.git/tree/?h=v6.10.2
- https://code.qt.io/cgit/qt/qtimageformats.git/tree/?h=v6.10.2
- https://code.qt.io/cgit/qt/qttranslations.git/tree/?h=v6.10.2

The `Qt` directory contains the upstream license texts from these pinned modules
and component attribution extracted from Qt's SPDX documents. Some listed components
are not present in every package. Qt is not statically linked into this application;
its included shared libraries can be replaced in the bundle.

Windows packages also contain MinGW GCC runtime, mingw-w64 and winpthreads libraries.
Their upstream notices, GPL texts and GCC Runtime Library Exception accompany them
in `MinGW`. Toolchain source is available from https://www.mingw-w64.org/ and
https://gcc.gnu.org/releases.html (GCC 13.1.0).

The source repository includes Chromium checkdeps under its BSD license, with
provenance and adaptations recorded in `tools/vendor/chromium_checkdeps`. It is a
development-only tool and is not included in runtime packages.

## MingCute icon

The laptop hotkey symbol (`presentation/assets/vendor-key.svg`) is MingCute's
regular `system/switch.svg`, by MingCute Design, licensed under Apache-2.0.
Its geometry and original SVG are unchanged; Qt applies the current UI icon color.

- Source: https://github.com/Richard9394/MingCute/blob/main/assets/svg/core/regular/system/switch.svg
- License: `MingCute/LICENSE` in the installed licenses directory.
