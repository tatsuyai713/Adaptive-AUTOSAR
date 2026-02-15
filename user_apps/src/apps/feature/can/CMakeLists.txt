if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/socketcan_receive_decode_app.cpp")
  add_executable(
    autosar_user_tpl_can_socketcan_receiver
    socketcan_receive_decode_app.cpp
    "${USER_APPS_FEATURE_CAN_DIR}/socketcan_receiver.cpp"
    "${USER_APPS_FEATURE_CAN_DIR}/mock_can_receiver.cpp"
    "${USER_APPS_FEATURE_CAN_DIR}/vehicle_status_can_decoder.cpp"
  )
  target_link_libraries(
    autosar_user_tpl_can_socketcan_receiver
    PRIVATE
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_log
  )
endif()
