add_subdirectory(libtapi)
# Minkar: the tapi tool and its companions are not built; LLVM's own
# llvm-readtapi --stubify writes the bootstrap stubs
# (Libraries/bootstrap/regen-stubs.sh).
if(NOT TAPI_MINKAR_LIBRARY_ONLY)
  add_subdirectory(tapi)
  add_subdirectory(tapi-binary-reader)
  add_subdirectory(tapi-frontend)
  if(NOT (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux"))
      add_subdirectory(tapi-run)
  endif()
  add_subdirectory(tapi-sdkdb)
  add_subdirectory(api-json-diff)
endif()
