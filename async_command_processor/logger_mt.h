// logger.h in Otus homework#11 project

#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>
#include <thread>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <unistd.h>
#include <numeric>
#include "listeners.h"
#include "smart_buffer_mt.h"
#include "thread_metrics.h"
#include "async_worker.h"


template <size_t threadCount = 2u>
class Logger : public NotificationListener,
               public MessageListener,
               public MessageBroadcaster,
               public std::enable_shared_from_this<NotificationListener>,
               public AsyncWorker<threadCount>
{
public:

  Logger(const std::string& newWorkerName,
         const SharedSizeStringBuffer& newBuffer,
         std::ostream& newErrorOut, std::mutex& newErrorOutLock,
         const std::string& newDestinationDirectory = "") :
    AsyncWorker<threadCount>{newWorkerName},
    buffer{newBuffer}, destinationDirectory{newDestinationDirectory},
    previousTimeStamp{}, additionalNameSection{},
    errorOut{newErrorOut}, errorOutLock{newErrorOutLock},
    threadMetrics{}
  {
    if (nullptr == buffer)
    {
      throw(std::invalid_argument{"Logger source buffer not defined!"});
    }

    for (size_t threadIndex{0}; threadIndex < threadCount; ++threadIndex)
    {
      threadMetrics.push_back(std::make_shared<ThreadMetrics>(
          std::string{"logger thread#"} + std::to_string(threadIndex)
      ));
      additionalNameSection.push_back(1u);
    }

  }

  ~Logger()
  {
    this->stop();
  }

  void reactNotification(NotificationBroadcaster* sender) override
  {
    if (buffer.get() == sender)
    {
      #ifdef NDEBUG
      #else
        //std::cout << this->workerName << " reactNotification\n";
      #endif

      ++this->notificationCount;
      this->threadNotifier.notify_one();
    }
  }

  void reactMessage(class MessageBroadcaster*, Message message) override
  {
    if (messageCode(message) < 1000) // non error message
    {
      switch(message)
      {
        case Message::NoMoreData :
        this->noMoreData.store(true);

          #ifdef NDEBUG
          #else
            //std::cout << "\n                     " << this->workerName<< " NoMoreData received\n";
          #endif

          this->threadNotifier.notify_all();
          break;

      default:
        break;
      }
    }
    else                             // error message
    {
      if (this->shouldExit.load() != true)
      {
        this->shouldExit.store(true);
        this->threadNotifier.notify_all();
        sendMessage(message);
      }
    }
  }

  auto getStringThreadID()
  {
    return std::make_shared<std::vector<std::string>>(this->stringThreadID);
  }

  const SharedMultyMetrics getMetrics()
  {
    return threadMetrics;
  }

private:

  bool threadProcess(const size_t threadIndex) override
  {
    if (nullptr == buffer)
    {
      throw(std::invalid_argument{"Logger source buffer not defined!"});
    }

    auto bufferReply{buffer->getItem(shared_from_this())};

    if (false == bufferReply.first)
    {
      #ifdef NDEBUG
      #else
        //std::cout << "\n                     " << this->workerName<< " FALSE received\n";
      #endif

      return false;
    }

    auto nextBulkInfo{bufferReply.second};

    if (nextBulkInfo.first != previousTimeStamp)
    {
      additionalNameSection[threadIndex] = 1u;
      previousTimeStamp = nextBulkInfo.first;
    }

    std::string bulkFileName{
      destinationDirectory + std::to_string(nextBulkInfo.first)
    };

    std::stringstream fileNameSuffix{};
    fileNameSuffix << ::getpid()<< "-" << this->stringThreadID[threadIndex]
                   << "_" << additionalNameSection[threadIndex];
    auto logFileName {bulkFileName + "_" + fileNameSuffix.str() + ".log"};


    std::ofstream logFile{logFileName, std::ios::trunc};

    if(!logFile)
    {
      std::lock_guard<std::mutex> lockErrorOut{errorOutLock};
      errorOut << "Cannot create log file " <<
                  logFileName << " !" << std::endl;
      throw(std::ios_base::failure{"Log file creation error!"});
    }

    logFile << nextBulkInfo.second << '\n';
    logFile.close();

    ++additionalNameSection[threadIndex];

    /* Refresh metrics */
    ++threadMetrics[threadIndex]->totalBulkCount;
    threadMetrics[threadIndex]->totalCommandCount
        += std::count(nextBulkInfo.second.begin(),
                      nextBulkInfo.second.end(), ',') + 1;

    return true;
  }

  void onThreadException(const std::exception& ex, const size_t threadIndex) override
  {
    {
      std::lock_guard<std::mutex> lockErrorOut{errorOutLock};
      errorOut << this->workerName << " thread #" << threadIndex << " stopped. Reason: " << ex.what() << std::endl;
    }

    this->threadFinished[threadIndex].store(true);
    this->shouldExit.store(true);
    this->threadNotifier.notify_all();

    if (ex.what() == std::string{"Buffer is empty!"})
    {
      errorMessage = Message::BufferEmpty;
    }

    sendMessage(errorMessage);
  }

  void onTermination(const size_t) override
  {
    #ifdef NDEBUG
    #else
      //std::cout << "\n                     " << this->workerName<< " AllDataLogged\n";
    #endif

    if (true == this->noMoreData.load() && this->notificationCount.load() == 0)
    {
      sendMessage(Message::AllDataLogged);
    }
  }


  SharedSizeStringBuffer buffer;
  std::string destinationDirectory;

  size_t previousTimeStamp;
  std::vector<size_t> additionalNameSection;

  std::ostream& errorOut;
  std::mutex& errorOutLock;

  SharedMultyMetrics threadMetrics;

  Message errorMessage{Message::SystemError};
};
