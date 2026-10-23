# CPack reads this before initializing each generator.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../packaging/windows")
if(CPACK_GENERATOR STREQUAL "NSIS")
    file(TO_NATIVE_PATH "${CPACK_NSIS_PRODUCT_MESSAGES}" CPACK_NSIS_PRODUCT_MESSAGES)
endif()
