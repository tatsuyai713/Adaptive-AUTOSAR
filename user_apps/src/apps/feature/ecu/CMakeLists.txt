if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/ecu_full_stack_app.cpp")
  set(user_apps_ecu_shared_sources
    "${USER_APPS_FEATURE_ECU_DIR}/ecu_sample_common.h"
    "${USER_APPS_FEATURE_ECU_DIR}/ecu_sample_common.cpp"
    "${USER_APPS_FEATURE_PUBSUB_DIR}/pubsub_autosar_portable_api.h"
    "${USER_APPS_FEATURE_PUBSUB_DIR}/pubsub_autosar_portable_api.cpp"
    "${USER_APPS_FEATURE_PUBSUB_DIR}/pubsub_common.h"
    "${USER_APPS_FEATURE_PUBSUB_DIR}/pubsub_common.cpp"
  )

  set(user_apps_ecu_can_sources
    "${USER_APPS_FEATURE_ECU_DIR}/ecu_sample_can_common.cpp"
    "${USER_APPS_FEATURE_CAN_DIR}/can_frame_receiver.h"
    "${USER_APPS_FEATURE_CAN_DIR}/socketcan_receiver.h"
    "${USER_APPS_FEATURE_CAN_DIR}/socketcan_receiver.cpp"
    "${USER_APPS_FEATURE_CAN_DIR}/mock_can_receiver.h"
    "${USER_APPS_FEATURE_CAN_DIR}/mock_can_receiver.cpp"
  )

  add_executable(
    autosar_user_tpl_ecu_full_stack
    ecu_full_stack_app.cpp
    ${user_apps_ecu_shared_sources}
    ${user_apps_ecu_can_sources}
    "${USER_APPS_FEATURE_CAN_DIR}/vehicle_status_can_decoder.h"
    "${USER_APPS_FEATURE_CAN_DIR}/vehicle_status_can_decoder.cpp"
  )
  configure_user_apps_ecu_feature_target(autosar_user_tpl_ecu_full_stack)
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/ecu_someip_source_app.cpp")
  set(user_apps_ecu_someip_shared_sources
    "${USER_APPS_FEATURE_PUBSUB_DIR}/pubsub_autosar_portable_api.h"
    "${USER_APPS_FEATURE_PUBSUB_DIR}/pubsub_autosar_portable_api.cpp"
    "${USER_APPS_FEATURE_PUBSUB_DIR}/pubsub_common.h"
    "${USER_APPS_FEATURE_PUBSUB_DIR}/pubsub_common.cpp"
  )

  add_executable(
    autosar_user_tpl_ecu_someip_source
    ecu_someip_source_app.cpp
    ${user_apps_ecu_someip_shared_sources}
  )
  configure_user_apps_feature_transport_target(autosar_user_tpl_ecu_someip_source)
endif()
