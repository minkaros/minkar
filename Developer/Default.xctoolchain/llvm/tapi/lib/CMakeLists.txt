# Minkar: only what libtapi (ld64's reader of .tbd files) needs is built;
# the tapi tool's libraries are left out (see tools/CMakeLists.txt).
if(NOT TAPI_MINKAR_LIBRARY_ONLY)
  add_subdirectory(APIVerifier)
  add_subdirectory(SDKDB)
  add_subdirectory(Driver)
  add_subdirectory(Frontend)
endif()
add_subdirectory(Config)
add_subdirectory(Core)
add_subdirectory(Diagnostics)
add_subdirectory(NoInits)
add_subdirectory(ObjCMetadata)
