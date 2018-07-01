#include "async_reader.h"
#include <chrono>

using namespace std::chrono_literals;

AsyncReader::AsyncReader(AsyncReader::SharedSocket newSocket,
  AsyncReader::SharedProcessor newProcessor,
  asio::ip::tcp::acceptor& newAcceptor,
  std::atomic<size_t>& newReaderCounter,
  std::condition_variable& newTerminationNotifier,
  std::mutex& newTerminationLock,
  std::ostream& newErrorStream,
  std::mutex& newOutputLock
):
  socket{newSocket}, processor{newProcessor},
  readBuffer{},
  bulkBuffer{}, bulkOpen{false},
  acceptor{newAcceptor}, readerCounter{newReaderCounter},
  terminationNotifier{newTerminationNotifier},
  terminationLock{newTerminationLock},
  errorStream{newErrorStream}, outputLock{newOutputLock},
  sharedThis{}
{
  ++readerCounter;
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
  if (socket != nullptr)
  {
    socket->close();
  }
}

void AsyncReader::doRead()
{
  socket->async_read_some(asio::buffer(readBuffer),
  [this](const system::error_code& error, std::size_t bytes_transferred)
  {
    if (error != 0)
    {
      std::lock_guard<std::mutex> lockOutput{outputLock};

      errorStream << "async_read error: "
                  << error.message()
                  << ". Error code: " << error.value() << '\n';

      return;
    }

    onReading(bytes_transferred);

    if (READ_BUFFER_SIZE == bytes_transferred)
    {
      doRead();
    }
    else
    {
      socket->close();
      --readerCounter;
      terminationNotifier.notify_all();

      std::this_thread::sleep_for(200ms);

      std::unique_lock<std::mutex> lockTermination{terminationLock};
      terminationNotifier.wait_for(lockTermination, 1s, [this]()
      {
        return readerCounter.load() == 0;
      });
      lockTermination.unlock();

      if (readerCounter.load() == 0)
      {
        acceptor.cancel();
      }

      sharedThis.reset();
    }
  });
}

void AsyncReader::onReading(std::size_t bytes_transferred)
{
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

    if (characterBuffer.good() != true)
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
  if ("{" == inputString)
  {
    bulkBuffer.push_back('{');
    bulkBuffer.push_back('\n');
    bulkOpen = true;
  }
  else if ("}" == inputString)
  {
    if (true == bulkOpen)
    {
      bulkBuffer.push_back('}');
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
