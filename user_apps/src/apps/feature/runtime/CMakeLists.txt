if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/runtime_lifecycle_app.cpp")
  add_user_template_target(
    autosar_user_tpl_runtime_lifecycle
    runtime_lifecycle_app.cpp
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_log
  )
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/exec_runtime_monitor_app.cpp")
  add_user_template_target(
    autosar_user_tpl_exec_runtime_monitor
    exec_runtime_monitor_app.cpp
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_exec
    AdaptiveAutosarAP::ara_log
    AdaptiveAutosarAP::ara_com
  )
endif()
