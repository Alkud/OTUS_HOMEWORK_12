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
                std::shared_ptr<AsyncCommandProcessor<2>> newProcessor,
                const char newBulkOpenDelimiter,
                const char newBulkCloseDelimiter,
                std::ostream& newErrorStream,
                std::condition_variable& newTerminationNotifier,
                std::atomic<bool>& newTerminationFlag);

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

  const char openDelimiter;
  const char closeDelimiter;

  SharedAsyncReader currentReader;

  std::atomic<size_t> activeReaderCount;
  std::mutex terminationLock;

  std::condition_variable& terminationNotifier;
  std::atomic<bool>& terminationFlag;

  std::atomic<bool> shouldExit;

  std::ostream& errorStream;
  std::mutex& outputLock;

  SharedGlobalMetrics metrics;
};
