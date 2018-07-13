// async_worker.h in Otus homework#12 project

#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <array>
#include <future>
#include <cassert>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>

enum class WorkerState
{
  NotStarted = 0,
  Started = 1,
  Finished = 2
};

template<size_t workingThreadCount = 1u>
class AsyncWorker
{
public:
  AsyncWorker() = delete;

  AsyncWorker(const std::string& newWorkerName) :
    shouldExit{false}, noMoreData{}, isStopped{true}, notificationCounts{},
    threadNotifiers{}, notifierLocks{},
    threadFinished{}, terminationLock{},
    workerName{newWorkerName}, state{WorkerState::NotStarted}
  {    
    for (auto& count : notificationCounts)
    {
      count.store(0);
    }

    for (auto& flag : noMoreData)
    {
      flag.store(false);
    }

    futureResults.reserve(workingThreadCount);
    threadID.resize(workingThreadCount, std::thread::id{});
    stringThreadID.resize(workingThreadCount, std::string{});
    for (auto& item : threadFinished)
    {
      item.store(false);
    }
  }

  virtual ~AsyncWorker()
  {
    stop();
    #ifdef NDEBUG
    #else
      //std::cout << "\n                    " << workerName << " destructor, shouldExit = " << shouldExit << "\n";
    #endif

    assert(isStopped == true);
  }

  virtual void start()
  {
    startWorkingThreads();
  }

  virtual bool startAndWait()
  {
    startWorkingThreads();
    if (futureResults.empty() != true)
    {
      return waitForThreadTermination();
    }
    else
    {
      return false;
    }
  }

  void stop()
  {
    if (true == isStopped)
    {
      return;
    }

    #ifdef NDEBUG
    #else
      //std::cout << "\n                    " << workerName << " trying to stop\n";
    #endif

    shouldExit.store(true);
    for (auto& notifier : threadNotifiers)
    {
      notifier.notify_one();
    }


    for (auto& result : futureResults)
    {
      while (result.valid()
             && result.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
      {
        shouldExit.store(true);
        for (auto& notifier : threadNotifiers)
        {
          notifier.notify_one();
        }
        result.wait_for(std::chrono::milliseconds(100));
      }
    }

    if (state.load() != WorkerState::Finished)
    {
      state.store(WorkerState::Finished);
    }

    isStopped = true;

    futureResults.clear();
  }

  WorkerState getWorkerState()
  {
    return state.load();
  }

protected:

  static std::mt19937 idGenerator;

  std::hash<std::thread::id> threadIdHasher{};
  std::hash<std::string> stringHasher{};

  void startWorkingThreads()
  {
    if (futureResults.empty() != true)
    {
      return;
    }

    /* start working threads */
    for (size_t threadIndex{0}; threadIndex < workingThreadCount; ++threadIndex)
    {
      futureResults.push_back(
        std::async(
          std::launch::async,
          &AsyncWorker<workingThreadCount>::run,
          this, threadIndex
        )
      );
    }
    isStopped = false;
    state.store(WorkerState::Started);
  }

  bool waitForThreadTermination()
  {
    /* wait for working threads results */
    bool workSuccess{true};
    for (auto& result : futureResults)
    {
      workSuccess = workSuccess && result.get();
    }

    return workSuccess;
  }

  virtual void onThreadStart(const size_t /*threadIndex*/)
  {}

  virtual bool run(const size_t threadIndex)
  {
    onThreadStart(threadIndex);

    try
    {
      /* get unique thread ID */
      threadID[threadIndex] = std::this_thread::get_id();
      std::stringstream idStream{};
      idStream << threadID[threadIndex] << "-" << std::setw(12) << std::setfill('0') << idGenerator();
      stringThreadID[threadIndex] = idStream.str();

      /* main data processing loop */
      while(shouldExit.load() != true
            && (noMoreData[threadIndex].load() != true || notificationCounts[threadIndex].load() > 0))
      {
        std::unique_lock<std::mutex> lockNotifier{notifierLocks[threadIndex]};

        if (notificationCounts[threadIndex].load() > 0)
        {
          #ifdef NDEBUG
          #else
            //std::cout << this->workerName << " decrement notificationCount\n";
          #endif

          --notificationCounts[threadIndex];
          lockNotifier.unlock();
          threadProcess(threadIndex);

          #ifdef NDEBUG
          #else
            //std::cout << this->workerName << " threadProcess success\n";
          #endif
        }
        else
        {
          if (shouldExit.load() != true && noMoreData[threadIndex].load() != true)
          {
            #ifdef NDEBUG
            #else
//              std::cout << "\n                     " << this->workerName
//                        << " waiting. shouldExit="<< shouldExit
//                        << ", noMoreData=" << noMoreData
//                        << "notificationCount=" << notificationCount.load() << "\n";
            #endif

            threadNotifiers[threadIndex].wait_for(lockNotifier, std::chrono::milliseconds(10), [this, &threadIndex]()
            {
              return this->noMoreData[threadIndex].load()
                      || this->notificationCounts[threadIndex].load() > 0
                      || this->shouldExit.load();
            });

            lockNotifier.unlock();
          }
        }
      }

      /*check if this thread is the only active one */
      std::unique_lock<std::mutex> lockTermination{terminationLock};

      size_t activeThreadCount{};
      for (size_t idx{0}; idx < workingThreadCount; ++idx)
      {
        if (idx != threadIndex
            && threadFinished[idx].load() != true)
        {
          ++activeThreadCount;
        }
      }

      #ifdef NDEBUG
      #else
        //std::cout << "\n                     " << this->workerName<< " activeThreadCount=" << activeThreadCount << "\n";
      #endif

      threadFinished[threadIndex].store(true);

      lockTermination.unlock();

      if (0 == activeThreadCount)
      {
        #ifdef NDEBUG
        #else
          //std::cout << "\n                     " << this->workerName<< " finishing\n";
        #endif

        if (shouldExit.load() != true)
        {
          onTermination(threadIndex);
        }
        state.store(WorkerState::Finished);
      }

      #ifdef NDEBUG
      #else
        //std::cout << "\n                     " << this->workerName<< " finished\n";
      #endif

      return true;
    }
    catch (const std::exception& ex)
    {
      onThreadException(ex, threadIndex);
      state.store(WorkerState::Finished);
      return false;
    }
  }

  virtual bool threadProcess(const size_t threadIndex) = 0;

  virtual void onThreadException(const std::exception& ex, const size_t threadIndex) = 0;

  virtual void onTermination(const size_t threadIndex) = 0;

  std::vector<std::future<bool>> futureResults{};
  std::vector<std::thread::id> threadID{};
  std::vector<std::string> stringThreadID{};
  std::atomic<bool> shouldExit;

  std::array<std::atomic<bool>, workingThreadCount> noMoreData;

  bool isStopped;

  std::array<std::atomic<size_t> , workingThreadCount> notificationCounts;
  std::array<std::condition_variable, workingThreadCount> threadNotifiers;
  std::array<std::mutex, workingThreadCount> notifierLocks;

  std::array<std::atomic<bool>, workingThreadCount> threadFinished;
  std::mutex terminationLock;

  const std::string workerName;

  std::atomic<WorkerState> state;
};

template<size_t workingThreadCount>
std::mt19937
AsyncWorker<workingThreadCount>::idGenerator{workingThreadCount};
