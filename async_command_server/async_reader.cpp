// async_reader.cpp in Otus homework#12 project

#include "async_reader.h"
#include <chrono>
#include <csignal>

using namespace std::chrono_literals;

AsyncReader::AsyncReader(AsyncReader::SharedSocket newSocket,
  AsyncReader::SharedProcessor newProcessor, const char newOpenDelimiter, const char newCloseDelimiter,
  asio::ip::tcp::acceptor& newAcceptor,
  std::atomic<size_t>& newReaderCounter,
  std::condition_variable& newTerminationNotifier,
  std::mutex& newTerminationLock,
  std::ostream& newErrorStream,
  std::mutex& newOutputLock,
  std::atomic<bool>& stopFlag
):
  socket{newSocket}, processor{newProcessor},
  openDelimiter{newOpenDelimiter}, closeDelimiter{newCloseDelimiter},
  readBuffer{},
  bulkBuffer{}, bulkOpen{false},
  acceptor{newAcceptor}, readerCounter{newReaderCounter},
  terminationNotifier{newTerminationNotifier},
  terminationLock{newTerminationLock},
  errorStream{newErrorStream}, outputLock{newOutputLock},
  sharedThis{},
  shouldExit{stopFlag},

  stopped{false}, controller{}, controllerNotifier{}
{
  ++readerCounter;
}

AsyncReader::~AsyncReader()
{
  #ifdef NDEBUG
  #else
    //std::cout << "-- reader destructor\n";
  #endif

  if (controller.joinable())
  {
    controller.detach();
  }
}

void AsyncReader::start()
{
  if (nullptr == sharedThis)
  {
    sharedThis = shared_from_this();
  }

  controller = std::thread{[this]()
  {
      while (shouldExit.load() != true && stopped.load() != true)
      {
        std::mutex dummyMutex{};
        std::unique_lock<std::mutex> dummyLock{dummyMutex};
        controllerNotifier.wait_for(dummyLock, 1s, [this]()
        {
          return shouldExit.load() == true || stopped.load() == true;
        });
      }

      if (shouldExit.load() == true
          && socket != nullptr
          && socket->is_open())
      {
        stop();
      }

      #ifdef NDEBUG
      #else
        //std::cout << "-- reader controller EXIT\n";
      #endif
  }};

  doRead();  
}

void AsyncReader::stop()
{
  #ifdef NDEBUG
  #else
    //std::cout << "-- reader stop\n";
  #endif

  stopped.store(true);
  controllerNotifier.notify_one();

  if (socket != nullptr)
  {    
    if (socket->is_open())
    {
      #ifdef NDEBUG
      #else
       //std::cout << "-- reader socket shutdown\n";
      #endif

      socket->shutdown(asio::ip::tcp::socket::shutdown_both);

      #ifdef NDEBUG
      #else
        //std::cout << "-- reader socket close\n";
      #endif      
    }

    if (readerCounter.load() != 0)
    {
      --readerCounter;
    }

    terminationNotifier.notify_all();

    sharedThis.reset();
  }
}

void AsyncReader::doRead()
{
  if (shouldExit.load() == true)
  {
    stop();
  }
  #ifdef NDEBUG
  #else
    //std::cout << "-- start doRead\n";
  #endif

  asio::async_read(*socket, asio::buffer(readBuffer),
  asio::transfer_at_least(1),
  [this](const system::error_code& error, std::size_t bytes_transferred)
  {
    if (shouldExit.load() != true && !error)
    {
      onReading(bytes_transferred);

      #ifdef NDEBUG
      #else
        //std::cout << "-- continue doRead\n";
      #endif

      doRead();
    }
    else
    {
      stop();
    }
  });
}

void AsyncReader::onReading(std::size_t bytes_transferred)
{
  #ifdef NDEBUG
  #else
    //std::cout << "-- start onReading\n";
  #endif

  if (processor == nullptr)
  {
    return;
  }

  for (size_t idx{0}; idx < bytes_transferred; ++idx)
  {
    characterBuffer << readBuffer[idx];
  }

  std::string tempString{};
  for (;;)
  {
    std::getline(characterBuffer, tempString);

    characterBuffer.peek();

    if (characterBuffer.good() != true) // last portion of data received
    {
      characterBuffer.clear();
      if (tempString.empty() != true)
      {
        if (readBuffer[bytes_transferred - 1] != '\n')
        {
          characterBuffer << tempString;
        }
        else
        {
          processInputString(tempString);
        }
      }
      else if ('\n' == readBuffer[0]) // process empty command
      {
        processInputString(tempString);
      }
      break;
    }

    processInputString(tempString);
  }
}

void AsyncReader::processInputString(std::string& inputString)
{
  if (inputString == std::string{openDelimiter})
  {
    bulkBuffer.push_back(openDelimiter);
    bulkBuffer.push_back('\n');
    bulkOpen = true;
  }
  else if (inputString == std::string{closeDelimiter})
  {
    if (true == bulkOpen)
    {
      bulkBuffer.push_back(closeDelimiter);
      bulkBuffer.push_back('\n');
      std::string tmpString{};
      std::copy(bulkBuffer.begin(), bulkBuffer.end(), std::back_inserter(tmpString));
      //processor->receiveData(bulkBuffer.data(), bulkBuffer.size());
      processor->receiveData(tmpString.c_str(), tmpString.size());
      bulkBuffer.clear();
      bulkOpen = false;
    }
    else
    {
      inputString.append("\n");
      processor->receiveData(inputString.c_str(), inputString.size());
    }
  }
  else
  {
    if (true == bulkOpen)
    {
      inputString.append("\n");
      for (const auto & ch : inputString)
      {
        bulkBuffer.push_back(ch);
      }
    }
    else
    {
      inputString.append("\n");
      processor->receiveData(inputString.c_str(), inputString.size());
    }
  }
}
