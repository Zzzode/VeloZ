add_library(veloz_core
  src/event_loop.cpp
  src/logger.cpp
  src/time.cpp
  src/config.cpp
  src/error.cpp
  src/memory.cpp
  src/metrics.cpp
)

target_include_directories(veloz_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(veloz_core PUBLIC cxx_std_23)

# nlohmann/json library
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)

# websocketpp library
FetchContent_Declare(
  websocketpp
  GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
  GIT_TAG 0.8.2
)

FetchContent_MakeAvailable(nlohmann_json websocketpp)

# Try to find system libcurl first
find_package(CURL)

if(CURL_FOUND)
  target_link_libraries(veloz_core PUBLIC CURL::libcurl)
else()
  message(WARNING "libcurl not found on system - disabling curl support")
endif()

target_link_libraries(veloz_core PUBLIC nlohmann_json::nlohmann_json)

target_include_directories(veloz_core
  PUBLIC
    ${websocketpp_SOURCE_DIR}
)

