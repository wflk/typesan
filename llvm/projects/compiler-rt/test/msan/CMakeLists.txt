set(MSAN_LIT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(MSAN_TESTSUITES)

set(MSAN_TEST_ARCH ${MSAN_SUPPORTED_ARCH})
if(APPLE)
  darwin_filter_host_archs(MSAN_SUPPORTED_ARCH MSAN_TEST_ARCH)
endif()

foreach(arch ${MSAN_TEST_ARCH})
  set(MSAN_TEST_TARGET_ARCH ${arch})
  string(TOLOWER "-${arch}" MSAN_TEST_CONFIG_SUFFIX)
  if(ANDROID OR ${arch} MATCHES "arm|aarch64")
    # This is only true if we are cross-compiling.
    # Build all tests with host compiler and use host tools.
    set(MSAN_TEST_TARGET_CC ${COMPILER_RT_TEST_COMPILER})
    set(MSAN_TEST_TARGET_CFLAGS ${COMPILER_RT_TEST_COMPILER_CFLAGS})
  else()
    get_target_flags_for_arch(${arch} MSAN_TEST_TARGET_CFLAGS)
    string(REPLACE ";" " " MSAN_TEST_TARGET_CFLAGS "${MSAN_TEST_TARGET_CFLAGS}")
  endif()

  string(TOUPPER ${arch} ARCH_UPPER_CASE)
  set(CONFIG_NAME ${ARCH_UPPER_CASE}Config)

  configure_lit_site_cfg(
    ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
    ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_NAME}/lit.site.cfg)
  list(APPEND MSAN_TESTSUITES ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_NAME})
endforeach()

set(MSAN_TEST_DEPS ${SANITIZER_COMMON_LIT_TEST_DEPS})
if(NOT COMPILER_RT_STANDALONE_BUILD)
  list(APPEND MSAN_TEST_DEPS msan)
endif()

if(COMPILER_RT_INCLUDE_TESTS AND COMPILER_RT_HAS_LIBCXX_SOURCES)
  configure_lit_site_cfg(
    ${CMAKE_CURRENT_SOURCE_DIR}/Unit/lit.site.cfg.in
    ${CMAKE_CURRENT_BINARY_DIR}/Unit/lit.site.cfg)
  list(APPEND MSAN_TEST_DEPS MsanUnitTests)
  list(APPEND MSAN_TESTSUITES ${CMAKE_CURRENT_BINARY_DIR}/Unit)
endif()

add_lit_testsuite(check-msan "Running the MemorySanitizer tests"
  ${MSAN_TESTSUITES}
  DEPENDS ${MSAN_TEST_DEPS}
  )
set_target_properties(check-msan PROPERTIES FOLDER "MSan tests")
