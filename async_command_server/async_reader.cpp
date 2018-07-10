// async_reader.cpp in Otus homework#12 project

#include "async_reader.h"
#include <chrono>

using namespace std::chrono_literals;

AsyncReader::AsyncReader(AsyncReader::SharedSocket newSocket,
  AsyncReader::SharedProcessor newProcessor, const char newOpenDelimiter, const char newCloseDelimiter,
  asio::ip::tcp::acceptor& newAcceptor,
  std::atomic<size_t>& newReaderCounter,
  std::condition_variable& newTerminationNotifier,
  std::mutex& newTerminationLock,
  std::ostream& newErrorStream,
  std::mutex& newOutputLock
):
  socket{newSocket}, processor{newProcessor},
  openDelimiter{newOpenDelimiter}, closeDelimiter{newCloseDelimiter},
  readBuffer{},
  bulkBuffer{}, bulkOpen{false},
  acceptor{newAcceptor}, readerCounter{newReaderCounter},
  terminationNotifier{newTerminationNotifier},
  terminationLock{newTerminationLock},
  errorStream{newErrorStream}, outputLock{newOutputLock},
  sharedThis{}
{
  #ifdef NDEBUG
  #else
  std::cout << "-- reader constructor\n";
  #endif

  ++readerCounter;
}

AsyncReader::~AsyncReader()
{
  #ifdef NDEBUG
  #else
   std::cout << "-- reader destructor\n";
  #endif

  if (readerCounter.load() != 0)
  {
    --readerCounter;
  }
}

void AsyncReader::start()
{
  if (nullptr == sharedThis)
  {
    sharedThis = shared_from_this();
  }

  doRead();
}

void AsyncReader::stop()
{
  #ifdef NDEBUG
  #else
    //std::cout << "-- reader stop\n";
  #endif

  if (socket != nullptr)
  {    
    if (socket->is_open())
    {
      socket->shutdown(asio::ip::tcp::socket::shutdown_both);
      socket->close();
    }    

    terminationNotifier.notify_all();

    #ifdef NDEBUG
    #else
      std::cout << "-- reader use count:" << sharedThis.use_count() << "\n";
    #endif

    sharedThis.reset();
  }
}

void AsyncReader::doRead()
{
  #ifdef NDEBUG
  #else
    //std::cout << "-- start doRead\n";
  #endif

  asio::async_read(*socket, asio::buffer(readBuffer),
  asio::transfer_at_least(1),
  [this](const system::error_code& error, std::size_t bytes_transferred)
  {
    if (!error)
    {
      onReading(bytes_transferred);

      #ifdef NDEBUG
      #else
        //std::cout << "-- continue doRead\n";
      #endif

      doRead();
    }
    else if (error != asio::error::eof)
    {
      std::lock_guard<std::mutex> lockOutput{outputLock};

      errorStream << "async_read error: "
                  << error.message()
                  << ". Error code: " << error.value() << '\n';
    }
    else
    {
      #ifdef NDEBUG
      #else
        //std::cout << "-- doRead Eof\n";
      #endif

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
      processor->receiveData(bulkBuffer.data(), bulkBuffer.size());
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
