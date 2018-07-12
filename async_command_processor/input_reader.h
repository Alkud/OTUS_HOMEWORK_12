// input_reader.h in Otus homework#12 project

#pragma once

#include <iostream>
#include <memory>
#include <list>
#include <sstream>
#include "broadcasters.h"
#include "simple_buffer_mt.h"
#include "async_worker.h"
#include "thread_metrics.h"

enum class InputReaderSettings
{
  MaxInputStringSize = 80
};

class InputReader : public MessageBroadcaster,
                    public MessageListener,
                    public NotificationListener,
                    public std::enable_shared_from_this<NotificationListener>,
                    public AsyncWorker<1>
{
public:

  using EntryDataType = std::list<char>;
  using InputBufferType = SimpleBuffer<EntryDataType>;
  using OutputBufferType = StringBuffer;

  InputReader(const std::string& newWorkerName,
              const std::shared_ptr<InputBufferType>& newInputBuffer,
              const SharedStringBuffer& newOutputBuffer,
              std::ostream& newErrorOut, std::mutex& newErrorOutLock);

  ~InputReader() override;

  void reactMessage(MessageBroadcaster*, Message message) override;

  void reactNotification(NotificationBroadcaster* sender) override;

  const SharedMetrics getMetrics();

  WorkerState getWorkerState();

private:

  bool threadProcess(const size_t) override;
  void onThreadException(const std::exception& ex, const size_t threadIndex) override;
  void onTermination(const size_t threadIndex) override;

  void putNextLine();

  std::shared_ptr<InputBufferType> inputBuffer;
  std::shared_ptr<OutputBufferType> outputBuffer;

  std::ostream& errorOut;
  std::mutex& errorOutLock;

  std::stringstream tempBuffer;

  SharedMetrics threadMetrics;

  Message errorMessage{Message::SystemError};
};
