cmake_minimum_required (VERSION 3.5)
project (RN-Praxis)
set (CMAKE_C_STANDARD 11)

# Packaging
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
  ${CMAKE_BINARY_DIR}
  /\\..*$
)
add_executable(webserver
        src/utils.c src/main.c
        src/udp.c
        src/udp.h)
set(CMAKE_BUILD_TYPE Debug)
set(CPACK_VERBATIM_VARIABLES YES)
find_package(OpenSSL REQUIRED)
target_link_libraries(webserver PRIVATE ${OPENSSL_LIBRARIES} -lm)
include(CPack)