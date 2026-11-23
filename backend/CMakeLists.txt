cmake_minimum_required(VERSION 3.25)
project(lumina LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(LUMINA_BUILD_TESTS "Build unit tests" ON)

# Drogon's installed CMake package forces a MySQL lookup even though we only use
# PostgreSQL. Link the MSYS2 libraries directly to keep the build hermetic.
find_path(DROGON_INCLUDE_DIR drogon/drogon.h)
find_path(JSONCPP_INCLUDE_DIR json/json.h PATH_SUFFIXES jsoncpp REQUIRED)
find_library(DROGON_LIBRARY NAMES drogon REQUIRED)
find_library(TRANTOR_LIBRARY NAMES trantor REQUIRED)
find_library(JSONCPP_LIBRARY NAMES jsoncpp REQUIRED)
find_library(UUID_LIBRARY NAMES uuid)
find_library(BROTLI_ENC_LIBRARY NAMES brotlienc)
find_library(BROTLI_DEC_LIBRARY NAMES brotlidec)
find_library(BROTLI_COMMON_LIBRARY NAMES brotlicommon)
find_library(CARES_LIBRARY NAMES cares)

add_library(Drogon::Drogon INTERFACE IMPORTED)
set(DROGON_LINK_LIBRARIES "${DROGON_LIBRARY};${TRANTOR_LIBRARY};${JSONCPP_LIBRARY}")
foreach(OPTIONAL_DROGON_LIBRARY UUID_LIBRARY BROTLI_ENC_LIBRARY BROTLI_DEC_LIBRARY
                               BROTLI_COMMON_LIBRARY CARES_LIBRARY)
  if(DEFINED ${OPTIONAL_DROGON_LIBRARY} AND
     NOT "${${OPTIONAL_DROGON_LIBRARY}}" MATCHES "-NOTFOUND$" AND
     NOT "${${OPTIONAL_DROGON_LIBRARY}}" STREQUAL "")
    list(APPEND DROGON_LINK_LIBRARIES "${${OPTIONAL_DROGON_LIBRARY}}")
  endif()
endforeach()
set_target_properties(Drogon::Drogon PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${DROGON_INCLUDE_DIR};${JSONCPP_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES "${DROGON_LINK_LIBRARIES}")

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

find_library(SODIUM_LIBRARY sodium REQUIRED)
find_library(PQXX_LIBRARY pqxx REQUIRED)
find_library(XLSX_LIBRARY xlsxwriter REQUIRED)
find_library(HPDF_LIBRARY hpdf REQUIRED)
find_library(CURL_LIBRARY curl)
find_package(Threads REQUIRED)

file(GLOB_RECURSE LUMINA_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER LUMINA_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(lumina_lib STATIC ${LUMINA_SOURCES})
target_include_directories(lumina_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(lumina_lib PUBLIC
  Drogon::Drogon
  OpenSSL::SSL OpenSSL::Crypto
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  PostgreSQL::PostgreSQL
  ${PQXX_LIBRARY}
  ${SODIUM_LIBRARY}
  ${XLSX_LIBRARY}
  ${HPDF_LIBRARY}
  Threads::Threads
)
if(WIN32)
  target_link_libraries(lumina_lib PUBLIC ws2_32 crypt32 iphlpapi)
endif()
if(CURL_LIBRARY)
  target_link_libraries(lumina_lib PUBLIC ${CURL_LIBRARY})
  target_compile_definitions(lumina_lib PUBLIC LUMINA_HAVE_CURL=1)
endif()
if(XLSX_LIBRARY)
  target_compile_definitions(lumina_lib PUBLIC LUMINA_HAVE_XLSX=1)
endif()
if(HPDF_LIBRARY)
  target_compile_definitions(lumina_lib PUBLIC LUMINA_HAVE_HPDF=1)
endif()
target_compile_definitions(lumina_lib PUBLIC LUMINA_VERSION="1.0.0")
if(MSVC)
  target_compile_options(lumina_lib PUBLIC /W4 /permissive- /utf-8)
else()
  target_compile_options(lumina_lib PUBLIC -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(lumina_server src/main.cpp)
target_link_libraries(lumina_server PRIVATE lumina_lib)

add_executable(lumina_migrator tools/migrator.cpp)
target_link_libraries(lumina_migrator PRIVATE lumina_lib)

add_executable(lumina_seeder tools/seeder.cpp)
target_link_libraries(lumina_seeder PRIVATE lumina_lib)

add_executable(lumina_worker tools/worker.cpp)
target_link_libraries(lumina_worker PRIVATE lumina_lib)

if(LUMINA_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  file(GLOB_RECURSE LUMINA_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
  if(LUMINA_TEST_SOURCES)
    add_executable(lumina_tests ${LUMINA_TEST_SOURCES})
    target_link_libraries(lumina_tests PRIVATE lumina_lib GTest::gtest GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(lumina_tests)
  endif()
endif()

