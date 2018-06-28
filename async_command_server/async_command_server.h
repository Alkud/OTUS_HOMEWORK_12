// async_command_server.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <boost/asio.hpp>
#include "async_acceptor.h"

using namespace boost;

class AsyncCommandServer
{
public:

  AsyncCommandServer() = delete;

  AsyncCommandServer(const asio::ip::address_v4 newAddress,
                     const uint16_t newPortNumber,
                     const size_t newBulkSize,
                     const char newBulkOpenDelimiter = '{',
                     const char newBulkCloseDelimiter = '}',
                     std::ostream& newOutputStream = std::cout,
                     std::ostream& newErrorStream = std::cerr,
                     std::ostream& newMetricsStream = std::cout);

  ~AsyncCommandServer();

  void start();

  void stop();

  std::mutex& getScreenOutputLock()
  { return outputLock; }

  const SharedGlobalMetrics getMetrics()
  {
    return asyncAcceptor->getMetrics();
  }

private:

  void run() noexcept;
  asio::ip::address_v4 address;
  uint16_t portNumber;
  asio::io_service service;
  std::unique_ptr<AsyncAcceptor> asyncAcceptor;

  std::vector<std::thread> workingThreads;

  std::ostream& errorStream;
  std::mutex& outputLock;
};
