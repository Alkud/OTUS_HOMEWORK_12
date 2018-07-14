#CmakeList.txt for OTUS Homework #12

#Check CMake version
cmake_minimum_required(VERSION 3.2)

#Set project name, set PROJECT_VERSION_MINOR value equal to TRAVIS_BUILD NUMBER
if (DEFINED ENV{TRAVIS_BUILD_NUMBER})
  set (VERSION_PATCH $ENV{TRAVIS_BUILD_NUMBER})
else ()
  set (VERSION_PATCH 1)
endif()

set(CMAKE_CXX_STANDARD 14)

project(bulkserver VERSION 0.0.${VERSION_PATCH})

#add library subdirectories
add_subdirectory(async_command_processor)
add_subdirectory(async_command_server)

set (BOOST_MIN_VERSION "1.54.0")

#Find Boost library components
find_package(Boost COMPONENTS system filesystem unit_test_framework REQUIRED)
find_package(Threads)

set(BOOST_DEBUG                ON)
set(Boost_USE_MULTITHREADED    ON)

set(homework_12
    ${PROJECT_SOURCE_DIR}/homework_12.h
    ${PROJECT_SOURCE_DIR}/homework_12.cpp)

#Set executable target
add_executable(bulkserver bulkserver.cpp ${homework_12})

#Set test target_link_libraries
add_executable(otus_hw_12_test otus_hw_12_test.cpp)

#Enable tests
enable_testing()
add_test(NAME test_hw_12
         COMMAND otus_hw_12_test)

#Set properties and compile options for executable target
set_target_properties(bulkserver
                      PROPERTIES CXX_STANDARD 14
                      CXX_STANDARD_REQUIRED ON
                      COMPILE_OPTIONS -Wpedantic -Wall -Wextra
)

#Set properties for test target
set_target_properties(otus_hw_12_test PROPERTIES
                      COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
                      INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
)

#Link
target_link_libraries(bulkserver pthread server processor ${Boost_LIBRARIES})

target_link_libraries(otus_hw_12_test pthread server processor ${Boost_LIBRARIES})

install(TARGETS bulkserver RUNTIME DESTINATION bin)


set(CPACK_GENERATOR DEB)

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_CONTACT alexandr_kudinov@mail.ru)

include(CPack)
