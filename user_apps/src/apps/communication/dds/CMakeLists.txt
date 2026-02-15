add_user_template_target(
  autosar_user_com_dds_pub_template
  pub_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_com
  AdaptiveAutosarAP::ara_log
)

add_user_template_target(
  autosar_user_com_dds_sub_template
  sub_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_com
  AdaptiveAutosarAP::ara_log
)

configure_user_apps_dds_app_target(autosar_user_com_dds_pub_template)
configure_user_apps_dds_app_target(autosar_user_com_dds_sub_template)
