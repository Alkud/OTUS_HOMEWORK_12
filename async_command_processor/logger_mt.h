// logger.h in Otus homework#11 project

#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>
#include <list>
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

  Logger(
    const std::string& newWorkerName,
    const std::vector<SharedSizeStringBuffer>& newBuffers,
    std::ostream& newErrorOut, std::mutex& newErrorOutLock,
    const std::string& newDestinationDirectory = ""
  ) :
    AsyncWorker<threadCount>{newWorkerName},
    buffers{newBuffers},
    destinationDirectory{newDestinationDirectory},
    outputFiles{},
    previousTimeStamp{}, additionalNameSection{},
    errorOut{newErrorOut}, errorOutLock{newErrorOutLock},
    threadMetrics{}
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

      outputFiles[threadIndex] = std::make_unique<std::ofstream>();
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

//    if (nextBulkInfo.first != previousTimeStamp)
//    {
//      additionalNameSection[threadIndex] = 1u;
//      previousTimeStamp = nextBulkInfo.first;
//    }

//    std::string bulkFileName{
//      destinationDirectory+ std::to_string(nextBulkInfo.first)
//    };

    //std::string processID{std::to_string(::getpid())};
    //std::string threadID{std::to_string(std::hash<std::string>(this->stringThreadID[threadIndex]))};


//    std::stringstream fileNameSuffix{};
//    fileNameSuffix << ::getpid()<< "-" << this->stringThreadID[threadIndex]
//                   << "_" << additionalNameSection[threadIndex];

    //auto suffixHash = std::to_string(this->stringHasher(fileNameSuffix.str()));
//    std::reverse(suffixHash.begin(), suffixHash.end());
//    std::random_device rd;
//    std::mt19937 g(rd());
    //std::shuffle(suffixHash.begin(), suffixHash.end(), this->idGenerator);

//    auto logFileName {bulkFileName + "_" + fileNameSuffix.str() + ".log"};

    auto logFileName {buildFileName(nextBulkInfo.first, threadIndex)};

    //auto delay{std::hash<std::string>{}(fileNameSuffix.str()) % 9};

    //std::this_thread::sleep_for(std::chrono::microseconds{25});

    std::stringstream fileNameSuffix{};
    fileNameSuffix << ::getpid()<< "-" << this->stringThreadID[threadIndex];

    auto startTime = std::chrono::high_resolution_clock::now();

    //diskAccessLock.lock();
    //std::fstream logFile{};
    //diskAccessLock.unlock();

    //logFile.open(logFileName, std::ios::out);
    //std::ios_base::sync_with_stdio(false);

    //auto fp{std::fopen(logFileName.c_str(), "w")};
    //std::fclose(fp);
    //std::this_thread::sleep_for(50ms);



    //outputFiles[threadIndex]->open(logFileName);

//    if(!logFile)
//    {
//      std::lock_guard<std::mutex> lockErrorOut{errorOutLock};
//      errorOut << "Cannot create log file " <<
//                  logFileName << " !" << std::endl;
//      throw(std::ios_base::failure{"Log file creation error!"});
//    }

    //std::this_thread::sleep_for(1ms);

    //for (const auto& smallDataChunk : nextBulkInfo.second)
    //{
    //  logFile << smallDataChunk;
      //std::this_thread::sleep_for(20ms);
    //}

    //logFile << '\n';

    //logFile << nextBulkInfo.second;

    //outputFiles[threadIndex]->close();

    writeToFile(threadIndex, logFileName/*fileNameSuffix.str()*/, nextBulkInfo.second);

    auto endTime = std::chrono::high_resolution_clock::now();

    auto waitTime {std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()};
    if (waitTime > 100)
    {
      //std::cout << "            write time : " << waitTime << "\n";
    }

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

  void writeToFile(const size_t threadIndex, const std::string& fileName, const std::string& fileContent)
  {

    auto newFile {std::make_shared<std::ofstream>()};
    newFile->open(fileName, std::ios::app);
    *newFile << fileContent;
    newFile->rdbuf()->sync();
    usedFiles.insert(newFile);

//    usedLock.lock();
//    if (usedFiles.size() > 500)
//    {
//      while (usedFiles.empty() != true)
//      {
//        usedFiles.pop_front();
//      }
//      usedFiles.push_back(newFile);
//    }
//    usedLock.unlock();

    //outputFiles[threadIndex].reset(new std::ofstream{});
    //outputFiles[threadIndex]->open(fileName, std::ios::app);
    //*outputFiles[threadIndex] << fileContent;
    //outputFiles[threadIndex]->write(fileContent.c_str(), fileContent.size());
    //outputFiles[threadIndex]->flush();
    //outputFiles[threadIndex]->close();

    //auto file {std::fopen(fileName.c_str(), "w")};
    //std::fwrite(fileContent.c_str(), 1, fileContent.size(), file);
    //std::fflush(file);
    //std::fclose(file);
  }


  const std::vector<SharedSizeStringBuffer>& buffers;

  std::string destinationDirectory;

  std::array<std::unique_ptr<std::ofstream>, threadCount> outputFiles;

  std::set<std::shared_ptr<std::ofstream>> usedFiles;
  std::mutex usedLock{};

  size_t previousTimeStamp;
  std::vector<size_t> additionalNameSection;

  std::ostream& errorOut;
  std::mutex& errorOutLock;

  SharedMultyMetrics threadMetrics;

  Message errorMessage{Message::SystemError};

  std::mutex diskAccessLock{};
};
