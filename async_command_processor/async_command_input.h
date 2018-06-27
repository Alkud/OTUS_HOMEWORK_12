// async_command_input.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <list>
#include <thread>
#include <functional>
#include <cstdlib>
#include <condition_variable>
#include "smart_buffer_mt.h"
#include "input_reader.h"
#include "input_processor.h"

class AsyncCommandInput
{
public:
  AsyncCommandInput();

private:
  std::mutex& screenOutputLock;

  std::shared_ptr<InputReader::InputBufferType> externalBuffer;
  std::shared_ptr<InputProcessor::InputBufferType> inputBuffer;
  std::shared_ptr<InputProcessor::OutputBufferType> outputBuffer;
  std::shared_ptr<InputReader> inputReader;
  std::shared_ptr<InputProcessor> inputProcessor;

  std::mutex inputStreamLock{};

  std::atomic_bool dataReceived;
  std::atomic_bool shouldExit;

  std::condition_variable terminationNotifier{};
  std::mutex notifierLock;

  std::ostream& errorOut;
  SharedGlobalMetrics globalMetrics;

  Message errorMessage{Message::SystemError};
};
