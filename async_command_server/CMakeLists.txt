#CmakeList.txt for command_processor library in OTUS Homework #10

# set library name and content
add_library(server
  async_command_server.h async_command_server.cpp
  async_acceptor.h async_acceptor.cpp
  async_reader.h async_reader.cpp
)

find_package(Boost COMPONENTS system REQUIRED)

target_link_libraries(server pthread ${Boost_LIBRARIES})
