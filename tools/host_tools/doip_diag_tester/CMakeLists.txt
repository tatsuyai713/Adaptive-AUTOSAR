cmake_minimum_required(VERSION 3.14)
project(AdaptiveAutosarHostDoipDiagTester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
  autosar_host_doip_diag_tester
  doip_diag_tester_app.cpp
)

install(
  TARGETS autosar_host_doip_diag_tester
  RUNTIME DESTINATION bin
)
