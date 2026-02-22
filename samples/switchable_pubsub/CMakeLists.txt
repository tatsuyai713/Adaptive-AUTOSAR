cmake_minimum_required(VERSION 3.14)
project(AdaptiveAutosarSwitchablePubSub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

set(AUTOSAR_AP_PREFIX "/opt/autosar_ap" CACHE PATH "Installed Adaptive AUTOSAR AP prefix")
set(CYCLONEDDS_PREFIX "/opt/cyclonedds" CACHE PATH "CycloneDDS install prefix")

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(AUTOSAR_CODEGEN_DIR "${REPO_ROOT}/tools/ara_com_codegen")
set(SAMPLE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SAMPLE_IDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/idl")

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
find_package(Python3 REQUIRED COMPONENTS Interpreter)

list(APPEND CMAKE_PREFIX_PATH "${CYCLONEDDS_PREFIX}/lib/cmake")
if(NOT TARGET CycloneDDS::ddsc)
  find_package(CycloneDDS REQUIRED)
endif()
if(NOT TARGET CycloneDDS-CXX::ddscxx)
  find_package(CycloneDDS-CXX REQUIRED)
endif()

find_program(IDLC_EXECUTABLE NAMES idlc HINTS "${CYCLONEDDS_PREFIX}/bin" "/opt/cyclonedds/bin")
if(NOT IDLC_EXECUTABLE)
  message(FATAL_ERROR "idlc was not found. Install CycloneDDS with idlc at ${CYCLONEDDS_PREFIX}/bin")
endif()

find_program(
  AUTOSAR_COMM_MANIFEST_GENERATOR
  NAMES autosar-generate-comm-manifest
  HINTS
    "${AUTOSAR_CODEGEN_DIR}"
    "${AUTOSAR_AP_PREFIX}/bin")
if(NOT AUTOSAR_COMM_MANIFEST_GENERATOR)
  message(FATAL_ERROR "autosar-generate-comm-manifest not found in PATH, ${AUTOSAR_CODEGEN_DIR}, or ${AUTOSAR_AP_PREFIX}/bin")
endif()

find_program(
  AUTOSAR_PROXY_SKELETON_GENERATOR
  NAMES autosar-generate-proxy-skeleton
  HINTS
    "${AUTOSAR_CODEGEN_DIR}"
    "${AUTOSAR_AP_PREFIX}/bin")
if(NOT AUTOSAR_PROXY_SKELETON_GENERATOR)
  message(FATAL_ERROR "autosar-generate-proxy-skeleton not found in PATH, ${AUTOSAR_CODEGEN_DIR}, or ${AUTOSAR_AP_PREFIX}/bin")
endif()

set(ARXML_GENERATOR_SCRIPT "${REPO_ROOT}/tools/arxml_generator/generate_arxml.py")
set(BINDING_GENERATOR_SCRIPT "${REPO_ROOT}/tools/arxml_generator/generate_ara_com_binding.py")
if(NOT EXISTS "${ARXML_GENERATOR_SCRIPT}")
  message(FATAL_ERROR "ARXML generator script not found: ${ARXML_GENERATOR_SCRIPT}")
endif()
if(NOT EXISTS "${BINDING_GENERATOR_SCRIPT}")
  message(FATAL_ERROR "binding generator script not found: ${BINDING_GENERATOR_SCRIPT}")
endif()

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_MAPPING_DDS "${GENERATED_DIR}/switchable_topic_mapping_dds.yaml")
set(GENERATED_MANIFEST_YAML_DDS "${GENERATED_DIR}/switchable_manifest_dds.yaml")
set(GENERATED_ARXML_DDS "${GENERATED_DIR}/switchable_manifest_dds.arxml")

set(GENERATED_MAPPING_SOMEIP "${GENERATED_DIR}/switchable_topic_mapping_vsomeip.yaml")
set(GENERATED_MANIFEST_YAML_SOMEIP "${GENERATED_DIR}/switchable_manifest_vsomeip.yaml")
set(GENERATED_ARXML_SOMEIP "${GENERATED_DIR}/switchable_manifest_vsomeip.arxml")

set(GENERATED_PROXY_HEADER "${GENERATED_DIR}/switchable_proxy_skeleton.hpp")
set(GENERATED_BINDING_HEADER "${GENERATED_DIR}/switchable_binding.hpp")

add_custom_command(
  OUTPUT
    "${GENERATED_MAPPING_DDS}"
    "${GENERATED_MANIFEST_YAML_DDS}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
  COMMAND ${AUTOSAR_COMM_MANIFEST_GENERATOR}
    --apps-root "${SAMPLE_SOURCE_DIR}"
    --output-mapping "${GENERATED_MAPPING_DDS}"
    --output-manifest "${GENERATED_MANIFEST_YAML_DDS}"
    --event-binding dds
    --print-summary
  DEPENDS
    "${SAMPLE_SOURCE_DIR}/pubsub_usage_scan.cpp"
  COMMENT "Generate DDS mapping/manifest for switchable Pub/Sub"
  VERBATIM
)

add_custom_command(
  OUTPUT
    "${GENERATED_MAPPING_SOMEIP}"
    "${GENERATED_MANIFEST_YAML_SOMEIP}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}"
  COMMAND ${AUTOSAR_COMM_MANIFEST_GENERATOR}
    --apps-root "${SAMPLE_SOURCE_DIR}"
    --output-mapping "${GENERATED_MAPPING_SOMEIP}"
    --output-manifest "${GENERATED_MANIFEST_YAML_SOMEIP}"
    --event-binding vsomeip
    --print-summary
  DEPENDS
    "${SAMPLE_SOURCE_DIR}/pubsub_usage_scan.cpp"
  COMMENT "Generate SOME/IP mapping/manifest for switchable Pub/Sub"
  VERBATIM
)

add_custom_command(
  OUTPUT "${GENERATED_ARXML_DDS}"
  COMMAND ${Python3_EXECUTABLE} "${ARXML_GENERATOR_SCRIPT}"
    --input "${GENERATED_MANIFEST_YAML_DDS}"
    --output "${GENERATED_ARXML_DDS}"
    --overwrite
    --print-summary
  DEPENDS
    "${GENERATED_MANIFEST_YAML_DDS}"
    "${ARXML_GENERATOR_SCRIPT}"
  COMMENT "Generate DDS-profile ARXML for switchable Pub/Sub"
  VERBATIM
)

add_custom_command(
  OUTPUT "${GENERATED_ARXML_SOMEIP}"
  COMMAND ${Python3_EXECUTABLE} "${ARXML_GENERATOR_SCRIPT}"
    --input "${GENERATED_MANIFEST_YAML_SOMEIP}"
    --output "${GENERATED_ARXML_SOMEIP}"
    --overwrite
    --print-summary
  DEPENDS
    "${GENERATED_MANIFEST_YAML_SOMEIP}"
    "${ARXML_GENERATOR_SCRIPT}"
  COMMENT "Generate SOME/IP-profile ARXML for switchable Pub/Sub"
  VERBATIM
)

add_custom_command(
  OUTPUT "${GENERATED_PROXY_HEADER}"
  COMMAND ${AUTOSAR_PROXY_SKELETON_GENERATOR}
    --mapping "${GENERATED_MAPPING_DDS}"
    --output "${GENERATED_PROXY_HEADER}"
    --namespace sample::switchable_generated
    --print-summary
  DEPENDS
    "${GENERATED_MAPPING_DDS}"
  COMMENT "Generate standard proxy/skeleton header from DDS mapping"
  VERBATIM
)

add_custom_command(
  OUTPUT "${GENERATED_BINDING_HEADER}"
  COMMAND ${Python3_EXECUTABLE} "${BINDING_GENERATOR_SCRIPT}"
    --input "${GENERATED_ARXML_SOMEIP}"
    --output "${GENERATED_BINDING_HEADER}"
    --namespace sample::switchable_generated
  DEPENDS
    "${GENERATED_ARXML_SOMEIP}"
    "${BINDING_GENERATOR_SCRIPT}"
  COMMENT "Generate binding constants header from SOME/IP ARXML"
  VERBATIM
)

add_custom_target(
  switchable_pubsub_codegen
  DEPENDS
    "${GENERATED_MAPPING_DDS}"
    "${GENERATED_MANIFEST_YAML_DDS}"
    "${GENERATED_ARXML_DDS}"
    "${GENERATED_MAPPING_SOMEIP}"
    "${GENERATED_MANIFEST_YAML_SOMEIP}"
    "${GENERATED_ARXML_SOMEIP}"
    "${GENERATED_PROXY_HEADER}"
    "${GENERATED_BINDING_HEADER}"
)

set(SWITCHABLE_IDL "${SAMPLE_IDL_DIR}/VehicleStatusFrame.idl")
set(SWITCHABLE_DDS_HPP "${CMAKE_CURRENT_BINARY_DIR}/VehicleStatusFrame.hpp")
set(SWITCHABLE_DDS_CPP "${CMAKE_CURRENT_BINARY_DIR}/VehicleStatusFrame.cpp")

add_custom_command(
  OUTPUT
    "${SWITCHABLE_DDS_HPP}"
    "${SWITCHABLE_DDS_CPP}"
  COMMAND ${CMAKE_COMMAND} -E env
    LD_LIBRARY_PATH=${CYCLONEDDS_PREFIX}/lib:/opt/iceoryx/lib:$ENV{LD_LIBRARY_PATH}
    "${IDLC_EXECUTABLE}"
    -l cxx
    -fcase-sensitive
    -I "${SAMPLE_IDL_DIR}"
    "${SWITCHABLE_IDL}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS "${SWITCHABLE_IDL}"
  COMMENT "Generate DDS C++ type from VehicleStatusFrame.idl"
  VERBATIM
)

set_source_files_properties(
  "${SWITCHABLE_DDS_HPP}"
  "${SWITCHABLE_DDS_CPP}"
  PROPERTIES GENERATED TRUE
)

add_library(
  switchable_pubsub_dds_types
  STATIC
  "${SWITCHABLE_DDS_HPP}"
  "${SWITCHABLE_DDS_CPP}"
)

target_include_directories(
  switchable_pubsub_dds_types
  BEFORE
  PUBLIC
  "${CYCLONEDDS_PREFIX}/include/ddscxx"
  "${CYCLONEDDS_PREFIX}/include"
  "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
  switchable_pubsub_dds_types
  PUBLIC
  CycloneDDS::ddsc
  CycloneDDS-CXX::ddscxx
)

function(add_switchable_pubsub_target target_name source_file)
  add_executable(
    ${target_name}
    "${source_file}"
  )

  add_dependencies(
    ${target_name}
    switchable_pubsub_codegen
  )

  target_link_libraries(
    ${target_name}
    PRIVATE
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_com
    AdaptiveAutosarAP::ara_log
    switchable_pubsub_dds_types
  )

  target_include_directories(
    ${target_name}
    BEFORE
    PRIVATE
    "${CYCLONEDDS_PREFIX}/include/ddscxx"
    "${CYCLONEDDS_PREFIX}/include"
  )

  if(EXISTS "/opt/autosar-ap-libs/include")
    target_include_directories(
      ${target_name}
      SYSTEM
      PRIVATE
      "/opt/autosar-ap-libs/include"
    )
  endif()

  if(EXISTS "/opt/vsomeip/include")
    target_include_directories(
      ${target_name}
      SYSTEM
      PRIVATE
      "/opt/vsomeip/include"
    )
  endif()

  target_include_directories(
    ${target_name}
    PRIVATE
    "${GENERATED_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
  )
endfunction()

add_switchable_pubsub_target(
  autosar_switchable_pubsub_pub
  "${SAMPLE_SOURCE_DIR}/switchable_pub.cpp"
)

add_switchable_pubsub_target(
  autosar_switchable_pubsub_sub
  "${SAMPLE_SOURCE_DIR}/switchable_sub.cpp"
)
