cmake_minimum_required(VERSION 3.14)
project(AdaptiveAutosarUserApps LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

option(USER_APPS_BUILD_APPS_BASIC "Build user apps/basic targets" ON)
option(USER_APPS_BUILD_APPS_COMMUNICATION "Build user apps/communication targets" ON)
option(USER_APPS_BUILD_APPS_FEATURE "Build user apps/feature targets" ON)

set(AUTOSAR_AP_PREFIX "/opt/autosar_ap" CACHE PATH "Installed AUTOSAR AP prefix")
set(_autosar_ap_cmake_libdir "${CMAKE_INSTALL_LIBDIR}")
if(NOT EXISTS "${AUTOSAR_AP_PREFIX}/${_autosar_ap_cmake_libdir}/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake")
  if(EXISTS "${AUTOSAR_AP_PREFIX}/lib/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake")
    set(_autosar_ap_cmake_libdir "lib")
  elseif(EXISTS "${AUTOSAR_AP_PREFIX}/lib64/cmake/AdaptiveAutosarAP/AdaptiveAutosarAPConfig.cmake")
    set(_autosar_ap_cmake_libdir "lib64")
  endif()
endif()

set(
  AdaptiveAutosarAP_DIR
  "${AUTOSAR_AP_PREFIX}/${_autosar_ap_cmake_libdir}/cmake/AdaptiveAutosarAP"
  CACHE PATH
  "Path to AdaptiveAutosarAPConfig.cmake")

find_package(
  AdaptiveAutosarAP REQUIRED CONFIG
  PATHS "${AdaptiveAutosarAP_DIR}"
  NO_DEFAULT_PATH
)

set(USER_APPS_APPS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/apps")
set(USER_APPS_FEATURE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/features")
set(USER_APPS_FEATURE_PUBSUB_DIR "${USER_APPS_FEATURE_ROOT}/communication/pubsub")
set(USER_APPS_FEATURE_VEHICLE_STATUS_DIR "${USER_APPS_FEATURE_ROOT}/communication/vehicle_status")
set(USER_APPS_FEATURE_CAN_DIR "${USER_APPS_FEATURE_ROOT}/communication/can")
set(USER_APPS_FEATURE_ECU_DIR "${USER_APPS_FEATURE_ROOT}/ecu")

function(add_user_template_target target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE ${ARGN})
endfunction()

function(configure_user_apps_dds_app_target target_name)
  if(USER_APPS_DDS_TYPE_AVAILABLE)
    target_link_libraries(
      ${target_name}
      PRIVATE
      user_apps_dds_types
    )
    # Ensure CycloneDDS headers are searched before vsomeip stub headers
    # (vsomeip ships dds/core/macros.hpp stubs that would shadow the real ones).
    if(USER_APPS_CYCLONEDDS_INCLUDE_DIRS)
      target_include_directories(
        ${target_name}
        BEFORE
        PRIVATE
        ${USER_APPS_CYCLONEDDS_INCLUDE_DIRS}
      )
    endif()
    target_include_directories(
      ${target_name}
      PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}"
    )
    target_compile_definitions(
      ${target_name}
      PRIVATE
      USER_APPS_DDS_TYPE_AVAILABLE=1
    )
  endif()
endfunction()

function(configure_user_apps_feature_transport_target target_name)
  target_link_libraries(
    ${target_name}
    PRIVATE
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_com
    AdaptiveAutosarAP::ara_log
  )

  if(USER_APPS_TEMPLATE_DDS_TYPE_AVAILABLE)
    target_link_libraries(
      ${target_name}
      PRIVATE
      user_apps_template_dds_types
    )
    if(USER_APPS_CYCLONEDDS_INCLUDE_DIRS)
      target_include_directories(
        ${target_name}
        BEFORE
        PRIVATE
        ${USER_APPS_CYCLONEDDS_INCLUDE_DIRS}
      )
    endif()
    target_include_directories(
      ${target_name}
      PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}"
    )
  endif()

  if(USER_APPS_VSOMEIP_INCLUDE_DIR)
    target_include_directories(
      ${target_name}
      SYSTEM
      PRIVATE
      "${USER_APPS_VSOMEIP_INCLUDE_DIR}"
    )
  endif()
endfunction()

function(configure_user_apps_ecu_feature_target target_name)
  configure_user_apps_feature_transport_target(${target_name})
  target_link_libraries(
    ${target_name}
    PRIVATE
    AdaptiveAutosarAP::ara_exec
    AdaptiveAutosarAP::ara_phm
    AdaptiveAutosarAP::ara_per
  )
endfunction()

# Detect optional transport backends from exported ara_com compile definitions.
set(USER_APPS_HAS_VSOMEIP OFF)
set(USER_APPS_HAS_ICEORYX OFF)
set(USER_APPS_HAS_DDS OFF)
get_target_property(_ara_com_defs AdaptiveAutosarAP::ara_com INTERFACE_COMPILE_DEFINITIONS)
if(_ara_com_defs)
  foreach(_def IN LISTS _ara_com_defs)
    if(_def STREQUAL "ARA_COM_USE_VSOMEIP=1" OR _def STREQUAL "ARA_COM_USE_VSOMEIP")
      set(USER_APPS_HAS_VSOMEIP ON)
    endif()
    if(_def STREQUAL "ARA_COM_USE_ICEORYX=1" OR _def STREQUAL "ARA_COM_USE_ICEORYX")
      set(USER_APPS_HAS_ICEORYX ON)
    endif()
    if(_def STREQUAL "ARA_COM_USE_CYCLONEDDS=1" OR _def STREQUAL "ARA_COM_USE_CYCLONEDDS")
      set(USER_APPS_HAS_DDS ON)
    endif()
  endforeach()
endif()

# vSomeIP headers are currently required by ara::com internal event-binding headers.
unset(USER_APPS_VSOMEIP_INCLUDE_DIR CACHE)
unset(USER_APPS_VSOMEIP_INCLUDE_DIR)
find_path(
  USER_APPS_VSOMEIP_INCLUDE_DIR
  NAMES vsomeip/vsomeip.hpp
  HINTS
    /opt/vsomeip/include
)

# Import CycloneDDS targets explicitly so we can query their include directories.
# This ensures we pick up the correct (cross-compilation) CycloneDDS paths even
# when host-installed headers exist at /opt/cyclonedds.
if(NOT TARGET CycloneDDS::ddsc)
  find_package(CycloneDDS QUIET)
endif()
if(NOT TARGET CycloneDDS-CXX::ddscxx)
  find_package(CycloneDDS-CXX QUIET NAMES CycloneDDS-CXX)
  if(NOT TARGET CycloneDDS-CXX::ddscxx)
    find_package(CycloneDDS-CXX QUIET)
  endif()
endif()

# Prefer include paths from CycloneDDS cmake imported targets when available.
# This avoids picking up host-installed CycloneDDS headers when cross-compiling.
set(USER_APPS_CYCLONEDDS_INCLUDE_DIRS "")
if(TARGET CycloneDDS-CXX::ddscxx)
  get_target_property(_ddscxx_incs CycloneDDS-CXX::ddscxx INTERFACE_INCLUDE_DIRECTORIES)
  if(_ddscxx_incs)
    list(APPEND USER_APPS_CYCLONEDDS_INCLUDE_DIRS ${_ddscxx_incs})
  endif()
endif()
if(TARGET CycloneDDS::ddsc)
  get_target_property(_ddsc_incs CycloneDDS::ddsc INTERFACE_INCLUDE_DIRECTORIES)
  if(_ddsc_incs)
    list(APPEND USER_APPS_CYCLONEDDS_INCLUDE_DIRS ${_ddsc_incs})
  endif()
endif()
# Fallback to find_path when targets are not yet imported (native builds).
if(NOT USER_APPS_CYCLONEDDS_INCLUDE_DIRS)
  unset(USER_APPS_CYCLONEDDS_DDSCXX_INCLUDE_DIR CACHE)
  unset(USER_APPS_CYCLONEDDS_DDSCXX_INCLUDE_DIR)
  find_path(
    USER_APPS_CYCLONEDDS_DDSCXX_INCLUDE_DIR
    NAMES dds/dds.hpp
    HINTS
      /opt/cyclonedds/include/ddscxx
  )

  unset(USER_APPS_CYCLONEDDS_INCLUDE_DIR CACHE)
  unset(USER_APPS_CYCLONEDDS_INCLUDE_DIR)
  find_path(
    USER_APPS_CYCLONEDDS_INCLUDE_DIR
    NAMES dds/ddsi/ddsi_serdata.h
    HINTS
      /opt/cyclonedds/include
  )

  if(USER_APPS_CYCLONEDDS_DDSCXX_INCLUDE_DIR)
    list(APPEND USER_APPS_CYCLONEDDS_INCLUDE_DIRS "${USER_APPS_CYCLONEDDS_DDSCXX_INCLUDE_DIR}")
  endif()
  if(USER_APPS_CYCLONEDDS_INCLUDE_DIR)
    list(APPEND USER_APPS_CYCLONEDDS_INCLUDE_DIRS "${USER_APPS_CYCLONEDDS_INCLUDE_DIR}")
  endif()
endif()

# Optional DDS IDL-to-C++ generation for communication DDS apps.
set(USER_APPS_DDS_TYPE_AVAILABLE OFF)
if(USER_APPS_HAS_DDS)
  set(USER_APPS_IDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/idl")
  set(USER_APPS_DDS_IDL "${USER_APPS_IDL_DIR}/UserAppsStatus.idl")

  find_program(USER_APPS_IDLC NAMES idlc HINTS /opt/cyclonedds/bin)
  if(USER_APPS_IDLC AND EXISTS "${USER_APPS_DDS_IDL}")
    set(USER_APPS_DDS_HPP "${CMAKE_CURRENT_BINARY_DIR}/UserAppsStatus.hpp")
    set(USER_APPS_DDS_CPP "${CMAKE_CURRENT_BINARY_DIR}/UserAppsStatus.cpp")

    add_custom_command(
      OUTPUT
        "${USER_APPS_DDS_HPP}"
        "${USER_APPS_DDS_CPP}"
      COMMAND ${CMAKE_COMMAND} -E env
        LD_LIBRARY_PATH=/opt/cyclonedds/lib:/opt/iceoryx/lib:$ENV{LD_LIBRARY_PATH}
        "${USER_APPS_IDLC}"
        -l cxx
        -fcase-sensitive
        -I "${USER_APPS_IDL_DIR}"
        "${USER_APPS_DDS_IDL}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      DEPENDS "${USER_APPS_DDS_IDL}"
      COMMENT "Generate DDS app type from UserAppsStatus.idl"
      VERBATIM
    )

    set_source_files_properties(
      "${USER_APPS_DDS_HPP}"
      "${USER_APPS_DDS_CPP}"
      PROPERTIES GENERATED TRUE
    )

    add_library(
      user_apps_dds_types
      STATIC
      "${USER_APPS_DDS_HPP}"
      "${USER_APPS_DDS_CPP}"
    )

    target_include_directories(
      user_apps_dds_types
      PUBLIC
      "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_link_libraries(
      user_apps_dds_types
      PUBLIC
      CycloneDDS::ddsc
      CycloneDDS-CXX::ddscxx
    )

    set(USER_APPS_DDS_TYPE_AVAILABLE ON)
  else()
    message(STATUS "user_apps: DDS communication apps will build in guidance mode (idlc not found or IDL missing).")
  endif()
endif()

# Optional DDS IDL-to-C++ generation for feature apps.
set(USER_APPS_FEATURE_SUPPORT_AVAILABLE OFF)
if(EXISTS "${USER_APPS_FEATURE_PUBSUB_DIR}" AND
   EXISTS "${USER_APPS_FEATURE_VEHICLE_STATUS_DIR}" AND
   EXISTS "${USER_APPS_FEATURE_CAN_DIR}" AND
   EXISTS "${USER_APPS_FEATURE_ECU_DIR}")
  set(USER_APPS_FEATURE_SUPPORT_AVAILABLE ON)
endif()

set(USER_APPS_TEMPLATE_DDS_TYPE_AVAILABLE OFF)
if(USER_APPS_FEATURE_SUPPORT_AVAILABLE AND USER_APPS_HAS_DDS)
  set(USER_APPS_TEMPLATE_DDS_IDL "${USER_APPS_FEATURE_PUBSUB_DIR}/idl/VehicleStatusFrame.idl")
  find_program(USER_APPS_TEMPLATE_IDLC NAMES idlc HINTS /opt/cyclonedds/bin)

  if(USER_APPS_TEMPLATE_IDLC AND EXISTS "${USER_APPS_TEMPLATE_DDS_IDL}")
    set(USER_APPS_TEMPLATE_DDS_HPP "${CMAKE_CURRENT_BINARY_DIR}/VehicleStatusFrame.hpp")
    set(USER_APPS_TEMPLATE_DDS_CPP "${CMAKE_CURRENT_BINARY_DIR}/VehicleStatusFrame.cpp")

    add_custom_command(
      OUTPUT
        "${USER_APPS_TEMPLATE_DDS_HPP}"
        "${USER_APPS_TEMPLATE_DDS_CPP}"
      COMMAND ${CMAKE_COMMAND} -E env
        LD_LIBRARY_PATH=/opt/cyclonedds/lib:/opt/iceoryx/lib:$ENV{LD_LIBRARY_PATH}
        "${USER_APPS_TEMPLATE_IDLC}"
        -l cxx
        -fcase-sensitive
        -I "${USER_APPS_FEATURE_PUBSUB_DIR}/idl"
        "${USER_APPS_TEMPLATE_DDS_IDL}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      DEPENDS "${USER_APPS_TEMPLATE_DDS_IDL}"
      COMMENT "Generate DDS feature type from VehicleStatusFrame.idl"
      VERBATIM
    )

    set_source_files_properties(
      "${USER_APPS_TEMPLATE_DDS_HPP}"
      "${USER_APPS_TEMPLATE_DDS_CPP}"
      PROPERTIES GENERATED TRUE
    )

    add_library(
      user_apps_template_dds_types
      STATIC
      "${USER_APPS_TEMPLATE_DDS_HPP}"
      "${USER_APPS_TEMPLATE_DDS_CPP}"
    )

    if(USER_APPS_CYCLONEDDS_INCLUDE_DIRS)
      target_include_directories(
        user_apps_template_dds_types
        BEFORE
        PUBLIC
        ${USER_APPS_CYCLONEDDS_INCLUDE_DIRS}
      )
    endif()
    target_include_directories(
      user_apps_template_dds_types
      PUBLIC
      "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_link_libraries(
      user_apps_template_dds_types
      PUBLIC
      CycloneDDS::ddsc
      CycloneDDS-CXX::ddscxx
    )

    set(USER_APPS_TEMPLATE_DDS_TYPE_AVAILABLE ON)
  else()
    message(STATUS "user_apps: feature apps with DDS backend need idlc and VehicleStatusFrame.idl.")
  endif()
endif()

add_subdirectory(src/apps)
