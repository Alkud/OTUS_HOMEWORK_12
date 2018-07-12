// publisher.cpp in Otus homework#12 project

#include "publisher_mt.h"


Publisher::Publisher(const std::string& newWorkerName,
                     const SharedSizeStringBuffer& newBuffer,
                     std::ostream& newOutput, std::mutex& newOutpuLock,
                     std::ostream& newErrorOut, std::mutex& newErrorOutLock) :
  AsyncWorker<1>{newWorkerName},
  buffer{newBuffer}, output{newOutput}, outputLock{newOutpuLock},
  errorOut{newErrorOut}, errorOutLock{newErrorOutLock},
  threadMetrics{std::make_shared<ThreadMetrics>("publisher")}
{
  if (nullptr == buffer)
  {
    throw(std::invalid_argument{"Publisher source buffer not defined!"});
  }
}

Publisher::~Publisher()
{
  stop();
}

void Publisher::reactNotification(NotificationBroadcaster* sender)
{
  if (buffer.get() == sender)
  {
    #ifdef NDEBUG
    #else
      //std::cout << this->workerName << " reactNotification\n";
    #endif

    ++notificationCounts[0];
    threadNotifiers[0].notify_one();
  }
}

void Publisher::reactMessage(MessageBroadcaster* /*sender*/, Message message)
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

const SharedMetrics Publisher::getMetrics()
{
  return threadMetrics;
}

bool Publisher::threadProcess(const size_t /*threadIndex*/)
{
  if (nullptr == buffer)
  {
    throw(std::invalid_argument{"Logger source buffer not defined!"});
  }

  auto bufferReply{buffer->getItem()};

  auto nextBulkInfo{bufferReply.second};

  std::lock_guard<std::mutex> lockOutput{outputLock};
  //output << nextBulkInfo << '\n';

  /* Refresh metrics */
  ++threadMetrics->totalBulkCount;
    threadMetrics->totalCommandCount
      += static_cast<size_t>(std::count(nextBulkInfo.begin(),
                    nextBulkInfo.end(), ',') + 1);

  return true;
}

void Publisher::onThreadException(const std::exception& ex, const size_t threadIndex)
{
  {
    std::lock_guard<std::mutex> lockErrorOut{errorOutLock};
    errorOut << this->workerName << " thread #" << threadIndex << " stopped. Reason: " << ex.what() << std::endl;
  }

  threadFinished[threadIndex] = true;
  shouldExit.store(true);
  threadNotifiers[0].notify_one();

  if (ex.what() == std::string{"Buffer is empty!"})
  {
    errorMessage = Message::BufferEmpty;
  }

  sendMessage(errorMessage);
}

void Publisher::onTermination(const size_t /*threadIndex*/)
{
  #ifdef NDEBUG
  #else
    //std::cout << "\n                     " << this->workerName<< " AllDataPublished\n";
  #endif

  if (true == noMoreData.load() && notificationCounts[0].load() == 0)
  {
    sendMessage(Message::AllDataPublsihed);
  }
}
