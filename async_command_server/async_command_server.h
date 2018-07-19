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
    const char newBulkOpenDelimiter,
    const char newBulkCloseDelimiter,
    std::ostream& newOutputStream,
    std::ostream& newErrorStream,
    std::ostream& newMetricsStream,
    bool stressTestNeeded,
    std::atomic<bool>& newTerminationFlag,
    std::condition_variable& newTerminationNotifier
  ) :
  address{newAddress},
  portNumber{newPortNumber},
  service{},

  stressTesting{stressTestNeeded},

  commandProcessor{
    new AsyncCommandProcessor<2> (
    std::string{"Command processor @"} + address.to_string() + ":" + std::to_string(portNumber),
    newBulkSize,
    newBulkOpenDelimiter,
    newBulkCloseDelimiter,
    newOutputStream,
    newErrorStream,
    newMetricsStream,
    processorFailed,
    terminationNotifier,
    stressTesting
    )
  },

  terminationLock{},
  terminationNotifier{},
  acceptorStopped{true},
  processorFailed{false},

  asyncAcceptor{ new AsyncAcceptor (
    newAddress,
    newPortNumber,
    service,
    commandProcessor,
    newBulkOpenDelimiter,
    newBulkCloseDelimiter,
    newErrorStream,
    terminationNotifier,
    acceptorStopped
  )},  

  shouldExit{false},

  errorStream{newErrorStream},
  outputLock{asyncAcceptor->getScreenOutputLock()},

  appTerminationFlag{newTerminationFlag},
  appTerminationNotifier{newTerminationNotifier}
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

    if (controller.joinable())
    {
      controller.join();
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

    controller =std::thread{[this]()
    {
      std::unique_lock<std::mutex> lockTermination{terminationLock};
      terminationNotifier.wait(lockTermination,[this]()
      {
        return (acceptorStopped.load() == true
                || processorFailed.load() == true);
      });

      if (processorFailed.load() == true)
      {
        stop();
        appTerminationFlag.store(true);
        appTerminationNotifier.notify_all();
      }
    }};
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

  bool stressTesting;

  std::shared_ptr<AsyncCommandProcessor<2>> commandProcessor;

  std::mutex terminationLock;
  std::condition_variable terminationNotifier;
  std::atomic<bool> acceptorStopped;
  std::atomic<bool> processorFailed;

  std::unique_ptr<AsyncAcceptor> asyncAcceptor;

  std::vector<std::thread> workingThreads;
  std::thread controller;

  std::atomic<bool> shouldExit;

  std::ostream& errorStream;
  std::mutex& outputLock;

  std::atomic<bool>& appTerminationFlag;
  std::condition_variable& appTerminationNotifier;
};
