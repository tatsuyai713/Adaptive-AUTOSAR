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

# Raspberry Pi / Linux mock ECU — exercises all 11 ara::* modules without
# requiring external middleware (vsomeip, CycloneDDS, CAN hardware).
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/raspi_mock_ecu_app.cpp")
  find_package(OpenSSL QUIET)
  if(OpenSSL_FOUND OR OPENSSL_FOUND)
    add_user_template_target(
      autosar_user_raspi_mock_ecu
      raspi_mock_ecu_app.cpp
      AdaptiveAutosarAP::ara_core
      AdaptiveAutosarAP::ara_log
      AdaptiveAutosarAP::ara_exec
      AdaptiveAutosarAP::ara_phm
      AdaptiveAutosarAP::ara_per
      AdaptiveAutosarAP::ara_diag
      AdaptiveAutosarAP::ara_sm
      AdaptiveAutosarAP::ara_tsync
      AdaptiveAutosarAP::ara_nm
      AdaptiveAutosarAP::ara_crypto
      AdaptiveAutosarAP::ara_ucm
      OpenSSL::Crypto
    )
  endif()
endif()

# Raspberry Pi ECU application — production-style vehicle app that delegates
# platform concerns (diag, PHM, crypto, …) to dedicated daemons.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/raspi_ecu_app.cpp")
  add_user_template_target(
    autosar_user_raspi_ecu
    raspi_ecu_app.cpp
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_log
    AdaptiveAutosarAP::ara_exec
    AdaptiveAutosarAP::ara_phm
    AdaptiveAutosarAP::ara_per
    AdaptiveAutosarAP::ara_sm
  )
endif()
