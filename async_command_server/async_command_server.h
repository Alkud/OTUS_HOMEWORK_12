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

  void start() noexcept;

  void stop();

private:
  asio::ip::address_v4 address;
  uint16_t portNumber;
  asio::io_service service;
  std::unique_ptr<AsyncAcceptor> asyncAcceptor;
};