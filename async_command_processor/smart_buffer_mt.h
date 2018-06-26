// smart_buffer.h in Otus homework#10 project

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

template<class T>
class SmartBuffer : public NotificationBroadcaster,
                    public MessageListener,
                    public MessageBroadcaster,
                    public AsyncWorker<1>
{
public:

  using ListenerSet = std::set<std::weak_ptr<NotificationListener>, WeakPtrLess<NotificationListener>>;

  std::mutex dataLock;

  SmartBuffer() = delete;

  SmartBuffer(const std::string& newWorkerName, std::ostream& newErrorOut, std::mutex& newErrorOutLock) :
    AsyncWorker<1>{newWorkerName},
    errorOut{newErrorOut}, errorOutLock{newErrorOutLock},
    dataReceived{true}
  {
    data.clear();
  }

  ~SmartBuffer()
  {
    stop();
  }

  /// Each element in the buffer has the list of recipients.
  /// When a recepient gets an element, it is added to this list.
  /// When all recipients have received this element, we can remove
  /// it from the buffer.
  struct Record
  {
    Record(T newValue) :
      value{newValue} {}

    Record(T newValue, const ListenerSet& newRecipients) :
      value{newValue}, recipients{newRecipients} {}

    T value{};
    ListenerSet recipients{};
  };

  /// Copy new element to the buffer
  void putItem(const T& newItem)
  {
    std::lock_guard<std::mutex> lockData{dataLock};

    /* don't accept data if NoMoreData message received! */
    if (noMoreData.load() == true)
    {
      return;
    }

    data.emplace_back(newItem, notificationListeners);
    dataReceived.store(false);
    ++notificationCount;
    threadNotifier.notify_one();
  }

  /// Move new element to the buffer
  void putItem(T&& newItem)
  {
    std::lock_guard<std::mutex> lockData{dataLock};

    /* don't accept data if NoMoreData message received! */
    if (noMoreData.load() == true)
    {
      return;
    }

    data.emplace_back(std::move(newItem), notificationListeners);
    dataReceived.store(false);
    ++notificationCount;
    threadNotifier.notify_one();
  }

  /// Each recipient starts looking from the first element in the queue.
  /// When an element that wasn't received yet by this recipient is found,
  /// the recipient gets the value of this element and updates pecipient list
  /// for this element.
  std::pair<bool, T> getItem(const std::shared_ptr<NotificationListener> recipient = nullptr)
  {
    std::unique_lock<std::mutex> lockData{dataLock};

    std::weak_ptr<NotificationListener> weakRecipient{recipient};

    if (data.empty() == true)
    {
      lockData.unlock();
      shouldExit.store(true);
      threadNotifier.notify_all();
      errorMessage = Message::BufferEmpty;
      throw std::out_of_range{"Buffer is empty!"};
    }

    if (nullptr == recipient)
    {
      lockData.unlock();
      return std::make_pair(false, data.front().value);
    }

    auto iter {data.begin()};
    while(iter != data.end()
          && (iter->recipients.find(weakRecipient) == iter->recipients.end()))
    {
      ++iter;
    }

    if (iter == data.end()
        && (notificationListeners.find(weakRecipient) != notificationListeners.end()))
    {
      lockData.unlock();
      return std::make_pair(false, data.front().value);
    }

    auto result {std::make_pair(true, iter->value)};

    iter->recipients.erase(weakRecipient);

    if (iter->recipients.empty() == true)
    {
      data.erase(iter);

      #ifdef NDEBUG
      #else
        //std::cout << "\n                    " << workerName<< " check if this is the last data item\n";
      #endif

      if (true == data.empty() && true == noMoreData.load())
      {
        #ifdef NDEBUG
        #else
          //std::cout << "\n                    " << workerName<< " all data received\n";
        #endif

        dataReceived.store(true);
        threadNotifier.notify_all();
      }
    }

    lockData.unlock();
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

    notificationCount.store(0);
  }

  void reactMessage(MessageBroadcaster* sender, Message message) override
  {
    if (messageCode(message) < 1000) // non error message
    {
      switch(message)
      {
      case Message::NoMoreData :
        noMoreData.store(true);

        #ifdef NDEBUG
        #else
          //std::cout << "\n                     " << this->workerName<< " NoMoreData received\n";
        #endif

        threadNotifier.notify_all();
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

  bool threadProcess(const size_t threadIndex) override
  {
    notify();
  }

  void onThreadException(const std::exception& ex, const size_t threadIndex) override
  {
    {
      std::lock_guard<std::mutex> lockErrorOutm{errorOutLock};
      errorOut << this->workerName << " thread #" << threadIndex << " stopped. Reason: " << ex.what() << std::endl;
    }

    if (ex.what() == "Buffer is empty!")
    {
      errorMessage = Message::BufferEmpty;
    }

    threadFinished[threadIndex].store(true);
    shouldExit.store(true);
    threadNotifier.notify_all();

    sendMessage(errorMessage);
  }

  void onTermination(const size_t threadIndex) override
  {
    if (noMoreData.load() == true
        && dataSize() == 0)
    {
      dataReceived.store(true);
    }

    while (dataReceived.load() != true)
    {
      #ifdef NDEBUG
        #else
//      std::cout << "\n                    "
//                << workerName << " dataReceived=" << dataReceived.load()
//                << "data.size()=" << data.size()
//                << "notificationCount=" << notificationCount.load() << "\n";
      #endif

      std::unique_lock<std::mutex> lockNotifier{notifierLock};
      threadNotifier.wait_for(lockNotifier, std::chrono::milliseconds{1000}, [this]()
      {        
        return dataReceived.load() == true;
      });
      lockNotifier.unlock();
    }

    sendMessage(Message::NoMoreData);
  }


  std::ostream& errorOut;
  std::mutex& errorOutLock;

  std::deque<Record> data;

  std::atomic_bool dataReceived;

  Message errorMessage{Message::SystemError};
};


using StringBuffer = SmartBuffer<std::string>;
using SharedStringBuffer = std::shared_ptr<StringBuffer>;
using SizeStringBuffer = SmartBuffer<std::pair<size_t, std::string>>;
using SharedSizeStringBuffer = std::shared_ptr<SizeStringBuffer>;
