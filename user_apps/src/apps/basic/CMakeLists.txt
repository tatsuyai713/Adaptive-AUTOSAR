add_user_template_target(
  autosar_user_minimal_runtime
  minimal_runtime_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_log
)

add_user_template_target(
  autosar_user_per_phm_demo
  per_phm_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_per
  AdaptiveAutosarAP::ara_phm
)

add_user_template_target(
  autosar_user_exec_signal_template
  exec_signal_app.cpp
  AdaptiveAutosarAP::ara_core
  AdaptiveAutosarAP::ara_exec
  AdaptiveAutosarAP::ara_log
)
