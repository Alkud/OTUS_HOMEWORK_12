// async_command_server.cpp in Otus homework#12 project

#include "async_command_server.h"


AsyncCommandServer::AsyncCommandServer(
    const asio::ip::address_v4 newAddress,
    const uint16_t newPortNumber,
    const size_t newBulkSize,
    const char newBulkOpenDelimiter,
    const char newBulkCloseDelimiter,
    std::ostream& newOutputStream,
    std::ostream& newErrorStream,
    std::ostream& newMetricsStream
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

AsyncCommandServer::~AsyncCommandServer()
{
  stop();
}

void AsyncCommandServer::start()
{
  asyncAcceptor->start();

  for (size_t idx{0}; idx < 4; ++idx)
  {
    workingThreads.push_back(std::thread{&AsyncCommandServer::run, this});
  }

  for (auto& thread : workingThreads)
  {
    if (thread.joinable() == true)
    {
      thread.join();
    }
  }

  asyncAcceptor->stop();
}

void AsyncCommandServer::stop()
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

void AsyncCommandServer::run() noexcept
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
