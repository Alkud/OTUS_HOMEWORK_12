// logger.h in Otus homework#11 project

#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>
#include <list>
#include <unordered_set>
#include <thread>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <unistd.h>
#include <numeric>
#include "listeners.h"
#include "simple_buffer_mt.h"
#include "thread_metrics.h"
#include "async_worker.h"


using namespace std::chrono_literals;

template <size_t threadCount = 2u>
class Logger : public NotificationListener,
               public MessageListener,
               public MessageBroadcaster,
               public std::enable_shared_from_this<NotificationListener>,
               public AsyncWorker<threadCount>
{
public:

  size_t STRESS_TEST_MAX_NUMBER_OF_FILES{1000};

  Logger(
    const std::string& newWorkerName,
    const std::vector<SharedSizeStringBuffer>& newBuffers,
    std::ostream& newErrorOut, std::mutex& newErrorOutLock,
    const std::string& newDestinationDirectory,
    bool stressTestNeeded
  ) :
    AsyncWorker<threadCount>{newWorkerName},
    buffers{newBuffers},
    destinationDirectory{newDestinationDirectory},
    previousTimeStamp{}, additionalNameSection{},
    errorOut{newErrorOut}, errorOutLock{newErrorOutLock},
    threadMetrics{},
    stressTesting{stressTestNeeded}
  {
    for (const auto& buffer : buffers)
    {
      if (nullptr == buffer)
      {
        throw(std::invalid_argument{"Logger source buffer not defined!"});
      }
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
    for (size_t threadIndex{0}; threadIndex < threadCount; ++threadIndex)
    {
      if (buffers[threadIndex].get() == sender)
      {
        ++this->notificationCounts[threadIndex];
        this->threadNotifiers[threadIndex].notify_one();
        break;
      }
    }

    #ifdef NDEBUG
    #else
      //std::cout << this->workerName << " reactNotification\n";
    #endif

  }

  void reactMessage(class MessageBroadcaster* sender, Message message) override
  {
    if (messageCode(message) < 1000) // non error message
    {
      switch(message)
      {
        case Message::NoMoreData :
        for (size_t threadIndex{0}; threadIndex < threadCount; ++threadIndex)
        {
          if (buffers[threadIndex].get() == sender)
          {
            this->noMoreData[threadIndex].store(true);
            break;
          }
        }


          #ifdef NDEBUG
          #else
            //std::cout << "\n                     " << this->workerName<< " NoMoreData received\n";
          #endif

          for (auto& notifier : this->threadNotifiers)
          {
            notifier.notify_one();
          }
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
        for (auto& notifier : this->threadNotifiers)
        {
          notifier.notify_one();
        }
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
    if (nullptr == buffers[threadIndex])
    {
      throw(std::invalid_argument{"Logger source buffer not defined!"});
    }

    auto nextBulkInfo{buffers[threadIndex]->getItem()};

    auto logFileName {buildFileName(nextBulkInfo.first, threadIndex)};

    writeToFile(logFileName, nextBulkInfo.second);

    ++additionalNameSection[threadIndex];

    /* Refresh metrics */
    ++threadMetrics[threadIndex]->totalBulkCount;
    threadMetrics[threadIndex]->totalCommandCount
        += std::count(nextBulkInfo.second.begin(),
                      nextBulkInfo.second.end(), ',') + 1;

    ++totalFilesProcessed;

    if (stressTesting == true
        && totalFilesProcessed.load() >= STRESS_TEST_MAX_NUMBER_OF_FILES)
    {
      throw std::bad_alloc();
    }

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
    for (auto& notifier : this->threadNotifiers)
    {
      notifier.notify_one();
    }

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

    auto totalNotifications {std::accumulate(
            this->notificationCounts.begin(),
            this->notificationCounts.end(), 0
          )};

    auto noDataAnymore{std::accumulate(
            this->noMoreData.begin(),
            this->noMoreData.end(), false
          )};

    if (true == noDataAnymore
        && totalNotifications == 0)
    {
      sendMessage(Message::AllDataLogged);
    }
    else
    {
      #ifdef NDEBUG
      #else
        //std::cout << "\n                     " << this->workerName << " unprocessed notifiacations: " << totalNotifications <<"\n";
      #endif
    }
  }

  std::string buildFileName(const std::size_t& timeStamp, const size_t& threadIndex)
  {
    std::string bulkFileName{
      destinationDirectory + std::to_string(timeStamp)
    };

    std::stringstream fileNameSuffix{};
    fileNameSuffix << ::getpid()<< "-" << this->stringThreadID[threadIndex]
                   << "_" << additionalNameSection[threadIndex];

    return (bulkFileName + "_" + fileNameSuffix.str() + ".log");
  }

  void writeToFile(const std::string& fileName, const std::string& fileContent)
  {
    std::ofstream newFile{fileName};
    newFile << fileContent;
  }


  const std::vector<SharedSizeStringBuffer>& buffers;

  std::string destinationDirectory;

  size_t previousTimeStamp;
  std::vector<size_t> additionalNameSection;

  std::ostream& errorOut;
  std::mutex& errorOutLock;

  SharedMultyMetrics threadMetrics;

  Message errorMessage{Message::SystemError};

  std::atomic<size_t> totalFilesProcessed{0};
  bool stressTesting;
};
