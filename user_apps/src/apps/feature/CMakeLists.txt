add_subdirectory(runtime)

if(USER_APPS_FEATURE_SUPPORT_AVAILABLE)
  add_subdirectory(can)

  if(USER_APPS_HAS_DDS)
    add_subdirectory(ecu)
  endif()
endif()
