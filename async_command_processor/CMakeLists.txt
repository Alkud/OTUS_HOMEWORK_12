#CmakeList.txt for command_processor library in OTUS Homework #10

# set library name and content
add_library(processor
  async_worker.h
  command_processor_instance.h
  broadcasters.h
  listeners.h
  async_command_processor.h  
  input_processor.h input_processor.cpp
  input_reader.h input_reader.cpp
  logger_mt.h
  smart_buffer_mt.h
  publisher_mt.h publisher_mt.cpp
  weak_ptr_less.h
  thread_metrics.h
)

target_link_libraries(processor pthread)
