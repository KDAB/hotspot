# Statically built libraries export $<LINK_ONLY:Qt6::XxxPrivate> in their link
# interface, but their config files don't look for it, so consumers fail with
# "target Qt6::XxxPrivate was not found". Make the configs find those modules.
# Before Qt 6.9 there's no separate Qt6XxxPrivate package, the target comes with Qt6Xxx.
file(GLOB _targets_files "${CURRENT_PACKAGES_DIR}/share/*/*Targets.cmake")
foreach(_targets IN LISTS _targets_files)
    file(READ "${_targets}" _contents)
    string(REGEX MATCHALL "Qt6::[A-Za-z]+Private" _private_targets "${_contents}")
    list(REMOVE_DUPLICATES _private_targets)
    string(REPLACE "Targets.cmake" "Config.cmake" _config "${_targets}")
    if(_private_targets AND EXISTS "${_config}")
        file(APPEND "${_config}" "\ninclude(CMakeFindDependencyMacro)\n")
        foreach(_target IN LISTS _private_targets)
            string(REPLACE "::" "" _package "${_target}")
            file(APPEND "${_config}" "if(NOT TARGET ${_target})\n    find_dependency(${_package})\nendif()\n")
        endforeach()
    endif()
endforeach()
