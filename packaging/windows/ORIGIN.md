# NSIS template provenance

Source: Kitware CMake 3.30.5, `Modules/Internal/CPack/NSIS.template.in`.
Upstream: https://github.com/Kitware/CMake/blob/v3.30.5/Modules/Internal/CPack/NSIS.template.in
License: BSD-3-Clause, reproduced in CMake-LICENSE.txt.

Local changes: replace installer/uninstaller `.onInit` callbacks with machine-wide
initialization using the 64-bit registry view and all-user shortcuts. Remember the
machine installation directory while preserving an explicit /D override. Never
redirect the default destination to Documents. Other template behavior is upstream.
This small behavioral patch is kept in the template so the directory page and
actual destination agree. Packaging.cmake owns product-specific options.
