// publisher.h in Otus homework#11 project

#pragma once

#include <iostream>
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>
#include "listeners.h"
#include "smart_buffer_mt.h"
#include "thread_metrics.h"


class Publisher : public NotificationListener,
                  public MessageListener,
                  public MessageBroadcaster,
                  public std::enable_shared_from_this<NotificationListener>,
                  public AsyncWorker<1>
{
public:

  Publisher(const std::string& newWorkerName,
            const SharedSizeStringBuffer& newBuffer,
            std::ostream& newOutput, std::mutex& newOutpuLock,
            std::ostream& newErrorOut, std::mutex& newErrorOutLock);

  ~Publisher();

  void reactNotification(NotificationBroadcaster* sender) override;

  void reactMessage(MessageBroadcaster* sender, Message message) override;

  const SharedMetrics getMetrics();

private:

  bool threadProcess(const size_t threadIndex) override;

  void onThreadException(const std::exception& ex, const size_t threadIndex) override;

  void onTermination(const size_t threadIndex) override;

  SharedSizeStringBuffer buffer;
  std::ostream& output;
  std::mutex& outputLock;

  std::ostream& errorOut;
  std::mutex& errorOutLock;

  SharedMetrics threadMetrics;

  Message errorMessage{Message::SystemError};
};
