add_user_template_target(
  autosar_user_com_someip_provider_template
  provider_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_com
  AdaptiveAutosarAP::ara_log
)

add_user_template_target(
  autosar_user_com_someip_consumer_template
  consumer_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_com
  AdaptiveAutosarAP::ara_log
)

if(USER_APPS_VSOMEIP_INCLUDE_DIR)
  target_include_directories(
    autosar_user_com_someip_provider_template
    SYSTEM
    PRIVATE
    "${USER_APPS_VSOMEIP_INCLUDE_DIR}"
  )
  target_include_directories(
    autosar_user_com_someip_consumer_template
    SYSTEM
    PRIVATE
    "${USER_APPS_VSOMEIP_INCLUDE_DIR}"
  )
else()
  message(WARNING "user_apps: vsomeip.hpp not found. SOME/IP apps may fail to compile.")
endif()
