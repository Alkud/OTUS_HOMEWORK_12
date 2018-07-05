// async_acceptor.cpp in Otus homework#12 project

#include "async_acceptor.h"

#include <string>
#include <vector>
#include <chrono>
#include <boost/bind.hpp>

using namespace std::chrono_literals;

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

openDelimiter{newBulkOpenDelimiter}, closeDelimiter{newBulkCloseDelimiter},

currentReader{}, activeReaderCount{},
shouldExit{false},
errorStream{newErrorStream},
outputLock{processor->getScreenOutputLock()},
metrics {}
{}

void AsyncAcceptor::start()
{  
  processor->connect();
  doAccept();
}

void AsyncAcceptor::stop()
{
  shouldExit.store(true);

  while (activeReaderCount.load() != 0)
  {
    std::unique_lock<std::mutex> lockTermination{terminationLock};
    terminationNotifier.wait_for(lockTermination, 100ms, [this]()
    {
      std::cout << "\nAcceptor waiting. Active readers: " << activeReaderCount.load() << "\n";

      return activeReaderCount.load() == 0;
    });
    lockTermination.unlock();
  }

  if (acceptor.is_open())
  {
    acceptor.close();
  }

  if (processor != nullptr)
  {
    processor->disconnect();
  }
}

void AsyncAcceptor::doAccept()
{
  auto socket {std::make_shared<asio::ip::tcp::socket>(service)};

  acceptor.async_accept(*socket.get(), [this, socket](const system::error_code& error)
  {
    if (error != 0)
    {
      if (error.value() != 125)
      {
        std::lock_guard<std::mutex> lockOutput{outputLock};

        errorStream << "Acceptor stopped. Reason: "
                    << error.message()
                    << ". Error code: " << error.value() << '\n';
      }

      if (shouldExit.load() != true)
      {
        stop();
      }

      return;
    }

    if (shouldExit.load() != true)
    {
      onAcception(socket);
    }
  });
}

void AsyncAcceptor::onAcception(SharedSocket acceptedSocket)
{
  currentReader.reset( new AsyncReader(
    acceptedSocket, processor,
    openDelimiter, closeDelimiter,
    acceptor, activeReaderCount,
    terminationNotifier, terminationLock,
    errorStream, outputLock
  ));

  currentReader->start();

  if (shouldExit.load() != true)
  {
    doAccept();
  }
}
