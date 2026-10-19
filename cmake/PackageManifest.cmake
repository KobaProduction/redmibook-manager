# The app does not use Qt Lottie; do not accidentally ship this optional GPL module.
file(GLOB_RECURSE unused_lottie LIST_DIRECTORIES FALSE "${CPACK_TEMPORARY_DIRECTORY}/*[Ll]ottie*")
if(unused_lottie)
    message(FATAL_ERROR "Unused Qt Lottie found in the package: ${unused_lottie}")
endif()
# CPack calls this after staging all runtime dependencies, before archive generation.
set(manifest "${CPACK_TEMPORARY_DIRECTORY}/payload.manifest")
file(WRITE "${manifest}" "")
file(GLOB_RECURSE payload_files LIST_DIRECTORIES FALSE RELATIVE "${CPACK_TEMPORARY_DIRECTORY}"
    "${CPACK_TEMPORARY_DIRECTORY}/*")
list(SORT payload_files)
list(JOIN payload_files "\n" manifest_text)
file(WRITE "${manifest}" "${manifest_text}\n")
