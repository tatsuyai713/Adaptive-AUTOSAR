if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/runtime_lifecycle_app.cpp")
  add_user_template_target(
    autosar_user_tpl_runtime_lifecycle
    runtime_lifecycle_app.cpp
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_log
  )
endif()
