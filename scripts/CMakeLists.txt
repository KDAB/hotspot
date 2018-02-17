configure_file(
    elevate_perf_privileges.sh
    "${PROJECT_BINARY_DIR}/${KDE_INSTALL_LIBEXECDIR}/elevate_perf_privileges.sh"
    @ONLY
)

install(PROGRAMS elevate_perf_privileges.sh DESTINATION ${KDE_INSTALL_LIBEXECDIR})
