# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include(FetchContent)
FetchContent_Declare(
  espidflib
  GIT_REPOSITORY https://github.com/UncleRus/esp-idf-lib.git
)
FetchContent_MakeAvailable(espidflib)
set(EXTRA_COMPONENT_DIRS ${espidflib_SOURCE_DIR}/components
             "${CMAKE_CURRENT_LIST_DIR}/libraries/backoffAlgorithm"
						 "${CMAKE_CURRENT_LIST_DIR}/libraries/coreMQTT"
						 "${CMAKE_CURRENT_LIST_DIR}/libraries/common/posix_compat"
						 "${CMAKE_CURRENT_LIST_DIR}/libraries/button"
						 "${CMAKE_CURRENT_LIST_DIR}/libraries/ws2812_led"
						 "${CMAKE_CURRENT_LIST_DIR}/libraries/qrcode"
             "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common"
						 "${CMAKE_CURRENT_LIST_DIR}/libraries/common/logging"
						 "${CMAKE_CURRENT_LIST_DIR}/main/mqtt"
						 "${CMAKE_CURRENT_LIST_DIR}/main/network_manager"
             "${CMAKE_CURRENT_LIST_DIR}/main/components"
  )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ip_internal_network)

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/certs/root_cert_auth.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/certs/client.crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/certs/client.key" TEXT)
