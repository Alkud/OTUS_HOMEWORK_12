// simple_buffer.h in Otus homework#12 project

#pragma once

#include <deque>
#include <algorithm>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <thread>
#include <atomic>

#include "async_worker.h"
#include "broadcasters.h"
#include "weak_ptr_less.h"

using namespace std::chrono_literals;

const size_t MAX_BUFFER_SIZE = 1;

template<class T>
class SimpleBuffer : public NotificationBroadcaster,
                     public MessageListener,
                      public MessageBroadcaster,
                      public AsyncWorker<1>
{
public:

  using ListenerSet = std::set<std::weak_ptr<NotificationListener>, WeakPtrLess<NotificationListener>>;

  std::mutex dataLock{};

  SimpleBuffer() = delete;

  SimpleBuffer(const std::string& newWorkerName, std::ostream& newErrorOut, std::mutex& newErrorOutLock) :
    AsyncWorker<1>{newWorkerName},
    errorOut{newErrorOut}, errorOutLock{newErrorOutLock},
    dataReceived{true}
  {
    data.clear();
  }

  ~SimpleBuffer()
  {
    stop();
  }

  /// Copy new element to the buffer
  void putItem(const T& newItem)
  {
    std::lock_guard<std::mutex> lockData{dataLock};

    /* don't accept data if NoMoreData message received! */
    if (noMoreData[0].load() == true)
    {
      return;
    }

//    while (data.size() >= MAX_BUFFER_SIZE)
//    {
//      overloadNotifier.wait_for(lockData, 10ms, [this]()
//      {
//        return data.size() == 0;
//      });
//    }

    data.push_back(newItem);
    dataReceived.store(false);
    ++notificationCounts[0];
    threadNotifiers[0].notify_one();
  }

  /// Move new element to the buffer
  void putItem(T&& newItem)
  {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::unique_lock<std::mutex> lockData{dataLock};


//    std::cout << "\n                    "
//              << workerName << "data.size()=" << data.size() << "\n";

    /* don't accept data if NoMoreData message received! */
    if (noMoreData[0].load() == true)
    {
      return;
    }

//    while (data.size() >= MAX_BUFFER_SIZE)
//    {
//      overloadNotifier.wait_for(lockData, 10ms, [this]()
//      {
//        return data.size() == 0;
//      });
//    }

    data.push_back(std::move(newItem));
    dataReceived.store(false);
    ++notificationCounts[0];
    threadNotifiers[0].notify_one();

    lockData.unlock();

    auto endTime = std::chrono::high_resolution_clock::now();

    auto waitTime {std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()};
    if (waitTime > 100)
    {
      //std::cout << "                        putItem time : " << waitTime << "\n";
    }
  }

  /// Each recipient starts looking from the first element in the queue.
  /// When an element that wasn't received yet by this recipient is found,
  /// the recipient gets the value of this element and updates pecipient list
  /// for this element.
  T getItem()
  {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::unique_lock<std::mutex> lockData{dataLock};

    if (data.empty() == true)
    {
      lockData.unlock();
      shouldExit.store(true);
      threadNotifiers[0].notify_one();
      errorMessage = Message::BufferEmpty;
      throw std::out_of_range{"Buffer is empty!"};
    }

    auto result {data.front()};

    data.pop_front();

//    ++shrinkCounter;

//    if (shrinkCounter.load() == 1000)
//    {
//      data.shrink_to_fit();
//      shrinkCounter.store(0);
//    }

    if (data.empty() == true)
    {
      //data.shrink_to_fit();
      overloadNotifier.notify_all();
    }

    if (true == data.empty() && true == noMoreData[0].load())
    {      
      #ifdef NDEBUG
      #else
        //std::cout << "\n                    " << workerName<< " all data received\n";
      #endif

      dataReceived.store(true);
      threadNotifiers[0].notify_one();
    }

    lockData.unlock();

    auto endTime = std::chrono::high_resolution_clock::now();

    auto waitTime {std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()};
    if (waitTime > 100)
    {
      //std::cout << "                        getItem time : " << waitTime << "\n";
    }

    return result;
  }

  /// Get elements count in the queue
  size_t dataSize()
  {
    std::unique_lock<std::mutex> lockData{dataLock};
    auto result{ data.size()};
    lockData.unlock();

    return result;
  }

  /// Clear data
  void clear()
  {
    std::unique_lock<std::mutex> lockData{dataLock};
    data.clear();
    lockData.unlock();

    notificationCounts[0].store(0);
  }

  void reactMessage(MessageBroadcaster* /*sender*/, Message message) override
  {
    if (messageCode(message) < 1000) // non error message
    {
      switch(message)
      {
      case Message::NoMoreData :
      {
        std::lock_guard<std::mutex> lockData{dataLock};
        noMoreData[0].store(true);

        #ifdef NDEBUG
        #else
          //std::cout << "\n                     " << this->workerName<< " NoMoreData received\n";
        #endif

        threadNotifiers[0].notify_one();
      }
        break;

      default:
        break;
      }
    }
    else                             // error message
    {
      if (shouldExit.load() != true)
      {
        shouldExit.store(true);
        sendMessage(message);
      }
    }
  }

private:

  bool threadProcess(const size_t /*threadIndex*/) override
  {
    notify();

//    std::mutex dummyMutex{};
//    std::unique_lock<std::mutex> dummyLock{dummyMutex};
//    if (data.size() >= MAX_BUFFER_SIZE)
//    {
//      while (data.size() > 0)
//      {
//        overloadNotifier.wait_for(dummyLock, 10ms, [this]()
//        {
//          return data.size() == 0;
//        });
//      }

//    }
  }

  void onThreadException(const std::exception& ex, const size_t threadIndex) override
  {
    {
      std::lock_guard<std::mutex> lockErrorOutm{errorOutLock};
      errorOut << this->workerName << " thread #" << threadIndex << " stopped. Reason: " << ex.what() << std::endl;
    }

    if (ex.what() == std::string{"Buffer is empty!"})
    {
      errorMessage = Message::BufferEmpty;
    }

    threadFinished[threadIndex].store(true);
    shouldExit.store(true);
    threadNotifiers[0].notify_one();

    sendMessage(errorMessage);
  }

  void onTermination(const size_t /*threadIndex*/) override
  {
    if (noMoreData[0].load() == true
        && dataSize() == 0)
    {
      dataReceived.store(true);
    }

    while (dataReceived.load() != true)
    {
      #ifdef NDEBUG
      #else
      //std::cout << "\n                    "
      //          << workerName << " dataReceived=" << dataReceived.load()
      //          << "data.size()=" << data.size()
      //          << "notificationCount=" << notificationCounts[0].load() << "\n";
      if (workerName.find("logger buffer") != std::string::npos)
      {
        std::cout << "\r PLAEASE WAIT! Writing files. Remain: " << data.size() << "                 \r";
      }
      #endif

      std::unique_lock<std::mutex> lockNotifier{notifierLocks[0]};
      threadNotifiers[0].wait_for(lockNotifier, std::chrono::milliseconds{100}, [this]()
      {
        return dataReceived.load() == true;
      });
      lockNotifier.unlock();
    }

    sendMessage(Message::NoMoreData);
  }


  std::ostream& errorOut;
  std::mutex& errorOutLock;

  std::deque<T> data;

  std::atomic_bool dataReceived;

  Message errorMessage{Message::SystemError};

  std::atomic<size_t> shrinkCounter{};

  std::condition_variable overloadNotifier{};
};


using StringBuffer = SimpleBuffer<std::string>;
using SharedStringBuffer = std::shared_ptr<StringBuffer>;
using SizeStringBuffer = SimpleBuffer<std::pair<size_t, std::string>>;
using SharedSizeStringBuffer = std::shared_ptr<SizeStringBuffer>;
