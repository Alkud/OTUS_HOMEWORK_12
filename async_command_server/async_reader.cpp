#include "async_reader.h"

AsyncReader::AsyncReader(AsyncReader::SharedSocket newSocket,
  AsyncReader::SharedProcessor newProcessor,
  std::ostream& newErrorStream,
  std::mutex& newOutputLock
):
  socket{newSocket}, processor{newProcessor},
  //readBuffer{std::make_unique<char[]>(READ_BUFFER_SIZE)},
  readBuffer{},
  bulkBuffer{}, bulkOpen{false},
  errorStream{newErrorStream}, outputLock{newOutputLock},
  sharedThis{}
{  
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

    if (!characterBuffer)
    {
      characterBuffer.clear();
      characterBuffer << tempString;
      break;
    }

    if ("{" == tempString)
    {
      bulkBuffer.push_back('{');
      bulkBuffer.push_back('\n');
      bulkOpen = true;
    }
    else if ("}" == tempString)
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
        tempString.append("\n");
        processor->receiveData(tempString.c_str(), tempString.size());
      }
    }
    else
    {
      if (true == bulkOpen)
      {
        tempString.append("\n");
        for (const auto & ch : tempString)
        {
          bulkBuffer.push_back(ch);
        }
      }
      else
      {
        tempString.append("\n");
        processor->receiveData(tempString.c_str(), tempString.size());
      }
    }
  }

}
