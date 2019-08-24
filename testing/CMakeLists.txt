cmake_minimum_required(VERSION 2.4.6)
project( pyconnect_testing )

add_definitions(-DRELEASE)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
add_definitions(-DBSD_COMPAT -std=c++11)
link_directories(/usr/local/opt/openssl/lib)
endif()

include_directories(${PROJECT_SOURCE_DIR}/..)
link_directories(${PROJECT_SOURCE_DIR}/../lib)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable( test_one test_sample1.cpp )
add_executable( test_two test_sample2.cpp )

set_target_properties (test_two PROPERTIES COMPILE_DEFINITIONS "HAS_OWN_MAIN_LOOP")

target_link_libraries(test_one pyconnect_wrapper crypto pthread)
target_link_libraries(test_two pyconnect_wrapper crypto pthread)
