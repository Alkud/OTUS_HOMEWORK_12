// async_worker.h in Otus homework#11 project

#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <array>
#include <future>
#include <cassert>

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
    shouldExit{false}, noMoreData{false}, isStopped{true}, notificationCount{0},
    threadFinished{}, workerName{newWorkerName}, state{WorkerState::NotStarted}
  {
    futureResults.reserve(workingThreadCount);
    for (auto& item : threadFinished)
    {
      item.store(false);
    }
  }

  virtual ~AsyncWorker()
  {
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
    threadNotifier.notify_all();

    for (auto& result : futureResults)
    {
      while (result.valid() && result.wait_for(std::chrono::milliseconds(0))
          != std::future_status::ready)
      {
        shouldExit.store(true);
        threadNotifier.notify_all();
        result.wait_for(std::chrono::milliseconds(500));
      }
    }

    if (state.load() != WorkerState::Finished)
    {
      state.store(WorkerState::Finished);
    }

    isStopped = true;
  }

  WorkerState getWorkerState()
  {
    return state.load();
  }

protected:

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

  virtual bool run(const size_t threadIndex)
  {
    try
    {
      while(shouldExit.load() != true
            && (noMoreData.load() != true || notificationCount.load() > 0))
      {
        std::unique_lock<std::mutex> lockNotifier{notifierLock};

        if (notificationCount.load() > 0)
        {
          #ifdef NDEBUG
          #else
            //std::cout << this->workerName << " decrement notificationCount\n";
          #endif

          --notificationCount;
          lockNotifier.unlock();
          threadProcess(threadIndex);

          #ifdef NDEBUG
          #else
            //std::cout << this->workerName << " threadProcess success\n";
          #endif
        }
        else
        {
          if (shouldExit.load() != true && noMoreData.load() != true)
          {
            #ifdef NDEBUG
            #else
//              std::cout << "\n                     " << this->workerName
//                        << " waiting. shouldExit="<< shouldExit
//                        << ", noMoreData=" << noMoreData
//                        << "notificationCount=" << notificationCount.load() << "\n";
            #endif

            threadNotifier.wait_for(lockNotifier, std::chrono::milliseconds(1000), [this]()
            {
              return this->noMoreData.load() || this->notificationCount.load() > 0 || this->shouldExit.load();
            });

            lockNotifier.unlock();
          }
        }
      }

      #ifdef NDEBUG
      #else
//         std::cout << "\n                     " << this->workerName
//                   << " terminating. shouldExit="<< shouldExit
//                   << ", noMoreData=" << noMoreData
//                   << "notificationCount=" << notificationCount.load() << "\n";
      #endif

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
  std::atomic_bool shouldExit;
  std::atomic_bool noMoreData;

  bool isStopped;

  std::atomic<size_t> notificationCount;
  std::condition_variable threadNotifier{};
  std::mutex notifierLock;

  std::array<std::atomic_bool, workingThreadCount> threadFinished;
  std::mutex terminationLock;

  const std::string workerName;

  std::atomic<WorkerState> state;
};
