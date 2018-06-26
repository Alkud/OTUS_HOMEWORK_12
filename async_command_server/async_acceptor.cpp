// async_acceptor.cpp in Otus homework#12 project

#include "async_acceptor.h"

#include <string>
#include <vector>

AsyncAcceptor::AsyncAcceptor(
  const asio::ip::address_v4 newAddress,
  const uint16_t newPortNumber,
  asio::io_service& newService,
  const size_t newBulkSize,
  const char newBulkOpenDelimiter,
  const char newBulkCloseDelimiter,
  std::ostream& newOutputStream,
  std::ostream& newErrorStream,
  std::ostream& newMetricsStream
):
address{newAddress}, portNumber{newPortNumber}, service{newService},
endpoint{address, portNumber}, socket{service}, acceptor{service, endpoint},
readBuffer{std::make_unique<char[]>(READ_BUFFER_SIZE)},

processor{
  std::make_unique<AsyncCommandProcessor<2>> (
    std::string{"Command processor @"} + address.to_string() + ":" + std::to_string(portNumber),
    newBulkSize,
    newBulkOpenDelimiter,
    newBulkCloseDelimiter,
    newOutputStream,
    newErrorStream,
    newMetricsStream
  )
},

shouldExit{false}
{}

void AsyncAcceptor::start()
{
  acceptor.listen();
  processor->connect();
    acceptor.async_accept(socket, &AsyncAcceptor::onAcception);
    }

void AsyncAcceptor::stop()
{
  shouldExit.store(true);
}

void AsyncAcceptor::onAcception(const system::error_code& error)
{
  if (error != 0)
  {
    std::cerr << "Acceptor stopped. Reason: "
              << error.message()
              << ". Error code: " << error.value();

    shouldExit.store(true);

    return;
  }

  socket.async_read_some(asio::buffer(readBuffer.get(), READ_BUFFER_SIZE), &AsyncAcceptor::onRead);

  if (shouldExit.load() != true)
  {
    acceptor.async_accept(socket, &AsyncAcceptor::onAcception);
  }
  else
  {
    processor->disconnect();
    acceptor.close();
  }
}

void AsyncAcceptor::onRead(const system::error_code& error, std::size_t bytes_transferred)
{
  if (error != 0)
  {
    std::cerr << "Acceptor stopped. Reason: "
              << error.message()
              << ". Error code: " << error.value();

    shouldExit.store(true);

    return;
  }

  processor->receiveData(readBuffer.get(), bytes_transferred);

  std::fill_n(readBuffer.get(), READ_BUFFER_SIZE, 0);
}

