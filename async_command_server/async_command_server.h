// async_command_server.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <boost/asio.hpp>
#include "async_acceptor.h"

using namespace boost;

template<size_t workingThreadCount = 2u>
class AsyncCommandServer
{
public:

  AsyncCommandServer() = delete;

  AsyncCommandServer(
    const asio::ip::address_v4 newAddress,
    const uint16_t newPortNumber,
    const size_t newBulkSize,
    const char newBulkOpenDelimiter = '{',
    const char newBulkCloseDelimiter = '}',
    std::ostream& newOutputStream = std::cout,
    std::ostream& newErrorStream = std::cerr,
    std::ostream& newMetricsStream = std::cout
  ) :
  address{newAddress},
  portNumber{newPortNumber},
  service{},

  asyncAcceptor{std::make_unique<AsyncAcceptor>(
    newAddress,
    newPortNumber,
    service,
    newBulkSize,
    newBulkOpenDelimiter,
    newBulkCloseDelimiter,
    newOutputStream,
    newErrorStream,
    newMetricsStream
  )},
  errorStream{newErrorStream},
  outputLock{asyncAcceptor->getScreenOutputLock()}
  {}

  ~AsyncCommandServer()
  {
    stop();
  }

  void start()
  {
    asyncAcceptor->start();

    for (size_t idx{0}; idx < workingThreadCount; ++idx)
    {
      workingThreads.push_back(std::thread{&AsyncCommandServer::run, this});
    }
  }

  void stop()
  {
    asyncAcceptor->stop();

    for (auto& thread : workingThreads)
    {
      if (thread.joinable() == true)
      {
        thread.join();
      }
    }

    service.stop();
  }

  std::mutex& getScreenOutputLock()
  {
    return outputLock;
  }

  const SharedGlobalMetrics getMetrics()
  {
    return asyncAcceptor->getMetrics();
  }

private:

  void run() noexcept
  {
    try
    {
      service.run();
    }
    catch (const std::exception& ex)
    {
      std::lock_guard<std::mutex> lockOutput{outputLock};
      errorStream << "Server stopped. Reason: " << ex.what() << '\n';
    }
  }


  asio::ip::address_v4 address;
  uint16_t portNumber;
  asio::io_service service;
  std::unique_ptr<AsyncAcceptor> asyncAcceptor;

  std::vector<std::thread> workingThreads;

  std::ostream& errorStream;
  std::mutex& outputLock;
};
