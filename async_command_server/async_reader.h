// async_reader.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include "../async_command_processor/async_command_processor.h"

using namespace boost;

constexpr size_t READ_BUFFER_SIZE = 64;

class AsyncReader : public std::enable_shared_from_this<AsyncReader>
{
public:

  using SharedSocket = std::shared_ptr<asio::ip::tcp::socket>;
  using SharedProcessor = std::shared_ptr<AsyncCommandProcessor<2>>;

  AsyncReader() = delete;

  AsyncReader(SharedSocket newSocket,
              SharedProcessor newProcessor,
              std::ostream& newErrorStream,
              std::mutex& newOutputLock);

  void start();

  void stop();

private:

  void doRead();

  void onReading(std::size_t bytes_transferred);

  SharedSocket socket;
  SharedProcessor processor;

  std::array<char, READ_BUFFER_SIZE> readBuffer;

  std::stringstream characterBuffer{};

  std::vector<char> bulkBuffer;
  bool bulkOpen;

  std::ostream& errorStream;
  std::mutex& outputLock;

  std::shared_ptr<AsyncReader> sharedThis;
};

using SharedAsyncReader = std::shared_ptr<AsyncReader>;
