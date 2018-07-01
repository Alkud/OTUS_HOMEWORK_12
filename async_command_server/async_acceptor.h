// async_acceptor.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <array>
#include <condition_variable>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include "async_reader.h"
#include "../async_command_processor/async_command_processor.h"

using namespace boost;

class AsyncAcceptor
{
public:

  using SharedSocket = std::shared_ptr<asio::ip::tcp::socket>;

  AsyncAcceptor() = delete;

  AsyncAcceptor(const asio::ip::address_v4 newAddress,
                const uint16_t newPortNumber,
                asio::io_service& newService,
                const size_t newBulkSize,
                const char newBulkOpenDelimiter = '{',
                const char newBulkCloseDelimiter = '}',
                std::ostream& newOutputStream = std::cout,
                std::ostream& newErrorStream = std::cerr,
                std::ostream& newMetricsStream = std::cout);

  void start();

  void stop();

  std::mutex& getScreenOutputLock()
  { return outputLock; }

  const SharedGlobalMetrics getMetrics()
  {
    return processor->getMetrics();
  }

private:

  void doAccept();
  void onAcception(SharedSocket acceptedSocket);

  asio::ip::address_v4 address;
  uint16_t portNumber;
  asio::io_service& service;
  asio::ip::tcp::endpoint endpoint;  
  asio::ip::tcp::acceptor acceptor;

  std::shared_ptr<AsyncCommandProcessor<2>> processor;

  SharedAsyncReader currentReader;

  std::atomic_uint64_t activeReaderCount;
  std::condition_variable terminationNotifier;
  std::mutex terminationLock;

  std::atomic_bool shouldExit;

  std::ostream& errorStream;
  std::mutex& outputLock;

  SharedGlobalMetrics metrics;
};
