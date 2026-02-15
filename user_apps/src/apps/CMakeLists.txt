if(USER_APPS_BUILD_APPS_BASIC)
  add_subdirectory(basic)
endif()

if(USER_APPS_BUILD_APPS_COMMUNICATION)
  add_subdirectory(communication)
endif()

if(USER_APPS_BUILD_APPS_FEATURE)
  add_subdirectory(feature)
endif()
