# CMakeLists.txt for ara::exec ExecutionManager orchestration demo apps.
#
# Three binaries are produced:
#   autosar_user_em_sensor_app   — managed sensor simulation (child process)
#   autosar_user_em_actuator_app — managed actuator simulation (child process)
#   autosar_user_em_daemon       — ExecutionManager orchestration daemon (parent)
#
# The daemon launches the sensor/actuator apps via fork()/exec().
# All three can be cross-compiled for Raspberry Pi (aarch64) via the standard
# cross-compilation toolchain defined in the project root CMake presets.

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/em_sensor_app.cpp")
  add_user_template_target(
    autosar_user_em_sensor_app
    em_sensor_app.cpp
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_exec
    AdaptiveAutosarAP::ara_log
  )
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/em_actuator_app.cpp")
  add_user_template_target(
    autosar_user_em_actuator_app
    em_actuator_app.cpp
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_exec
    AdaptiveAutosarAP::ara_log
  )
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/em_daemon_app.cpp")
  add_user_template_target(
    autosar_user_em_daemon
    em_daemon_app.cpp
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_exec
    AdaptiveAutosarAP::ara_com
    AdaptiveAutosarAP::ara_log
  )
endif()
