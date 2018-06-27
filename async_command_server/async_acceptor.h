// async_acceptor.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <array>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include "../async_command_processor/async_command_processor.h"

using namespace boost;

constexpr size_t READ_BUFFER_SIZE = 128;

class AsyncAcceptor
{
public:
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

private:

  void onAcception();

  void onRead(std::size_t bytes_transferred);

  asio::ip::address_v4 address;
  uint16_t portNumber;
  asio::io_service& service;
  asio::ip::tcp::endpoint endpoint;
  asio::ip::tcp::socket socket;
  asio::ip::tcp::acceptor acceptor;

  std::unique_ptr<char[]> readBuffer;

  std::unique_ptr<AsyncCommandProcessor<2>> processor;
  std::atomic_bool shouldExit;

  std::ostream& errorStream;
  std::mutex& outputLock;
};
