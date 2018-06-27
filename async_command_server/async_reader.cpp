#include "async_reader.h"

AsyncReader::AsyncReader(AsyncReader::SharedSocket newSocket,
  AsyncReader::SharedProcessor newProcessor,
  std::ostream& newErrorStream,
  std::mutex& newOutputLock
):
  socket{newSocket}, processor{newProcessor},
  readBuffer{std::make_unique<char[]>(READ_BUFFER_SIZE)},
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
  socket->async_read_some(asio::buffer(readBuffer.get(), READ_BUFFER_SIZE),
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
  if (processor != nullptr)
  {
    processor->receiveData(readBuffer.get(), bytes_transferred);
  }
}
