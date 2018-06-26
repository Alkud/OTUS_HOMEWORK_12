// async_command_server.cpp in Otus homework#12 project

#include "async_command_server.h"


AsyncCommandServer::AsyncCommandServer(const asio::ip::address_v4 newAddress,
                                       const uint16_t newPortNumber,
                                       const size_t newBulkSize,
                                       const char newBulkOpenDelimiter,
                                       const char newBulkCloseDelimiter,
                                       std::ostream& newOutputStream,
                                       std::ostream& newErrorStream,
                                       std::ostream& newMetricsStream) :
  address{newAddress},
  portNumber{newPortNumber},
  service{},
  asyncAcceptor{std::make_unique<AsyncAcceptor>(newAddress,
                                                newPortNumber,
                                                service,
                                                newBulkSize,
                                                newBulkOpenDelimiter,
                                                newBulkCloseDelimiter,
                                                newOutputStream,
                                                newErrorStream,
                                                newMetricsStream)}
{}

void AsyncCommandServer::start() noexcept
{
  try
  {
    service.run();
  }
  catch(const std::exception& ex)
  {
    std::cerr << ex.what() << std::endl;
  }
}
