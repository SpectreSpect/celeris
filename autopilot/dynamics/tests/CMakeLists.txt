cmake_minimum_required(VERSION 3.20)

project(celeris_dynamics_contract_tests LANGUAGES CXX)

enable_testing()

add_executable(trajectory_contract_tests trajectory_contract_tests.cpp)
target_compile_features(trajectory_contract_tests PRIVATE cxx_std_20)
set_target_properties(trajectory_contract_tests PROPERTIES CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(trajectory_contract_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME trajectory_contract COMMAND trajectory_contract_tests)

add_executable(component_contract_tests component_contract_tests.cpp)
target_compile_features(component_contract_tests PRIVATE cxx_std_20)
set_target_properties(component_contract_tests PROPERTIES CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(component_contract_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME component_contracts COMMAND component_contract_tests)
set_tests_properties(trajectory_contract component_contracts PROPERTIES TIMEOUT 30)
