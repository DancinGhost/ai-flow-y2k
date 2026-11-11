cmake_minimum_required(VERSION 3.22)
project(ai-flow-y2k VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt AUTOMOC, AUTORCC, AUTOUIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 1. Functional Core Library (pure domain logic, reducers, parsers)
set(CORE_SOURCES
    src/core/policy_engine.cpp
    src/core/stream_parser.cpp
    src/core/theme_engine.cpp
    src/core/flow_reducer.cpp
)

add_library(aiflow_core STATIC ${CORE_SOURCES})
target_link_libraries(aiflow_core PUBLIC Qt6::Core)
target_include_directories(aiflow_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 2. Imperative Shell & GUI Executable
set(APP_HEADERS
    include/shell/proxy_server.hpp
    include/shell/simulator.hpp
    include/ui/visualizer_widget.hpp
    include/ui/tool_approval_dialog.hpp
    include/ui/main_window.hpp
)

set(APP_SOURCES
    ${APP_HEADERS}
    src/shell/proxy_server.cpp
    src/shell/simulator.cpp
    src/ui/visualizer_widget.cpp
    src/ui/tool_approval_dialog.cpp
    src/ui/main_window.cpp
    src/main.cpp
)

add_executable(ai-flow-y2k ${APP_SOURCES})
target_link_libraries(ai-flow-y2k PRIVATE
    aiflow_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# 3. Unit Tests Executable
add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE aiflow_core Qt6::Core)

enable_testing()
add_test(NAME CoreTests COMMAND test_core)

# Install rules
install(TARGETS ai-flow-y2k DESTINATION bin)
