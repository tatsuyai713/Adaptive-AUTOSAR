########################################################################
#
# Requirements and dependencies:

cmake_minimum_required(VERSION 3.14)
project(Adaptive_Platform)

# This AUTOSAR Adaptive Platform requires C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable 'rdyamic' switch to export the executable's symbols for debugging
set(CMAKE_ENABLE_EXPORTS 1)

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Fetching GoogleTest 1.12.1 for unit testing
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/58d77fa8070e8cec2dc1ed015d66b454c8d78850.zip
)

# Fetching Async-BSD-Socket-Lib for network abstraction
FetchContent_Declare(
  Async-BSD-Socket-Lib
  URL https://github.com/langroodi/Async-BSD-Socket-Lib/archive/8c84da8f4b5e7552b41a54c92acb0b4565d07024.zip
)

# Fetching  OBD-II-Emulator for OBD2 CAN bus communication
FetchContent_Declare(
  OBD-II-Emulator
  URL https://github.com/langroodi/OBD-II-Emulator/archive/2c33042a6397d7030f3bafb57b6b88b913d95cb2.zip
)

# Fetching  Doip-Lib for DoIP communication
FetchContent_Declare(
  Doip-Lib
  URL https://github.com/langroodi/Doip-Lib/archive/5acfd483eb3dc90429a98dbe4b70de8b1db68af6.zip
)

FetchContent_MakeAvailable(
  googletest
  Async-BSD-Socket-Lib
  OBD-II-Emulator
  Doip-Lib
)

# Normalize Async-BSD-Socket-Lib include usage so exported install targets
# do not keep build-tree absolute include paths.
FetchContent_GetProperties(
  Async-BSD-Socket-Lib
  SOURCE_DIR ASYNC_BSD_SOCKET_LIB_SOURCE_DIR)
if(TARGET async_bsd_socket_lib)
  set_property(
    TARGET async_bsd_socket_lib
    PROPERTY INTERFACE_INCLUDE_DIRECTORIES
      "$<BUILD_INTERFACE:${ASYNC_BSD_SOCKET_LIB_SOURCE_DIR}/include>;$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
endif()

########################################################################
#
# Options:

option(build_tests "Build all the tests." ON)
option(ARA_COM_USE_VSOMEIP "Use vsomeip as SOME/IP transport backend." ON)
option(ARA_COM_USE_ICEORYX "Use iceoryx for ara::com zero-copy backend." ON)
option(ARA_COM_USE_CYCLONEDDS "Use Cyclone DDS as ara::com DDS backend." OFF)
option(ARA_COM_ENABLE_ARXML_CODEGEN "Generate ara::com sample binding headers from ARXML." ON)
option(AUTOSAR_AP_BUILD_PLATFORM_APP "Build the bundled platform executable (adaptive_autosar)." ON)
option(AUTOSAR_AP_BUILD_SAMPLES "Build bundled sample applications in this repository." ON)
option(AUTOSAR_AP_INSTALL_TOOLS "Install Python/shell helper tools into install prefix." ON)
option(AUTOSAR_AP_INSTALL_USER_APP_TEMPLATE "Install standalone user-app template project." ON)

find_package(OpenSSL REQUIRED)

if(ARA_COM_USE_VSOMEIP)
  set(VSOMEIP_PREFIX "/opt/vsomeip" CACHE PATH "vsomeip install prefix")
  link_directories(${VSOMEIP_PREFIX}/lib)
endif()

if(ARA_COM_USE_ICEORYX)
  set(ICEORYX_PREFIX "/opt/iceoryx" CACHE PATH "iceoryx install prefix")
  list(APPEND CMAKE_PREFIX_PATH "${ICEORYX_PREFIX}/lib/cmake")
  find_package(iceoryx_posh REQUIRED)
endif()

if(ARA_COM_USE_CYCLONEDDS)
  set(CYCLONEDDS_PREFIX "/opt/cyclonedds" CACHE PATH "Cyclone DDS install prefix")
  list(APPEND CMAKE_PREFIX_PATH "${CYCLONEDDS_PREFIX}/lib/cmake")

  if(NOT ARA_COM_USE_ICEORYX)
    set(ICEORYX_PREFIX "/opt/iceoryx" CACHE PATH "iceoryx install prefix")
    list(APPEND CMAKE_PREFIX_PATH "${ICEORYX_PREFIX}/lib/cmake")
  endif()

  find_package(iceoryx_binding_c REQUIRED)
  find_package(CycloneDDS REQUIRED)
  find_package(CycloneDDS-CXX REQUIRED)
endif()

set(ARA_COM_GENERATED_INCLUDE_DIR "")
if(ARA_COM_ENABLE_ARXML_CODEGEN)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  set(
    ARA_COM_CODEGEN_INPUT_ARXML
    "${CMAKE_SOURCE_DIR}/configuration/pubsub_sample_manifest.arxml"
    CACHE FILEPATH
    "ARXML file used for ara::com sample code generation")
  set(
    ARA_COM_CODEGEN_PROVIDED_SERVICE_SHORT_NAME
    "VehicleStatusProviderInstance"
    CACHE STRING
    "SHORT-NAME of PROVIDED-SOMEIP-SERVICE-INSTANCE used by codegen")
  set(
    ARA_COM_CODEGEN_PROVIDED_EVENT_GROUP_SHORT_NAME
    "VehicleStatusEventGroup"
    CACHE STRING
    "SHORT-NAME of SOMEIP-PROVIDED-EVENT-GROUP used by codegen")

  set(ARA_COM_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
  set(
    ARA_COM_CODEGEN_OUTPUT_HEADER
    "${ARA_COM_GENERATED_INCLUDE_DIR}/ara_com_generated/vehicle_status_manifest_binding.h")
  set(
    ARA_COM_CODEGEN_SCRIPT
    "${CMAKE_SOURCE_DIR}/tools/arxml_generator/generate_ara_com_binding.py")

  add_custom_command(
    OUTPUT ${ARA_COM_CODEGEN_OUTPUT_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${ARA_COM_CODEGEN_SCRIPT}
      --input ${ARA_COM_CODEGEN_INPUT_ARXML}
      --output ${ARA_COM_CODEGEN_OUTPUT_HEADER}
      --namespace sample::vehicle_status::generated
      --provided-service-short-name ${ARA_COM_CODEGEN_PROVIDED_SERVICE_SHORT_NAME}
      --provided-event-group-short-name ${ARA_COM_CODEGEN_PROVIDED_EVENT_GROUP_SHORT_NAME}
    DEPENDS
      ${ARA_COM_CODEGEN_SCRIPT}
      ${ARA_COM_CODEGEN_INPUT_ARXML}
    COMMENT "Generate ara::com sample binding header from ARXML"
    VERBATIM
  )

  add_custom_target(
    ara_com_arxml_codegen
    DEPENDS ${ARA_COM_CODEGEN_OUTPUT_HEADER}
  )
endif()

########################################################################
#
# Source Directories:

set(source_dir
  "${CMAKE_SOURCE_DIR}/src")

set(source_arxml_dir
  "${CMAKE_SOURCE_DIR}/src/arxml")

set(source_ara_core_dir
  "${CMAKE_SOURCE_DIR}/src/ara/core")

set(source_ara_log_dir
  "${CMAKE_SOURCE_DIR}/src/ara/log")

set(source_ara_log_sink_dir
  "${CMAKE_SOURCE_DIR}/src/ara/log/sink")

set(source_ara_sm_dir
  "${CMAKE_SOURCE_DIR}/src/ara/sm")

set(source_ara_com_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com")

set(source_ara_com_zerocopy_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/zerocopy")

set(source_ara_com_cg_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/cg")

set(source_ara_com_e2e_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/e2e")

set(source_ara_com_helper_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/helper")

set(source_ara_com_option_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/option")  

set(source_ara_com_entry_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/entry")

set(source_ara_com_someip_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/someip")

set(source_ara_com_dds_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/dds")

set(source_ara_com_internal_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/internal")

set(source_ara_com_someip_rpc_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/someip/rpc")

set(source_ara_com_someip_sd_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/someip/sd")

set(source_ara_com_someip_sd_fsm_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/someip/sd/fsm")

set(source_ara_com_someip_pubsub_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/someip/pubsub")

set(source_ara_com_someip_pubsub_fsm_dir
  "${CMAKE_SOURCE_DIR}/src/ara/com/someip/pubsub/fsm")

set(source_ara_exec_dir
  "${CMAKE_SOURCE_DIR}/src/ara/exec")

set(source_ara_exec_helper_dir
  "${CMAKE_SOURCE_DIR}/src/ara/exec/helper")

set(source_ara_diag_dir
  "${CMAKE_SOURCE_DIR}/src/ara/diag")

set(source_ara_diag_routing_dir
  "${CMAKE_SOURCE_DIR}/src/ara/diag/routing")

set(source_ara_diag_debouncing_dir
  "${CMAKE_SOURCE_DIR}/src/ara/diag/debouncing")

set(source_ara_phm_dir
  "${CMAKE_SOURCE_DIR}/src/ara/phm")

set(source_ara_per_dir
  "${CMAKE_SOURCE_DIR}/src/ara/per")

set(source_ara_crypto_dir
  "${CMAKE_SOURCE_DIR}/src/ara/crypto")

set(source_ara_iam_dir
  "${CMAKE_SOURCE_DIR}/src/ara/iam")

set(source_ara_tsync_dir
  "${CMAKE_SOURCE_DIR}/src/ara/tsync")

set(source_ara_ucm_dir
  "${CMAKE_SOURCE_DIR}/src/ara/ucm")

set(source_ara_phm_supervisors_dir
  "${CMAKE_SOURCE_DIR}/src/ara/phm/supervisors")

set(source_application_dir
  "${CMAKE_SOURCE_DIR}/src/application")

set(source_application_doip_dir
  "${CMAKE_SOURCE_DIR}/src/application/doip")

set(source_application_helper_dir
  "${CMAKE_SOURCE_DIR}/src/application/helper")

set(source_application_platform_dir
  "${CMAKE_SOURCE_DIR}/src/application/platform")

set(source_sample_ara_com_pubsub_dir
  "${CMAKE_SOURCE_DIR}/user_apps/src/features/communication/pubsub")

set(source_sample_ara_com_standard_dir
  "${CMAKE_SOURCE_DIR}/user_apps/src/apps/communication/someip")

set(source_sample_ara_com_socketcan_dir
  "${CMAKE_SOURCE_DIR}/user_apps/src/features/communication/can")

set(source_sample_ecu_showcase_dir
  "${CMAKE_SOURCE_DIR}/user_apps/src/features/ecu")

# Test Directories:

set(test_arxml_dir
  "${CMAKE_SOURCE_DIR}/test/arxml")

set(test_ara_core_dir
  "${CMAKE_SOURCE_DIR}/test/ara/core")

set(test_ara_log_dir
  "${CMAKE_SOURCE_DIR}/test/ara/log")

set(test_ara_sm_dir
  "${CMAKE_SOURCE_DIR}/test/ara/sm")

set(test_ara_com_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com")

set(test_ara_com_zerocopy_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/zerocopy")

set(test_ara_com_e2e_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/e2e")

set(test_ara_com_entry_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/entry")

set(test_ara_com_helper_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/helper")

set(test_ara_com_option_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/option")

set(test_ara_com_someip_pubsub_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/someip/pubsub")

set(test_ara_com_someip_pubsub_fsm_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/someip/pubsub/fsm")

set(test_ara_com_someip_rpc_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/someip/rpc")

set(test_ara_com_someip_sd_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/someip/sd")

set(test_ara_com_someip_sd_fsm_dir
  "${CMAKE_SOURCE_DIR}/test/ara/com/someip/sd/fsm")

set(test_ara_exec_dir
  "${CMAKE_SOURCE_DIR}/test/ara/exec")

set(test_ara_exec_helper_dir
  "${CMAKE_SOURCE_DIR}/test/ara/exec/helper")

set(test_ara_diag_dir
  "${CMAKE_SOURCE_DIR}/test/ara/diag")

set(test_ara_diag_routing_dir
  "${CMAKE_SOURCE_DIR}/test/ara/diag/routing")

set(test_ara_diag_debouncing_dir
  "${CMAKE_SOURCE_DIR}/test/ara/diag/debouncing")

set(test_ara_phm_dir
  "${CMAKE_SOURCE_DIR}/test/ara/phm")

set(test_ara_per_dir
  "${CMAKE_SOURCE_DIR}/test/ara/per")

set(test_ara_crypto_dir
  "${CMAKE_SOURCE_DIR}/test/ara/crypto")

set(test_ara_iam_dir
  "${CMAKE_SOURCE_DIR}/test/ara/iam")

set(test_ara_tsync_dir
  "${CMAKE_SOURCE_DIR}/test/ara/tsync")

set(test_ara_ucm_dir
  "${CMAKE_SOURCE_DIR}/test/ara/ucm")

set(test_ara_phm_supervisors_dir
  "${CMAKE_SOURCE_DIR}/test/ara/phm/supervisors")

########################################################################

add_library(
  arxml
  ${source_arxml_dir}/pugiconfig.hpp
  ${source_arxml_dir}/pugixml.hpp
  ${source_arxml_dir}/pugixml.cpp
  ${source_arxml_dir}/arxml_node.h
  ${source_arxml_dir}/arxml_node.cpp
  ${source_arxml_dir}/arxml_node_iterator.h
  ${source_arxml_dir}/arxml_node_range.h
  ${source_arxml_dir}/arxml_reader.h
  ${source_arxml_dir}/arxml_reader.cpp
)

add_library(
  ara_core
  ${source_ara_core_dir}/optional.h
  ${source_ara_core_dir}/result.h
  ${source_ara_core_dir}/future.h
  ${source_ara_core_dir}/promise.h
  ${source_ara_core_dir}/error_domain.h
  ${source_ara_core_dir}/error_code.h
  ${source_ara_core_dir}/error_code.cpp
  ${source_ara_core_dir}/instance_specifier.h
  ${source_ara_core_dir}/instance_specifier.cpp
  ${source_ara_core_dir}/initialization.h
  ${source_ara_core_dir}/initialization.cpp
)

add_library(
  ara_log
  ${source_ara_log_dir}/argument.h
  ${source_ara_log_dir}/log_stream.h
  ${source_ara_log_dir}/log_stream.cpp
  ${source_ara_log_dir}/logger.h
  ${source_ara_log_dir}/logger.cpp
  ${source_ara_log_dir}/logging_framework.h
  ${source_ara_log_dir}/logging_framework.cpp
  ${source_ara_log_sink_dir}/console_log_sink.h
  ${source_ara_log_sink_dir}/console_log_sink.cpp
  ${source_ara_log_sink_dir}/file_log_sink.h
  ${source_ara_log_sink_dir}/file_log_sink.cpp
  ${source_ara_log_sink_dir}/log_sink.h
  ${source_ara_log_sink_dir}/log_sink.cpp
  ${source_ara_log_sink_dir}/network_log_sink.h
  ${source_ara_log_sink_dir}/network_log_sink.cpp
)

add_library(
  ara_com
  ${source_ara_com_dir}/types.h
  ${source_ara_com_dir}/sample_ptr.h
  ${source_ara_com_dir}/serialization.h
  ${source_ara_com_dir}/event.h
  ${source_ara_com_dir}/method.h
  ${source_ara_com_dir}/field.h
  ${source_ara_com_dir}/service_proxy_base.h
  ${source_ara_com_dir}/service_proxy_base.cpp
  ${source_ara_com_dir}/service_skeleton_base.h
  ${source_ara_com_dir}/service_skeleton_base.cpp
  ${source_ara_com_dir}/service_handle_type.h
  ${source_ara_com_dir}/com_error_domain.h
  ${source_ara_com_dir}/com_error_domain.cpp
  ${source_ara_com_dds_dir}/dds_pubsub.h
  ${source_ara_com_dds_dir}/cyclonedds_pubsub.h
  ${source_ara_com_zerocopy_dir}/zero_copy.h
  ${source_ara_com_zerocopy_dir}/zero_copy_binding.h
  ${source_ara_com_zerocopy_dir}/iceoryx_zerocopy.h
  ${source_ara_com_zerocopy_dir}/iceoryx_zerocopy.cpp
  ${source_ara_com_cg_dir}/communication_group_client.h
  ${source_ara_com_cg_dir}/communication_group_server.h
  ${source_ara_com_e2e_dir}/profile11.h
  ${source_ara_com_e2e_dir}/profile11.cpp
  ${source_ara_com_e2e_dir}/e2e_event.h
  ${source_ara_com_helper_dir}/payload_helper.h
  ${source_ara_com_helper_dir}/payload_helper.cpp
  ${source_ara_com_helper_dir}/ipv4_address.h
  ${source_ara_com_helper_dir}/ipv4_address.cpp
  ${source_ara_com_helper_dir}/abstract_state_machine.h
  ${source_ara_com_helper_dir}/machine_state.h
  ${source_ara_com_helper_dir}/finite_state_machine.h
  ${source_ara_com_helper_dir}/ttl_timer.h
  ${source_ara_com_helper_dir}/ttl_timer.cpp
  ${source_ara_com_helper_dir}/network_layer.h
  ${source_ara_com_helper_dir}/concurrent_queue.h
  ${source_ara_com_entry_dir}/entry.h
  ${source_ara_com_entry_dir}/entry.cpp
  ${source_ara_com_entry_dir}/eventgroup_entry.h
  ${source_ara_com_entry_dir}/eventgroup_entry.cpp
  ${source_ara_com_entry_dir}/service_entry.h
  ${source_ara_com_entry_dir}/service_entry.cpp
  ${source_ara_com_entry_dir}/entry_deserializer.h
  ${source_ara_com_entry_dir}/entry_deserializer.cpp
  ${source_ara_com_option_dir}/option.h
  ${source_ara_com_option_dir}/option.cpp
  ${source_ara_com_option_dir}/ipv4_endpoint_option.h
  ${source_ara_com_option_dir}/ipv4_endpoint_option.cpp
  ${source_ara_com_option_dir}/loadbalancing_option.h
  ${source_ara_com_option_dir}/loadbalancing_option.cpp
  ${source_ara_com_option_dir}/option_deserializer.h
  ${source_ara_com_option_dir}/option_deserializer.cpp
  ${source_ara_com_someip_dir}/someip_message.h
  ${source_ara_com_someip_dir}/someip_message.cpp
  ${source_ara_com_someip_dir}/vsomeip_application.h
  ${source_ara_com_someip_dir}/vsomeip_application.cpp
  ${source_ara_com_someip_rpc_dir}/someip_rpc_message.h
  ${source_ara_com_someip_rpc_dir}/someip_rpc_message.cpp
  ${source_ara_com_someip_rpc_dir}/rpc_server.h
  ${source_ara_com_someip_rpc_dir}/rpc_server.cpp
  ${source_ara_com_someip_rpc_dir}/socket_rpc_server.h
  ${source_ara_com_someip_rpc_dir}/socket_rpc_server.cpp
  ${source_ara_com_someip_rpc_dir}/rpc_client.h
  ${source_ara_com_someip_rpc_dir}/rpc_client.cpp
  ${source_ara_com_someip_rpc_dir}/socket_rpc_client.h
  ${source_ara_com_someip_rpc_dir}/socket_rpc_client.cpp
  ${source_ara_com_someip_pubsub_dir}/someip_pubsub_server.h
  ${source_ara_com_someip_pubsub_dir}/someip_pubsub_server.cpp
  ${source_ara_com_someip_pubsub_dir}/someip_pubsub_client.h
  ${source_ara_com_someip_pubsub_dir}/someip_pubsub_client.cpp
  ${source_ara_com_someip_pubsub_fsm_dir}/service_down_state.h
  ${source_ara_com_someip_pubsub_fsm_dir}/service_down_state.cpp
  ${source_ara_com_someip_pubsub_fsm_dir}/notsubscribed_state.h
  ${source_ara_com_someip_pubsub_fsm_dir}/notsubscribed_state.cpp
  ${source_ara_com_someip_pubsub_fsm_dir}/subscribed_state.h
  ${source_ara_com_someip_pubsub_fsm_dir}/subscribed_state.cpp
  ${source_ara_com_someip_sd_dir}/someip_sd_agent.h
  ${source_ara_com_someip_sd_dir}/someip_sd_message.h
  ${source_ara_com_someip_sd_dir}/someip_sd_message.cpp
  ${source_ara_com_someip_sd_dir}/someip_sd_server.h
  ${source_ara_com_someip_sd_dir}/someip_sd_server.cpp
  ${source_ara_com_someip_sd_dir}/someip_sd_client.h
  ${source_ara_com_someip_sd_dir}/someip_sd_client.cpp
  ${source_ara_com_someip_sd_dir}/sd_network_layer.h
  ${source_ara_com_someip_sd_dir}/sd_network_layer.cpp
  ${source_ara_com_someip_sd_fsm_dir}/timer_set_state.h
  ${source_ara_com_someip_sd_fsm_dir}/client_service_state.h
  ${source_ara_com_someip_sd_fsm_dir}/notready_state.h
  ${source_ara_com_someip_sd_fsm_dir}/notready_state.cpp
  ${source_ara_com_someip_sd_fsm_dir}/initial_wait_state.h
  ${source_ara_com_someip_sd_fsm_dir}/client_initial_wait_state.h
  ${source_ara_com_someip_sd_fsm_dir}/client_initial_wait_state.cpp
  ${source_ara_com_someip_sd_fsm_dir}/repetition_state.h
  ${source_ara_com_someip_sd_fsm_dir}/client_repetition_state.h
  ${source_ara_com_someip_sd_fsm_dir}/client_repetition_state.cpp
  ${source_ara_com_someip_sd_fsm_dir}/main_state.h
  ${source_ara_com_someip_sd_fsm_dir}/main_state.cpp
  ${source_ara_com_someip_sd_fsm_dir}/stopped_state.h
  ${source_ara_com_someip_sd_fsm_dir}/stopped_state.cpp
  ${source_ara_com_someip_sd_fsm_dir}/service_notseen_state.h
  ${source_ara_com_someip_sd_fsm_dir}/service_notseen_state.cpp
  ${source_ara_com_someip_sd_fsm_dir}/service_ready_state.h
  ${source_ara_com_someip_sd_fsm_dir}/service_ready_state.cpp
  ${source_ara_com_someip_sd_fsm_dir}/service_seen_state.h
  ${source_ara_com_someip_sd_fsm_dir}/service_seen_state.cpp
  ${source_ara_com_internal_dir}/event_binding.h
  ${source_ara_com_internal_dir}/method_binding.h
  ${source_ara_com_internal_dir}/vsomeip_event_binding.h
  ${source_ara_com_internal_dir}/vsomeip_event_binding.cpp
  ${source_ara_com_internal_dir}/vsomeip_method_binding.h
  ${source_ara_com_internal_dir}/vsomeip_method_binding.cpp
  ${source_ara_com_internal_dir}/binding_factory.h
)

add_library(
  ara_sm
  ${source_ara_sm_dir}/states.h
  ${source_ara_sm_dir}/trigger.h
  ${source_ara_sm_dir}/notifier.h
  ${source_ara_sm_dir}/trigger_in.h
  ${source_ara_sm_dir}/trigger_out.h
  ${source_ara_sm_dir}/trigger_inout.h
)

set_target_properties(
  ara_sm
  PROPERTIES LINKER_LANGUAGE CXX
)

add_library(
  ara_exec
  ${source_ara_exec_dir}/worker_thread.h
  ${source_ara_exec_dir}/worker_thread.cpp
  ${source_ara_exec_dir}/worker_runnable.h
  ${source_ara_exec_dir}/deterministic_client.h
  ${source_ara_exec_dir}/deterministic_client.cpp
  ${source_ara_exec_dir}/execution_client.h
  ${source_ara_exec_dir}/execution_client.cpp
  ${source_ara_exec_dir}/execution_server.h
  ${source_ara_exec_dir}/execution_server.cpp
  ${source_ara_exec_dir}/function_group.h
  ${source_ara_exec_dir}/function_group.cpp
  ${source_ara_exec_dir}/function_group_state.h
  ${source_ara_exec_dir}/function_group_state.cpp
  ${source_ara_exec_dir}/exec_error_domain.h
  ${source_ara_exec_dir}/exec_error_domain.cpp
  ${source_ara_exec_dir}/exec_exception.h
  ${source_ara_exec_dir}/exec_exception.cpp
  ${source_ara_exec_dir}/execution_error_event.h
  ${source_ara_exec_dir}/signal_handler.h
  ${source_ara_exec_dir}/signal_handler.cpp
  ${source_ara_exec_dir}/state_client.h
  ${source_ara_exec_dir}/state_client.cpp
  ${source_ara_exec_dir}/state_server.h
  ${source_ara_exec_dir}/state_server.cpp
  ${source_ara_exec_helper_dir}/atomic_optional.h
  ${source_ara_exec_helper_dir}/modelled_process.h
  ${source_ara_exec_helper_dir}/modelled_process.cpp
  ${source_ara_exec_helper_dir}/process_watchdog.h
  ${source_ara_exec_helper_dir}/process_watchdog.cpp
)

add_library(
  ara_diag
  ${source_ara_diag_dir}/meta_info.h
  ${source_ara_diag_dir}/meta_info.cpp
  ${source_ara_diag_dir}/cancellation_handler.h
  ${source_ara_diag_dir}/cancellation_handler.cpp
  ${source_ara_diag_dir}/event.h
  ${source_ara_diag_dir}/event.cpp
  ${source_ara_diag_dir}/dtc_information.h
  ${source_ara_diag_dir}/dtc_information.cpp
  ${source_ara_diag_dir}/condition.h
  ${source_ara_diag_dir}/condition.cpp
  ${source_ara_diag_dir}/operation_cycle.h
  ${source_ara_diag_dir}/operation_cycle.cpp
  ${source_ara_diag_dir}/conversation.h
  ${source_ara_diag_dir}/conversation.cpp
  ${source_ara_diag_dir}/diag_error_domain.h
  ${source_ara_diag_dir}/diag_error_domain.cpp
  ${source_ara_diag_dir}/generic_uds_service.h
  ${source_ara_diag_dir}/security_access.h
  ${source_ara_diag_dir}/security_access.cpp
  ${source_ara_diag_dir}/download.h
  ${source_ara_diag_dir}/download.cpp
  ${source_ara_diag_dir}/upload.h
  ${source_ara_diag_dir}/upload.cpp
  ${source_ara_diag_dir}/ecu_reset_request.h
  ${source_ara_diag_dir}/ecu_reset_request.cpp
  ${source_ara_diag_dir}/generic_routine.h
  ${source_ara_diag_dir}/generic_routine.cpp
  ${source_ara_diag_dir}/monitor.h
  ${source_ara_diag_dir}/monitor.cpp
  ${source_ara_diag_routing_dir}/routable_uds_service.h
  ${source_ara_diag_routing_dir}/routable_uds_service.cpp
  ${source_ara_diag_routing_dir}/uds_service_router.h
  ${source_ara_diag_routing_dir}/uds_service_router.cpp
  ${source_ara_diag_routing_dir}/delay_timer.h
  ${source_ara_diag_routing_dir}/delay_timer.cpp
  ${source_ara_diag_routing_dir}/transfer_data.h
  ${source_ara_diag_routing_dir}/transfer_data.cpp
  ${source_ara_diag_routing_dir}/request_transfer_exit.h
  ${source_ara_diag_routing_dir}/request_transfer_exit.cpp
  ${source_ara_diag_routing_dir}/nrc_exception.h
  ${source_ara_diag_routing_dir}/nrc_exception.cpp
  ${source_ara_diag_routing_dir}/request_transfer.h
  ${source_ara_diag_routing_dir}/request_transfer.cpp
  ${source_ara_diag_debouncing_dir}/debouncer.h
  ${source_ara_diag_debouncing_dir}/debouncer.cpp
  ${source_ara_diag_debouncing_dir}/counter_based_debouncer.h
  ${source_ara_diag_debouncing_dir}/counter_based_debouncer.cpp
  ${source_ara_diag_debouncing_dir}/timer_based_debouncer.h
  ${source_ara_diag_debouncing_dir}/timer_based_debouncer.cpp
)

add_library(
  ara_phm
  ${source_ara_phm_dir}/checkpoint_communicator.h
  ${source_ara_phm_dir}/checkpoint_communicator.cpp
  ${source_ara_phm_dir}/supervised_entity.h
  ${source_ara_phm_dir}/supervised_entity.cpp
  ${source_ara_phm_dir}/recovery_action.h
  ${source_ara_phm_dir}/recovery_action.cpp
  ${source_ara_phm_dir}/restart_recovery_action.h
  ${source_ara_phm_dir}/restart_recovery_action.cpp
  ${source_ara_phm_dir}/recovery_action_dispatcher.h
  ${source_ara_phm_dir}/recovery_action_dispatcher.cpp
  ${source_ara_phm_dir}/health_channel.h
  ${source_ara_phm_dir}/health_channel.cpp
  ${source_ara_phm_supervisors_dir}/elementary_supervision.h
  ${source_ara_phm_supervisors_dir}/elementary_supervision.cpp
  ${source_ara_phm_supervisors_dir}/alive_supervision.h
  ${source_ara_phm_supervisors_dir}/alive_supervision.cpp
  ${source_ara_phm_supervisors_dir}/deadline_supervision.h
  ${source_ara_phm_supervisors_dir}/deadline_supervision.cpp
  ${source_ara_phm_supervisors_dir}/global_supervision.h
  ${source_ara_phm_supervisors_dir}/global_supervision.cpp
)

add_library(
  ara_per
  ${source_ara_per_dir}/per_error_domain.h
  ${source_ara_per_dir}/per_error_domain.cpp
  ${source_ara_per_dir}/shared_handle.h
  ${source_ara_per_dir}/read_accessor.h
  ${source_ara_per_dir}/read_accessor.cpp
  ${source_ara_per_dir}/read_write_accessor.h
  ${source_ara_per_dir}/read_write_accessor.cpp
  ${source_ara_per_dir}/key_value_storage.h
  ${source_ara_per_dir}/key_value_storage.cpp
  ${source_ara_per_dir}/file_storage.h
  ${source_ara_per_dir}/file_storage.cpp
  ${source_ara_per_dir}/persistency.h
  ${source_ara_per_dir}/persistency.cpp
)

add_library(
  ara_crypto
  ${source_ara_crypto_dir}/crypto_error_domain.h
  ${source_ara_crypto_dir}/crypto_error_domain.cpp
  ${source_ara_crypto_dir}/crypto_provider.h
  ${source_ara_crypto_dir}/crypto_provider.cpp
)

add_library(
  ara_iam
  ${source_ara_iam_dir}/iam_error_domain.h
  ${source_ara_iam_dir}/iam_error_domain.cpp
  ${source_ara_iam_dir}/access_control.h
  ${source_ara_iam_dir}/access_control.cpp
)

add_library(
  ara_tsync
  ${source_ara_tsync_dir}/tsync_error_domain.h
  ${source_ara_tsync_dir}/tsync_error_domain.cpp
  ${source_ara_tsync_dir}/time_sync_client.h
  ${source_ara_tsync_dir}/time_sync_client.cpp
)

add_library(
  ara_ucm
  ${source_ara_ucm_dir}/ucm_error_domain.h
  ${source_ara_ucm_dir}/ucm_error_domain.cpp
  ${source_ara_ucm_dir}/update_manager.h
  ${source_ara_ucm_dir}/update_manager.cpp
)

target_link_libraries(
  ara_per
  ara_core
)

target_link_libraries(
  ara_crypto
  ara_core
  OpenSSL::Crypto
)

target_link_libraries(
  ara_iam
  ara_core
)

target_link_libraries(
  ara_tsync
  ara_core
)

target_link_libraries(
  ara_ucm
  ara_core
  ara_crypto
)

target_link_libraries(
  ara_com
  ara_core
  async_bsd_socket_lib
)

if(ARA_COM_USE_VSOMEIP)
  target_include_directories(
    ara_com
    PRIVATE
    ${VSOMEIP_PREFIX}/include
  )

  target_link_directories(
    ara_com
    PUBLIC
    $<BUILD_INTERFACE:${VSOMEIP_PREFIX}/lib>
    $<INSTALL_INTERFACE:${VSOMEIP_PREFIX}/lib>
  )

  target_compile_definitions(
    ara_com
    PUBLIC ARA_COM_USE_VSOMEIP=1
  )

  target_link_libraries(
    ara_com
    vsomeip3
    vsomeip3-sd
    vsomeip3-e2e
  )
endif()

if(ARA_COM_USE_ICEORYX)
  target_compile_definitions(
    ara_com
    PUBLIC ARA_COM_USE_ICEORYX=1
  )

  target_link_libraries(
    ara_com
    iceoryx_posh::iceoryx_posh
  )
endif()

if(ARA_COM_USE_CYCLONEDDS)
  target_compile_definitions(
    ara_com
    PUBLIC ARA_COM_USE_CYCLONEDDS=1
  )

  target_link_libraries(
    ara_com
    CycloneDDS::ddsc
    CycloneDDS-CXX::ddscxx
  )
endif()

target_link_libraries(
  ara_exec
  ara_core
  async_bsd_socket_lib
)

target_link_libraries(
  ara_log
  ara_core
  ara_com
)

target_link_libraries(
  ara_diag
  ara_core
)

target_link_libraries(
  ara_phm
  ara_exec
)

# Public include roots for installed consumers.
set(autosar_ap_public_include_roots
  ${CMAKE_SOURCE_DIR}/src
)

foreach(_autosar_target arxml ara_core ara_log ara_com ara_sm ara_exec ara_diag ara_phm ara_per ara_crypto ara_iam ara_tsync ara_ucm)
  if(TARGET ${_autosar_target})
    target_include_directories(
      ${_autosar_target}
      PUBLIC
      $<BUILD_INTERFACE:${autosar_ap_public_include_roots}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
  endif()
endforeach()

if(AUTOSAR_AP_BUILD_PLATFORM_APP)
  add_executable(
    adaptive_autosar
    ${source_dir}/main.cpp
    ${source_application_dir}/extended_vehicle.h
    ${source_application_dir}/extended_vehicle.cpp
    ${source_application_helper_dir}/argument_configuration.h
    ${source_application_helper_dir}/argument_configuration.cpp
    ${source_application_helper_dir}/network_configuration.h
    ${source_application_helper_dir}/network_configuration.cpp
    ${source_application_helper_dir}/rpc_configuration.h
    ${source_application_helper_dir}/rpc_configuration.cpp
    ${source_application_helper_dir}/read_data_by_identifier.h
    ${source_application_helper_dir}/read_data_by_identifier.cpp
    ${source_application_helper_dir}/memory_cache.h
    ${source_application_helper_dir}/log_recovery_action.h
    ${source_application_helper_dir}/log_recovery_action.cpp
    ${source_application_helper_dir}/fifo_checkpoint_communicator.h
    ${source_application_helper_dir}/fifo_checkpoint_communicator.cpp
    ${source_application_doip_dir}/vehicle_id_request_handler.h
    ${source_application_doip_dir}/vehicle_id_request_handler.cpp
    ${source_application_doip_dir}/diag_message_handler.h
    ${source_application_doip_dir}/diag_message_handler.cpp
    ${source_application_doip_dir}/doip_server.h
    ${source_application_doip_dir}/doip_server.cpp
    ${source_application_doip_dir}/doip_client.h
    ${source_application_doip_dir}/doip_client.cpp
    ${source_application_doip_dir}/obd_to_doip_converter.h
    ${source_application_doip_dir}/obd_to_doip_converter.cpp
    ${source_application_platform_dir}/state_management.h
    ${source_application_platform_dir}/state_management.cpp
    ${source_application_platform_dir}/execution_management.h
    ${source_application_platform_dir}/execution_management.cpp
    ${source_application_platform_dir}/diagnostic_manager.h
    ${source_application_platform_dir}/diagnostic_manager.cpp
    ${source_application_platform_dir}/platform_health_management.h
    ${source_application_platform_dir}/platform_health_management.cpp
  )

  target_link_libraries(
    adaptive_autosar
    obd_ii_emulator
    doip_lib
    arxml
    ara_exec
    ara_log
    ara_diag
    ara_phm
  )

  if(ARA_COM_USE_VSOMEIP)
    target_include_directories(
      adaptive_autosar
      PRIVATE
      ${VSOMEIP_PREFIX}/include
    )
  endif()
endif()

if(ARA_COM_USE_VSOMEIP)
  add_executable(
    autosar_vsomeip_routing_manager
    ${source_dir}/main_vsomeip_routing_manager.cpp
  )

  target_include_directories(
    autosar_vsomeip_routing_manager
    PRIVATE
    ${VSOMEIP_PREFIX}/include
  )

  target_link_directories(
    autosar_vsomeip_routing_manager
    PUBLIC
    $<BUILD_INTERFACE:${VSOMEIP_PREFIX}/lib>
    $<INSTALL_INTERFACE:${VSOMEIP_PREFIX}/lib>
  )

  target_link_libraries(
    autosar_vsomeip_routing_manager
    vsomeip3
    vsomeip3-sd
    vsomeip3-e2e
  )
endif()

add_executable(
  autosar_time_sync_daemon
  ${source_dir}/main_time_sync_daemon.cpp
)

target_link_libraries(
  autosar_time_sync_daemon
  ara_tsync
)

add_executable(
  autosar_persistency_guard
  ${source_dir}/main_persistency_guard.cpp
)

target_link_libraries(
  autosar_persistency_guard
  ara_per
)

add_executable(
  autosar_iam_policy_loader
  ${source_dir}/main_iam_policy_loader.cpp
)

target_link_libraries(
  autosar_iam_policy_loader
  ara_iam
)

add_executable(
  autosar_can_interface_manager
  ${source_dir}/main_can_interface_manager.cpp
)

add_executable(
  autosar_watchdog_supervisor
  ${source_dir}/main_watchdog_supervisor.cpp
)

add_executable(
  autosar_user_app_monitor
  ${source_dir}/main_user_app_monitor.cpp
)

target_link_libraries(
  autosar_user_app_monitor
  ara_core
  ara_phm
)

set(has_pubsub_sample_entrypoints OFF)
if(EXISTS "${source_sample_ara_com_pubsub_dir}/pubsub_provider.cpp" AND
   EXISTS "${source_sample_ara_com_pubsub_dir}/pubsub_consumer.cpp")
  set(has_pubsub_sample_entrypoints ON)
elseif(AUTOSAR_AP_BUILD_SAMPLES)
  message(STATUS "AUTOSAR_AP_BUILD_SAMPLES=ON but pubsub sample entrypoints are not present in this tree. Skipping legacy pubsub sample targets.")
endif()

if(AUTOSAR_AP_BUILD_SAMPLES AND has_pubsub_sample_entrypoints AND (ARA_COM_USE_VSOMEIP OR ARA_COM_USE_CYCLONEDDS))
  set(pubsub_sample_extra_link_libraries "")
  set(pubsub_sample_common_sources
    ${source_sample_ara_com_pubsub_dir}/pubsub_autosar_portable_api.h
    ${source_sample_ara_com_pubsub_dir}/pubsub_autosar_portable_api.cpp
    ${source_sample_ara_com_pubsub_dir}/pubsub_common.h
    ${source_sample_ara_com_pubsub_dir}/pubsub_common.cpp
  )

  if(ARA_COM_USE_CYCLONEDDS)
    set(pubsub_dds_idl
      ${source_sample_ara_com_pubsub_dir}/idl/VehicleStatusFrame.idl)
    set(pubsub_dds_generated_hpp
      ${CMAKE_CURRENT_BINARY_DIR}/VehicleStatusFrame.hpp)
    set(pubsub_dds_generated_cpp
      ${CMAKE_CURRENT_BINARY_DIR}/VehicleStatusFrame.cpp)

    add_custom_command(
      OUTPUT
        ${pubsub_dds_generated_hpp}
        ${pubsub_dds_generated_cpp}
      COMMAND ${CMAKE_COMMAND} -E env
        LD_LIBRARY_PATH=${CYCLONEDDS_PREFIX}/lib:${ICEORYX_PREFIX}/lib:$ENV{LD_LIBRARY_PATH}
        ${CYCLONEDDS_PREFIX}/bin/idlc
        -l cxx
        -fcase-sensitive
        -I ${source_sample_ara_com_pubsub_dir}/idl
        ${pubsub_dds_idl}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${pubsub_dds_idl}
      COMMENT "Generate CycloneDDS C++ sample type from IDL"
      VERBATIM
    )

    set_source_files_properties(
      ${pubsub_dds_generated_hpp}
      ${pubsub_dds_generated_cpp}
      PROPERTIES GENERATED TRUE
    )

    add_library(
      ara_com_pubsub_dds_types
      STATIC
      ${pubsub_dds_generated_hpp}
      ${pubsub_dds_generated_cpp}
    )

    target_include_directories(
      ara_com_pubsub_dds_types
      BEFORE
      PUBLIC
      ${CYCLONEDDS_PREFIX}/include/ddscxx
      ${CYCLONEDDS_PREFIX}/include
      ${CMAKE_CURRENT_BINARY_DIR}
    )

    target_link_libraries(
      ara_com_pubsub_dds_types
      PUBLIC
      CycloneDDS::ddsc
      CycloneDDS-CXX::ddscxx
    )

    list(APPEND pubsub_sample_extra_link_libraries ara_com_pubsub_dds_types)
  endif()

  function(configure_pubsub_sample_target target_name)
    target_link_libraries(
      ${target_name}
      ara_core
      ara_com
      ara_log
      ${pubsub_sample_extra_link_libraries}
    )

    if(ARA_COM_USE_CYCLONEDDS)
      target_include_directories(
        ${target_name}
        BEFORE PRIVATE
        ${CYCLONEDDS_PREFIX}/include/ddscxx
        ${CYCLONEDDS_PREFIX}/include
      )
    endif()

    if(ARA_COM_USE_VSOMEIP)
      target_include_directories(
        ${target_name}
        SYSTEM PRIVATE
        ${VSOMEIP_PREFIX}/include
      )
    endif()

    if(ARA_COM_ENABLE_ARXML_CODEGEN)
      add_dependencies(${target_name} ara_com_arxml_codegen)
      target_include_directories(
        ${target_name}
        PRIVATE
        ${ARA_COM_GENERATED_INCLUDE_DIR}
      )
    endif()

  endfunction()

  function(add_pubsub_provider_sample target_name default_event_backend default_zerocopy_backend)
    add_executable(
      ${target_name}
      ${pubsub_sample_common_sources}
      ${source_sample_ara_com_pubsub_dir}/pubsub_provider.cpp
    )

    target_compile_definitions(
      ${target_name}
      PRIVATE
      PUBSUB_DEFAULT_EVENT_BACKEND=\"${default_event_backend}\"
      PUBSUB_DEFAULT_ZERO_COPY_BACKEND=\"${default_zerocopy_backend}\"
    )

    configure_pubsub_sample_target(${target_name})
  endfunction()

  function(add_pubsub_consumer_sample target_name default_event_backend default_zerocopy_backend)
    add_executable(
      ${target_name}
      ${pubsub_sample_common_sources}
      ${source_sample_ara_com_pubsub_dir}/pubsub_consumer.cpp
    )

    target_compile_definitions(
      ${target_name}
      PRIVATE
      PUBSUB_DEFAULT_EVENT_BACKEND=\"${default_event_backend}\"
      PUBSUB_DEFAULT_ZERO_COPY_BACKEND=\"${default_zerocopy_backend}\"
    )

    configure_pubsub_sample_target(${target_name})
  endfunction()

  if(ARA_COM_USE_VSOMEIP)
    add_pubsub_provider_sample(
      ara_com_pubsub_vsomeip_pub_sample
      someip
      none
    )
    add_pubsub_consumer_sample(
      ara_com_pubsub_vsomeip_sub_sample
      someip
      none
    )
  endif()

  if(ARA_COM_USE_VSOMEIP AND ARA_COM_USE_ICEORYX)
    add_pubsub_provider_sample(
      ara_com_pubsub_iceoryx_pub_sample
      someip
      zerocopy
    )
    add_pubsub_consumer_sample(
      ara_com_pubsub_iceoryx_sub_sample
      someip
      zerocopy
    )
  endif()

  if(ARA_COM_USE_CYCLONEDDS)
    add_pubsub_provider_sample(
      ara_com_pubsub_cyclonedds_pub_sample
      dds
      none
    )
    add_pubsub_consumer_sample(
      ara_com_pubsub_cyclonedds_sub_sample
      dds
      none
    )
  endif()

  set(default_provider_event_backend "")
  set(default_provider_zerocopy_backend "")
  if(ARA_COM_USE_VSOMEIP AND ARA_COM_USE_ICEORYX)
    set(default_provider_event_backend "someip")
    set(default_provider_zerocopy_backend "zerocopy")
  elseif(ARA_COM_USE_VSOMEIP)
    set(default_provider_event_backend "someip")
    set(default_provider_zerocopy_backend "none")
  elseif(ARA_COM_USE_CYCLONEDDS)
    set(default_provider_event_backend "dds")
    set(default_provider_zerocopy_backend "none")
  endif()

  if(NOT "${default_provider_event_backend}" STREQUAL "")
    add_pubsub_provider_sample(
      ara_com_pubsub_provider_sample
      ${default_provider_event_backend}
      ${default_provider_zerocopy_backend}
    )
    add_pubsub_consumer_sample(
      ara_com_pubsub_consumer_sample
      ${default_provider_event_backend}
      ${default_provider_zerocopy_backend}
    )
  endif()

  if(ARA_COM_USE_VSOMEIP AND ARA_COM_USE_CYCLONEDDS)
    add_executable(
      ara_com_pubsub_bridge_someip_to_dds_sample
      ${pubsub_sample_common_sources}
      ${source_sample_ara_com_pubsub_dir}/pubsub_bridge_someip_to_dds.cpp
    )
    configure_pubsub_sample_target(ara_com_pubsub_bridge_someip_to_dds_sample)
  endif()
endif()

# ── Standard AUTOSAR AP ara::com sample applications ──
function(enable_arxml_codegen_for_target target_name)
  if(ARA_COM_ENABLE_ARXML_CODEGEN)
    add_dependencies(${target_name} ara_com_arxml_codegen)
    target_include_directories(
      ${target_name}
      PRIVATE
      ${ARA_COM_GENERATED_INCLUDE_DIR}
    )
  endif()
endfunction()

set(has_standard_sample_entrypoints OFF)
if(EXISTS "${source_sample_ara_com_standard_dir}/standard_provider.cpp" AND
   EXISTS "${source_sample_ara_com_standard_dir}/standard_consumer.cpp" AND
   EXISTS "${source_sample_ara_com_socketcan_dir}/socketcan_gateway_provider.cpp" AND
   EXISTS "${source_sample_ara_com_socketcan_dir}/socketcan_gateway_consumer.cpp")
  set(has_standard_sample_entrypoints ON)
elseif(AUTOSAR_AP_BUILD_SAMPLES)
  message(STATUS "AUTOSAR_AP_BUILD_SAMPLES=ON but standard/socketcan sample entrypoints are not present in this tree. Skipping those sample targets.")
endif()

if(AUTOSAR_AP_BUILD_SAMPLES AND has_standard_sample_entrypoints AND ARA_COM_USE_VSOMEIP)
  add_executable(
    ara_com_standard_provider_sample
    ${source_sample_ara_com_standard_dir}/standard_provider.cpp
  )

  target_link_libraries(
    ara_com_standard_provider_sample
    ara_core
    ara_com
  )

  target_include_directories(
    ara_com_standard_provider_sample
    PRIVATE
    ${VSOMEIP_PREFIX}/include
  )
  enable_arxml_codegen_for_target(ara_com_standard_provider_sample)

  add_executable(
    ara_com_standard_consumer_sample
    ${source_sample_ara_com_standard_dir}/standard_consumer.cpp
  )

  target_link_libraries(
    ara_com_standard_consumer_sample
    ara_core
    ara_com
  )

  target_include_directories(
    ara_com_standard_consumer_sample
    PRIVATE
    ${VSOMEIP_PREFIX}/include
  )
  enable_arxml_codegen_for_target(ara_com_standard_consumer_sample)

  add_executable(
    ara_com_socketcan_provider_sample
    ${source_sample_ara_com_socketcan_dir}/socketcan_gateway_provider.cpp
    ${source_sample_ara_com_socketcan_dir}/socketcan_receiver.cpp
    ${source_sample_ara_com_socketcan_dir}/mock_can_receiver.cpp
    ${source_sample_ara_com_socketcan_dir}/vehicle_status_can_decoder.cpp
  )

  target_link_libraries(
    ara_com_socketcan_provider_sample
    ara_core
    ara_com
  )

  target_include_directories(
    ara_com_socketcan_provider_sample
    PRIVATE
    ${VSOMEIP_PREFIX}/include
  )
  enable_arxml_codegen_for_target(ara_com_socketcan_provider_sample)

  add_executable(
    ara_com_socketcan_consumer_sample
    ${source_sample_ara_com_socketcan_dir}/socketcan_gateway_consumer.cpp
  )

  target_link_libraries(
    ara_com_socketcan_consumer_sample
    ara_core
    ara_com
  )

  target_include_directories(
    ara_com_socketcan_consumer_sample
    PRIVATE
    ${VSOMEIP_PREFIX}/include
  )
  enable_arxml_codegen_for_target(ara_com_socketcan_consumer_sample)
endif()

set(has_ecu_showcase_entrypoints OFF)
if(EXISTS "${source_sample_ecu_showcase_dir}/can_to_dds_gateway.cpp" AND
   EXISTS "${source_sample_ecu_showcase_dir}/someip_to_dds_bridge.cpp" AND
   EXISTS "${source_sample_ecu_showcase_dir}/can_someip_fusion_to_dds.cpp" AND
   EXISTS "${source_sample_ecu_showcase_dir}/ecu_reference_gateway.cpp")
  set(has_ecu_showcase_entrypoints ON)
elseif(AUTOSAR_AP_BUILD_SAMPLES)
  message(STATUS "AUTOSAR_AP_BUILD_SAMPLES=ON but ECU showcase entrypoints are not present in this tree. Skipping ECU sample targets.")
endif()

if(AUTOSAR_AP_BUILD_SAMPLES AND has_ecu_showcase_entrypoints AND ARA_COM_USE_CYCLONEDDS)
  set(ecu_showcase_shared_sources
    ${source_sample_ecu_showcase_dir}/ecu_sample_common.h
    ${source_sample_ecu_showcase_dir}/ecu_sample_common.cpp
    ${source_sample_ara_com_pubsub_dir}/pubsub_autosar_portable_api.h
    ${source_sample_ara_com_pubsub_dir}/pubsub_autosar_portable_api.cpp
    ${source_sample_ara_com_pubsub_dir}/pubsub_common.h
    ${source_sample_ara_com_pubsub_dir}/pubsub_common.cpp
  )

  set(ecu_showcase_can_sources
    ${source_sample_ecu_showcase_dir}/ecu_sample_can_common.cpp
    ${source_sample_ara_com_socketcan_dir}/can_frame_receiver.h
    ${source_sample_ara_com_socketcan_dir}/socketcan_receiver.h
    ${source_sample_ara_com_socketcan_dir}/socketcan_receiver.cpp
    ${source_sample_ara_com_socketcan_dir}/mock_can_receiver.h
    ${source_sample_ara_com_socketcan_dir}/mock_can_receiver.cpp
  )

  function(configure_ecu_showcase_target target_name)
    configure_pubsub_sample_target(${target_name})
    target_link_libraries(
      ${target_name}
      ara_exec
      ara_phm
      ara_per
    )
  endfunction()

  add_executable(
    ara_ecu_can_to_dds_gateway_sample
    ${ecu_showcase_shared_sources}
    ${ecu_showcase_can_sources}
    ${source_sample_ecu_showcase_dir}/can_to_dds_gateway.cpp
    ${source_sample_ara_com_socketcan_dir}/vehicle_status_can_decoder.h
    ${source_sample_ara_com_socketcan_dir}/vehicle_status_can_decoder.cpp
  )
  configure_ecu_showcase_target(ara_ecu_can_to_dds_gateway_sample)

  if(ARA_COM_USE_VSOMEIP)
    add_executable(
      ara_ecu_someip_to_dds_bridge_sample
      ${ecu_showcase_shared_sources}
      ${source_sample_ecu_showcase_dir}/someip_to_dds_bridge.cpp
    )
    configure_ecu_showcase_target(ara_ecu_someip_to_dds_bridge_sample)

    add_executable(
      ara_ecu_can_someip_fusion_to_dds_sample
      ${ecu_showcase_shared_sources}
      ${ecu_showcase_can_sources}
      ${source_sample_ecu_showcase_dir}/can_someip_fusion_to_dds.cpp
      ${source_sample_ara_com_socketcan_dir}/vehicle_status_can_decoder.h
      ${source_sample_ara_com_socketcan_dir}/vehicle_status_can_decoder.cpp
    )
    configure_ecu_showcase_target(ara_ecu_can_someip_fusion_to_dds_sample)

    # ECU reference sample: integrated CAN + SOME/IP ingress to DDS egress
    add_executable(
      ara_ecu_reference_gateway_sample
      ${ecu_showcase_shared_sources}
      ${ecu_showcase_can_sources}
      ${source_sample_ecu_showcase_dir}/ecu_reference_gateway.cpp
      ${source_sample_ara_com_socketcan_dir}/vehicle_status_can_decoder.h
      ${source_sample_ara_com_socketcan_dir}/vehicle_status_can_decoder.cpp
    )
    configure_ecu_showcase_target(ara_ecu_reference_gateway_sample)
  endif()
endif()

if(build_tests)
  enable_testing()

  add_executable(
    arxml_unit_test
    ${test_arxml_dir}/arxml_node_test.cpp
    ${test_arxml_dir}/arxml_reader_test.cpp
  )

  add_executable(
    ara_unit_test
    ${test_ara_log_dir}/argument_test.cpp
    ${test_ara_log_dir}/log_stream_test.cpp
    ${test_ara_log_dir}/logger_test.cpp
    ${test_ara_log_dir}/logging_framework_test.cpp
    ${test_ara_sm_dir}/trigger_in_test.cpp
    ${test_ara_sm_dir}/trigger_out_test.cpp
    ${test_ara_sm_dir}/trigger_inout_test.cpp
    ${test_ara_sm_dir}/power_mode_test.cpp
    ${test_ara_com_e2e_dir}/profile11_test.cpp
    ${test_ara_com_entry_dir}/eventgroup_entry_test.cpp
    ${test_ara_com_entry_dir}/service_entry_test.cpp
    ${test_ara_com_helper_dir}/ipv4_address_test.cpp
    ${test_ara_com_helper_dir}/mockup_network_layer.h
    ${test_ara_com_helper_dir}/ttl_timer_test.cpp
    ${test_ara_com_helper_dir}/concurrent_queue_test.cpp
    ${test_ara_com_option_dir}/ipv4_endpoint_option_test.cpp
    ${test_ara_com_option_dir}/loadbalancing_option_test.cpp
    ${test_ara_com_someip_pubsub_dir}/someip_pubsub_test.cpp
    ${test_ara_com_someip_pubsub_fsm_dir}/pubsub_state_test.cpp
    ${test_ara_com_someip_rpc_dir}/someip_rpc_message_test.cpp
    ${test_ara_com_someip_rpc_dir}/rpc_server_test.cpp
    ${test_ara_com_someip_rpc_dir}/rpc_client_test.cpp
    ${test_ara_com_someip_sd_dir}/someip_sd_message_test.cpp
    ${test_ara_com_someip_sd_dir}/network_abstraction_test.cpp
    ${test_ara_com_someip_sd_dir}/someip_sd_test.cpp
    ${test_ara_com_someip_sd_fsm_dir}/machine_state_test.cpp
    ${test_ara_exec_dir}/worker_thread_test.cpp
    ${test_ara_exec_dir}/worker_runnable_test.cpp
    ${test_ara_exec_dir}/deterministic_client_test.cpp
    ${test_ara_exec_dir}/execution_client_test.cpp
    ${test_ara_exec_dir}/execution_server_test.cpp
    ${test_ara_exec_dir}/function_group_test.cpp
    ${test_ara_exec_dir}/function_group_state_test.cpp
    ${test_ara_exec_dir}/exec_exception_test.cpp
    ${test_ara_exec_dir}/state_client_test.cpp
    ${test_ara_exec_dir}/state_server_test.cpp
    ${test_ara_exec_helper_dir}/atomic_optional_test.cpp
    ${test_ara_exec_helper_dir}/mock_rpc_client.h
    ${test_ara_exec_helper_dir}/mock_rpc_server.h
    ${test_ara_exec_helper_dir}/modelled_process_test.cpp
    ${test_ara_core_dir}/optional_test.cpp
    ${test_ara_core_dir}/result_test.cpp
    ${test_ara_core_dir}/result_void_test.cpp
    ${test_ara_core_dir}/error_domain_test.cpp
    ${test_ara_core_dir}/error_code_test.cpp
    ${test_ara_core_dir}/instance_specifier_test.cpp
    ${test_ara_core_dir}/future_test.cpp
    ${test_ara_core_dir}/initialization_test.cpp
    ${test_ara_diag_dir}/obd_communication_test.cpp
    ${test_ara_diag_dir}/meta_info_test.cpp
    ${test_ara_diag_dir}/cancellation_handler_test.cpp
    ${test_ara_diag_dir}/event_test.cpp
    ${test_ara_diag_dir}/dtc_information_test.cpp
    ${test_ara_diag_dir}/condition_test.cpp
    ${test_ara_diag_dir}/operation_cycle_test.cpp
    ${test_ara_diag_dir}/conversation_test.cpp
    ${test_ara_diag_dir}/diag_error_domain_test.cpp
    ${test_ara_diag_dir}/generic_uds_service_test.cpp
    ${test_ara_diag_dir}/security_access_test.cpp
    ${test_ara_diag_dir}/ecu_reset_request_test.cpp
    ${test_ara_diag_dir}/generic_routine_test.cpp
    ${test_ara_diag_dir}/monitor_test.cpp
    ${test_ara_diag_dir}/doip_lib_test.cpp
    ${test_ara_diag_routing_dir}/testable_uds_service.h
    ${test_ara_diag_routing_dir}/uds_service_router_test.cpp
    ${test_ara_diag_routing_dir}/delay_timer_test.cpp
    ${test_ara_diag_routing_dir}/transfer_data_test.cpp
    ${test_ara_diag_routing_dir}/request_transfer_exit_test.cpp
    ${test_ara_diag_routing_dir}/nrc_exception_test.cpp
    ${test_ara_diag_routing_dir}/request_transfer_test.cpp
    ${test_ara_diag_debouncing_dir}/counter_based_debouncer_test.cpp
    ${test_ara_diag_debouncing_dir}/timer_based_debouncer_test.cpp
    ${test_ara_phm_dir}/recovery_action_test.cpp
    ${test_ara_phm_dir}/mocked_checkpoint_communicator.h
    ${test_ara_phm_dir}/supervised_entity_test.cpp
    ${test_ara_phm_supervisors_dir}/dummy_supervision.h
    ${test_ara_phm_supervisors_dir}/elementary_supervision_test.cpp
    ${test_ara_phm_supervisors_dir}/alive_supervision_test.cpp
    ${test_ara_phm_supervisors_dir}/deadline_supervision_test.cpp
    ${test_ara_phm_supervisors_dir}/global_supervision_test.cpp
    ${test_ara_phm_dir}/health_channel_test.cpp
    ${test_ara_phm_dir}/restart_recovery_action_test.cpp
    ${test_ara_phm_dir}/recovery_action_dispatcher_test.cpp
    ${test_ara_com_dir}/com_error_domain_test.cpp
    ${test_ara_com_dir}/types_test.cpp
    ${test_ara_com_dir}/sample_ptr_test.cpp
    ${test_ara_com_dir}/serialization_test.cpp
    ${test_ara_com_dir}/mock_event_binding.h
    ${test_ara_com_dir}/event_test.cpp
    ${test_ara_com_dir}/method_test.cpp
    ${test_ara_com_dir}/field_test.cpp
    ${test_ara_com_dir}/service_proxy_base_test.cpp
    ${test_ara_com_zerocopy_dir}/iceoryx_zerocopy_test.cpp
    ${test_ara_com_e2e_dir}/e2e_event_test.cpp
    ${test_ara_exec_dir}/signal_handler_test.cpp
    ${test_ara_exec_dir}/process_watchdog_test.cpp
    ${test_ara_per_dir}/per_error_domain_test.cpp
    ${test_ara_per_dir}/key_value_storage_test.cpp
    ${test_ara_per_dir}/file_storage_test.cpp
    ${test_ara_per_dir}/read_accessor_test.cpp
    ${test_ara_crypto_dir}/crypto_error_domain_test.cpp
    ${test_ara_crypto_dir}/crypto_provider_test.cpp
    ${test_ara_iam_dir}/iam_error_domain_test.cpp
    ${test_ara_iam_dir}/access_control_test.cpp
    ${test_ara_tsync_dir}/tsync_error_domain_test.cpp
    ${test_ara_tsync_dir}/time_sync_client_test.cpp
    ${test_ara_ucm_dir}/ucm_error_domain_test.cpp
    ${test_ara_ucm_dir}/update_manager_test.cpp
  )

  target_link_libraries(
    arxml_unit_test
    gtest_main
    arxml
  )

  target_link_libraries(
    ara_unit_test
    gtest_main
    obd_ii_emulator
    doip_lib
    ara_log
    ara_sm
    ara_phm
    ara_diag
    ara_per
    ara_crypto
    ara_iam
    ara_tsync
    ara_ucm
  )

  if(ARA_COM_USE_VSOMEIP)
    target_include_directories(
      ara_unit_test
      PRIVATE
      ${VSOMEIP_PREFIX}/include
    )
  endif()

  include(GoogleTest)
  gtest_discover_tests(arxml_unit_test)
  gtest_discover_tests(ara_unit_test)
 endif()

########################################################################
#
# Install / package export:

set(autosar_ap_install_targets
  arxml
  ara_core
  ara_log
  ara_com
  ara_sm
  ara_exec
  ara_diag
  ara_phm
  ara_per
  ara_crypto
  ara_iam
  ara_tsync
  ara_ucm
)

if(TARGET async_bsd_socket_lib)
  list(APPEND autosar_ap_install_targets async_bsd_socket_lib)
endif()

install(
  TARGETS ${autosar_ap_install_targets}
  EXPORT AdaptiveAutosarAPTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET adaptive_autosar)
  install(
    TARGETS adaptive_autosar
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

if(TARGET autosar_vsomeip_routing_manager)
  install(
    TARGETS autosar_vsomeip_routing_manager
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

install(
  TARGETS
    autosar_time_sync_daemon
    autosar_persistency_guard
    autosar_iam_policy_loader
    autosar_can_interface_manager
    autosar_watchdog_supervisor
    autosar_user_app_monitor
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/src/ara
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
)

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/src/arxml
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/configuration")
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/configuration/
    DESTINATION configuration
  )
endif()

if(DEFINED ASYNC_BSD_SOCKET_LIB_SOURCE_DIR AND EXISTS "${ASYNC_BSD_SOCKET_LIB_SOURCE_DIR}/include")
  install(
    DIRECTORY ${ASYNC_BSD_SOCKET_LIB_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()

if(AUTOSAR_AP_INSTALL_TOOLS)
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/tools/arxml_generator
    DESTINATION tools
  )
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/tools/sample_runner
    DESTINATION tools
  )
endif()

if(AUTOSAR_AP_INSTALL_USER_APP_TEMPLATE AND EXISTS "${CMAKE_SOURCE_DIR}/user_apps/CMakeLists.txt")
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/user_apps/
    DESTINATION user_apps
  )
endif()

set(ADAPTIVE_AUTOSAR_AP_CMAKE_INSTALL_DIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/AdaptiveAutosarAP")

configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/AdaptiveAutosarAPConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveAutosarAPConfig.cmake
  INSTALL_DESTINATION ${ADAPTIVE_AUTOSAR_AP_CMAKE_INSTALL_DIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveAutosarAPConfigVersion.cmake
  VERSION 0.1.0
  COMPATIBILITY SameMajorVersion
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveAutosarAPConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveAutosarAPConfigVersion.cmake
  DESTINATION ${ADAPTIVE_AUTOSAR_AP_CMAKE_INSTALL_DIR}
)

install(
  EXPORT AdaptiveAutosarAPTargets
  FILE AdaptiveAutosarAPTargets.cmake
  NAMESPACE AdaptiveAutosarAP::
  DESTINATION ${ADAPTIVE_AUTOSAR_AP_CMAKE_INSTALL_DIR}
)
