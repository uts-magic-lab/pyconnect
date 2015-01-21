cmake_minimum_required(VERSION 2.4.6)
project( pyconnect_wrapper )

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

add_definitions(-DRELEASE)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
add_definitions(-DBSD_COMPAT)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
add_definitions(-DLINUX)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
add_definitions(-DWIN_32 -DWIN32_LEAN_AND_MEAN)
target_link_libraries(${PROJECT_NAME} ws2_32 Kernel32)
endif()

add_library( ${PROJECT_NAME} PyConnectCommon.cpp
                                PyConnectWrapper.cpp
                                PyConnectNetComm.cpp
                                PyConnectObjComm.cpp)

target_link_libraries(${PROJECT_NAME} crypto pthread)

install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION lib)

install(FILES ${PROJECT_SOURCE_DIR}/PyConnectCommon.h
              ${PROJECT_SOURCE_DIR}/PyConnectWrapper.h
              ${PROJECT_SOURCE_DIR}/PyConnectNetComm.h
              ${PROJECT_SOURCE_DIR}/PyConnectObjComm.h
                                 DESTINATION include/pyconnect )
