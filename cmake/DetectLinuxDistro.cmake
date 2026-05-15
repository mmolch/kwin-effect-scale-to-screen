# DetectLinuxDistro.cmake
#
# Usage:
#   detect_linux_distro(DISTRO_NAME DISTRO_VERSION)
#
# Output variables:
#   <DISTRO_NAME>    - e.g. "Ubuntu", "Arch Linux", "Fedora"
#   <DISTRO_VERSION> - e.g. "22.04", "rolling", "40"

function(detect_linux_distro OUT_NAME OUT_VERSION)
    set(OS_RELEASE_FILE "/etc/os-release")

    if(EXISTS "${OS_RELEASE_FILE}")
        file(READ "${OS_RELEASE_FILE}" OS_RELEASE_CONTENTS)
        string(PREPEND OS_RELEASE_CONTENTS "\n")

        # Extract NAME=
        string(REGEX MATCH "\n *ID=\"?([^\n\"]+)\"?" _ "${OS_RELEASE_CONTENTS}")
        set(DISTRO_NAME "${CMAKE_MATCH_1}")

        # Extract VERSION_ID=
        string(REGEX MATCH "\n *VERSION_ID=\"?([^\n\"]+)\"?" _ "${OS_RELEASE_CONTENTS}")
        set(DISTRO_VERSION "${CMAKE_MATCH_1}")

        # Fallback if VERSION_ID is missing (e.g. Arch)
        if(NOT DISTRO_VERSION)
            string(REGEX MATCH "\n *VERSION=\"?([^\n\"]+)\"?" _ "${OS_RELEASE_CONTENTS}")
            set(DISTRO_VERSION "${CMAKE_MATCH_1}")
        endif()

        set(${OUT_NAME} "${DISTRO_NAME}" PARENT_SCOPE)
        set(${OUT_VERSION} "${DISTRO_VERSION}" PARENT_SCOPE)
        return()
    endif()

    # Fallback: try lsb_release
    find_program(LSB_RELEASE_EXEC lsb_release)
    if(LSB_RELEASE_EXEC)
        execute_process(
            COMMAND ${LSB_RELEASE_EXEC} -si
            OUTPUT_VARIABLE DISTRO_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND ${LSB_RELEASE_EXEC} -sr
            OUTPUT_VARIABLE DISTRO_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        set(${OUT_NAME} "${DISTRO_NAME}" PARENT_SCOPE)
        set(${OUT_VERSION} "${DISTRO_VERSION}" PARENT_SCOPE)
        return()
    endif()

    # Last-resort fallback
    set(${OUT_NAME} "Unknown" PARENT_SCOPE)
    set(${OUT_VERSION} "Unknown" PARENT_SCOPE)
endfunction()

