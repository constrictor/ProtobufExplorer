# The Debian package. It links the distribution's Qt rather than carrying one, and each
# Ubuntu release has its own, so each has its own package, which its version names.

cmake_host_system_information(RESULT distro QUERY DISTRIB_INFO)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_CONTACT "Valentyn Pavliuchenko <valentyn.pavliuchenko@gmail.com>")
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION devel)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
"Opens .proto schemas, shows their messages as trees, and the source of what is
selected beside them. Lays JSON over the message it is of, and shows which fields
it sets, which it leaves out and what is wrong with it.")

# A tilde sorts before anything, so that these make way for a plain 0.1.0, not shadow it.
if(distro_VERSION_ID)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION}~${distro_ID}${distro_VERSION_ID})
endif()

include(CPack)
