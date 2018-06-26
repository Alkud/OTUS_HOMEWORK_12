// input_processor.cpp in Otus homework#7 project

#include "input_processor.h"

InputProcessor::InputProcessor(const std::string& newWorkerName, const size_t newBulkSize, const char newBulkOpenDelimiter, const char newBulkCloseDelimiter,
    const SharedStringBuffer& newInputBuffer,
    const SharedSizeStringBuffer& newOutputBuffer,
    std::ostream& newErrorOut
  , std::mutex& newErrorOutLock) :
  AsyncWorker<1>{newWorkerName},
  bulkSize{newBulkSize},
  bulkOpenDelimiter{newBulkOpenDelimiter},
  bulkCloseDelimiter{newBulkCloseDelimiter},
  inputBuffer{newInputBuffer},
  outputBuffer{newOutputBuffer},
  customBulkStarted{false},
  nestingDepth{0},
  errorOut{newErrorOut}, errorOutLock{newErrorOutLock},
  threadMetrics{std::make_shared<ThreadMetrics>("input processor")}
{
  if (nullptr == inputBuffer)
  {
    throw(std::invalid_argument{"Input processor source buffer not defined!"});
  }

  if (nullptr == outputBuffer)
  {
    throw(std::invalid_argument{"Input processor destination buffer not defined!"});
  }
}

InputProcessor::~InputProcessor()
{
  stop();
}

void InputProcessor::reactNotification(NotificationBroadcaster* sender)
{
  if (inputBuffer.get() == sender)
  {
    #ifdef NDEBUG
    #else
      //std::cout << this->workerName << " reactNotification\n";
    #endif

    ++notificationCount;
    threadNotifier.notify_one();
  }
}

void InputProcessor::reactMessage(MessageBroadcaster* sender, Message message)
{
  if (messageCode(message) < 1000) // non error message
  {
    switch(message)
    {
    case Message::NoMoreData :
      #ifdef NDEBUG
      #else
        //std::cout << "\n                     " << this->workerName<< " NoMoreData received\n";
      #endif

      noMoreData.store(true);
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

const SharedMetrics InputProcessor::getMetrics()
{
  return threadMetrics;
}

WorkerState InputProcessor::getWorkerState()
{
  return state;
}

bool InputProcessor::threadProcess(const size_t threadIndex)
{
  if (nullptr == inputBuffer)
  {
    errorMessage = Message::SourceNullptr;
    throw(std::invalid_argument{"Input processor source buffer not defined!"});
  }

  auto bufferReply {inputBuffer->getItem(shared_from_this())};

  if (false == bufferReply.first)
   {
     return false;
   }

  auto nextCommand{bufferReply.second};

  /* Refresh metrics */
  ++threadMetrics->totalStringCount;

  if (bulkOpenDelimiter == nextCommand)          // bulk open command received
  {
    /* if a custom bulk isn't started,
     * send accumulated commands to the output buffer,
     * then start a new custom bulk */
    if (customBulkStarted == false)
    {
      startNewBulk();
    }

    ++nestingDepth;
  }
  else if (bulkCloseDelimiter == nextCommand)    // bulk close command received
  {
    if (nestingDepth >= 1)
    {
       --nestingDepth;
    }

    /* if a custom bulk is started,
    * send accumulated commands to the output buffer,
    * then label custom bulk as closed */
    if (true == customBulkStarted &&
       0 == nestingDepth)
    {
      closeCurrentBulk();
    }
  }
  else                                           // any other command received
  {
   /* if no custom bulk started and temporary buffer is empty,
    * reset bulk start time */
   if (false == customBulkStarted &&
       true == tempBuffer.empty())
   {
     bulkStartTime = std::chrono::system_clock::now();
   }
   /* put new command to the temporary buffer */
   addCommandToBulk(std::move(nextCommand));
   /* if custom bulk isn't started,
    * and current bulk is complete,
    * send it to the output buffer */
   if (tempBuffer.size() == bulkSize &&
       customBulkStarted == false)
   {
     sendCurrentBulk();
   }
  }
}

void InputProcessor::onThreadException(const std::exception& ex, const size_t threadIndex)
{
  {
    std::lock_guard<std::mutex> lockErrorOut{errorOutLock};
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

void InputProcessor::onTermination(const size_t threadIndex)
{
  if (customBulkStarted != true)
  {
    closeCurrentBulk();
  }

  sendMessage(Message::NoMoreData);
}

void InputProcessor::sendCurrentBulk()
{
  if (tempBuffer.empty() == true)
  {
    return;
  }

  if (nullptr == outputBuffer)
  {
    errorMessage = Message::DestinationNullptr;
    throw(std::invalid_argument{"Input reader destination buffer not defined!"});
  }

  /* concatenate commands to a bulk */
  std::string newBulk{"bulk: "};
  auto iter{tempBuffer.begin()};
  newBulk += *iter;
  ++iter;
  for (; iter != tempBuffer.end(); ++iter)
  {
    newBulk += (", " + *iter);
  }

  /* convert bulk start time to integer ticks count */
  auto ticksCount{
    std::chrono::duration_cast<std::chrono::seconds>(
      bulkStartTime.time_since_epoch()
    ).count()
  };

  /* send the bulk to the output buffer */
  outputBuffer->putItem(std::make_pair(ticksCount, newBulk));

  /* Refresh metrics */
  threadMetrics->totalCommandCount += tempBuffer.size();
  ++threadMetrics->totalBulkCount;

  /*clear temporary buffer */
  tempBuffer.clear();
}

void InputProcessor::startNewBulk()
{
  sendCurrentBulk();
  bulkStartTime = std::chrono::system_clock::now();
  customBulkStarted = true;
}


void InputProcessor::closeCurrentBulk()
{
  sendCurrentBulk();
  customBulkStarted = false;
}

void InputProcessor::addCommandToBulk(std::string&& newCommand)
{
  tempBuffer.push_back(std::move(newCommand));
}


