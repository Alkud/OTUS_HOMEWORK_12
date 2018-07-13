// input_reader.cpp in Otus homework#12 project

#include "input_reader.h"
#include <string>
#include <stdexcept>
#include <mutex>

InputReader::InputReader(const std::string& newWorkerName,
    const std::shared_ptr<InputBufferType>& newInputBuffer,
    const SharedStringBuffer& newOutputBuffer,
    std::ostream& newErrorOut
  , std::mutex& newErrorOutLock) :
  AsyncWorker<1>{newWorkerName},
  inputBuffer{newInputBuffer},
  outputBuffer{newOutputBuffer},
  errorOut{newErrorOut}, errorOutLock{newErrorOutLock},
  threadMetrics{std::make_shared<ThreadMetrics>("input reader")}
{
  if (nullptr == inputBuffer)
  {
    throw(std::invalid_argument{"Input reader source buffer not defined!"});
  }

  if (nullptr == outputBuffer)
  {
    throw(std::invalid_argument{"Input reader destination buffer not defined!"});
  }
}

InputReader::~InputReader()
{
  stop();
}

void InputReader::reactMessage(MessageBroadcaster* /*sender*/, Message message)
{
  if (messageCode(message) < 1000) // non error message
  {
    switch(message)
    {
    case Message::NoMoreData :
      noMoreData[0].store(true);

      #ifdef NDEBUG
      #else
        //std::cout << "\n                     " << this->workerName<< " NoMoreData received\n";
      #endif

      threadNotifiers[0].notify_one();
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

void InputReader::reactNotification(NotificationBroadcaster* sender)
{
  if (inputBuffer.get() == sender)
  {
    #ifdef NDEBUG
    #else
      //std::cout << this->workerName << " reactNotification\n";
    #endif

    ++notificationCounts[0];
    threadNotifiers[0].notify_one();
  }
}

const SharedMetrics InputReader::getMetrics()
{
  return threadMetrics;
}

bool InputReader::threadProcess(const size_t /*threadIndex*/)
{
  if (nullptr == inputBuffer)
  {
    errorMessage = Message::SourceNullptr;
    throw(std::invalid_argument{"Input reader source buffer not defined!"});
  }

  auto bufferReply {inputBuffer->getItem()};

  /* Refresh metrics */
  ++threadMetrics->totalReceptionCount;

  for (const auto& element : bufferReply)
  {
    /* Refresh metrics */
    ++threadMetrics->totalCharacterCount;

    tempBuffer << element;
    if ('\n' == element)
    {
      putNextLine();
    }
  }

  return true;
}

void InputReader::onThreadException(const std::exception& ex, const size_t threadIndex)
{
  {
    std::lock_guard<std::mutex> lockErrorOut{errorOutLock};
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

void InputReader::onTermination(const size_t /*threadIndex*/)
{
  #ifdef NDEBUG
  #else
    //std::cout << "\n                     " << this->workerName<< " all characters received\n";
  #endif

  if (true == noMoreData[0].load() &&
      notificationCounts[0].load() == 0)
  {
    sendMessage(Message::NoMoreData);
    sendMessage(Message::AllDataReceived);
  }
}

void InputReader::putNextLine()
{
  if (nullptr == outputBuffer)
  {
    errorMessage = Message::DestinationNullptr;
    throw(std::invalid_argument{"Input reader destination buffer not defined!"});
  }

  std::string nextString{};

  std::getline(tempBuffer, nextString);

  if (nextString.size() >
      static_cast<size_t>(InputReaderSettings::MaxInputStringSize))
  {
    std::lock_guard<std::mutex> lockErrorOut{errorOutLock};
    errorOut << "Maximum command length exceeded! String truncated";
    nextString = nextString.substr(0, static_cast<size_t>(
                                     InputReaderSettings::MaxInputStringSize));
  }

  /* Refresh metrics */
  ++threadMetrics->totalStringCount;

  outputBuffer->putItem(std::move(nextString));
}

WorkerState InputReader::getWorkerState()
{
  return state;
}
