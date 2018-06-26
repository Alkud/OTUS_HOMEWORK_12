// input_processor.h in Otus homework#7 project

#pragma once

#include <memory>
#include <chrono>
#include <ctime>
#include "smart_buffer_mt.h"
#include "thread_metrics.h"


class InputProcessor : public NotificationListener,
                       public MessageBroadcaster,
                       public MessageListener,
                       public std::enable_shared_from_this<InputProcessor>,
                       public AsyncWorker<1>
{
public:

  using InputBufferType = StringBuffer;
  using OutputBufferType = SizeStringBuffer;

  InputProcessor(const std::string& newWorkerName,
                 const size_t newBulkSize,
                 const char newBulkOpenDelimiter,
                 const char newBulkCloseDelimiter,
                 const SharedStringBuffer& newInputBuffer,
                 const SharedSizeStringBuffer& newOutputBuffer,
                 std::ostream& newErrorOut, std::mutex& newErrorOutLock);

  ~InputProcessor();

  void reactNotification(NotificationBroadcaster* sender) override;

  void reactMessage(MessageBroadcaster* sender, Message message) override;

  const SharedMetrics getMetrics();

  WorkerState getWorkerState();

private:

  bool threadProcess(const size_t threadIndex) override;
  void onThreadException(const std::exception& ex, const size_t threadIndex) override;
  void onTermination(const size_t threadIndex) override;

  void sendCurrentBulk();
  void startNewBulk();
  void closeCurrentBulk();
  void addCommandToBulk(std::string&& newCommand);

  const size_t bulkSize;
  const std::string bulkOpenDelimiter;
  const std::string bulkCloseDelimiter;

  SharedStringBuffer inputBuffer;
  SharedSizeStringBuffer outputBuffer;

  std::deque<std::string> tempBuffer;
  bool customBulkStarted;
  size_t nestingDepth;
  std::chrono::time_point<std::chrono::system_clock> bulkStartTime;

  std::ostream& errorOut;
  std::mutex& errorOutLock;

  SharedMetrics threadMetrics;

  Message errorMessage{Message::SystemError};
};
