// async_acceptor.cpp in Otus homework#12 project

#include "async_acceptor.h"

#include <string>
#include <vector>
#include <boost/bind.hpp>

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
endpoint{address, portNumber}, acceptor{service, endpoint},

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

shouldExit{false},
errorStream{newErrorStream},
outputLock{processor->getScreenOutputLock()}
{}

void AsyncAcceptor::start()
{  
  processor->connect();
  doAccept();
}

void AsyncAcceptor::stop()
{
  shouldExit.store(true);

  if (currentReader != nullptr)
  {
    currentReader->stop();
  }

  if (processor != nullptr)
  {
    processor->disconnect();
  }

  acceptor.cancel();
}

void AsyncAcceptor::doAccept()
{
  auto socket {std::make_shared<asio::ip::tcp::socket>(service)};

  acceptor.async_accept(*socket.get(), [this, socket](const system::error_code& error)
  {
    if (error != 0)
    {
      std::lock_guard<std::mutex> lockOutput{outputLock};

      errorStream << "Acceptor stopped. Reason: "
                  << error.message()
                  << ". Error code: " << error.value() << '\n';

      shouldExit.store(true);

      return;
    }

    onAcception(socket);
  });
}

void AsyncAcceptor::onAcception(SharedSocket acceptedSocket)
{
  currentReader.reset( new AsyncReader(
    acceptedSocket, processor, errorStream, outputLock
  ));

  if (shouldExit.load() != true)
  {    
    currentReader->start();
    //acceptor.listen();
    doAccept();
  }
  else
  {
    processor->disconnect();
    acceptor.close();
  }
}
