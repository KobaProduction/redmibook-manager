get_filename_component(project_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
execute_process(COMMAND git config --local core.hooksPath .githooks
    WORKING_DIRECTORY "${project_root}" COMMAND_ERROR_IS_FATAL ANY)
if(UNIX)
    file(CHMOD "${CMAKE_CURRENT_LIST_DIR}/../.githooks/pre-push"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()
message(STATUS "Installed tracked pre-push hook; verify with git config --get core.hooksPath")
