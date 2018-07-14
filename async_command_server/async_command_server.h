// async_command_server.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <csignal>
#include <boost/asio.hpp>
#include "async_acceptor.h"

using namespace boost;

template<size_t workingThreadCount = 2u>
class AsyncCommandServer
{
public:

  AsyncCommandServer() = delete;

  AsyncCommandServer(
    const asio::ip::address_v4 newAddress,
    const uint16_t newPortNumber,
    const size_t newBulkSize,
    const char newBulkOpenDelimiter = '{',
    const char newBulkCloseDelimiter = '}',
    std::ostream& newOutputStream = std::cout,
    std::ostream& newErrorStream = std::cerr,
    std::ostream& newMetricsStream = std::cout
  ) :
  address{newAddress},
  portNumber{newPortNumber},
  service{},

  terminationLock{},
  terminationNotifier{},
  acceptorStopped{true},

  asyncAcceptor{ new AsyncAcceptor (
    newAddress,
    newPortNumber,
    service,
    newBulkSize,
    newBulkOpenDelimiter,
    newBulkCloseDelimiter,
    newOutputStream,
    newErrorStream,
    newMetricsStream,
    terminationNotifier,
    acceptorStopped
  )},  

  errorStream{newErrorStream},
  outputLock{asyncAcceptor->getScreenOutputLock()}
  {}

  ~AsyncCommandServer()
  {
    #ifdef NDEBUG
    #else
      //std::cout << "-- Server destructor\n";
    #endif

    if (acceptorStopped.load() != true)
    {
      stop();
    }
  }

  void start()
  {
    asyncAcceptor->start();

    acceptorStopped.store(false);

    for (size_t idx{0}; idx < workingThreadCount; ++idx)
    {
      workingThreads.push_back(std::thread{&AsyncCommandServer::run, this});
    }
  }

  void stop()
  {
    #ifdef NDEBUG
    #else
      //std::cout << "-- called Server::stop()\n";
    #endif

    service.stop();

    while(service.stopped() != true)
    {}

    std::this_thread::sleep_for(1s);

    asyncAcceptor->stop();

    #ifdef NDEBUG
    #else
      //std::cout << "-- waiting Acceptor termination. Termination flag: "
      //          << std::boolalpha << acceptorStopped.load() << "\n";
    #endif

    while (acceptorStopped.load() != true)
    {
      #ifdef NDEBUG
      #else
        //std::cout << "-- waiting Acceptor termination. Termination flag: "
        //          << std::boolalpha << acceptorStopped.load() << "\n";
      #endif

      std::unique_lock<std::mutex> lockTermination{terminationLock};
      terminationNotifier.wait_for(lockTermination, 100ms,[this]()
      {
        return acceptorStopped.load() == true;
      });
    }

    for (auto& thread : workingThreads)
    {
      if (thread.joinable() == true)
      {
        thread.join();
      }
    }

    //service.stop();

    #ifdef NDEBUG
    #else
      //std::cout << "-- Server stopped\n";
    #endif
  }

  std::mutex& getScreenOutputLock()
  {
    return outputLock;
  }

  const SharedGlobalMetrics getMetrics()
  {
    return asyncAcceptor->getMetrics();
  }

private:

  void run() noexcept
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


  asio::ip::address_v4 address;
  uint16_t portNumber;
  asio::io_service service;

  std::mutex terminationLock;
  std::condition_variable terminationNotifier;
  std::atomic<bool> acceptorStopped;

  std::unique_ptr<AsyncAcceptor> asyncAcceptor;

  std::vector<std::thread> workingThreads;  

  std::ostream& errorStream;
  std::mutex& outputLock;
};
