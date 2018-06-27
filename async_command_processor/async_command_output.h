// async_command_output.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <list>
#include <thread>
#include <functional>
#include <condition_variable>
#include "smart_buffer_mt.h"
#include "publisher_mt.h"
#include "logger_mt.h"

template<size_t loggingThreadCount = 2u>
class AsyncCommandOutput :  public MessageBroadcaster,
                            public MessageListener,
                            public std::enable_shared_from_this<MessageListener>
{

private:
  std::mutex& screenOutputLock;

  SharedSizeStringBuffer outputBuffer;
  std::shared_ptr<Logger<loggingThreadCount>> logger;
  std::shared_ptr<Publisher> publisher;

  std::atomic_bool dataPublished;
  std::atomic_bool dataLogged;
  std::atomic_bool shouldExit;

  std::condition_variable terminationNotifier{};
  std::mutex notifierLock;

  std::ostream& errorOut;
  std::ostream& metricsOut;
  SharedGlobalMetrics globalMetrics;

  Message errorMessage{Message::SystemError};
};
