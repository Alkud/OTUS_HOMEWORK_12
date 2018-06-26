// command_processor.h in Otus homework#11 project

#pragma once

#include <memory>
#include <mutex>
#include <list>
#include <thread>
#include <functional>
#include <condition_variable>
#include "input_reader.h"
#include "input_processor.h"
#include "smart_buffer_mt.h"
#include "publisher_mt.h"
#include "logger_mt.h"

template<size_t loggingThreadCount = 2u>
class CommandProcessorInstance :  public MessageBroadcaster,
                                  public MessageListener,
                                  public std::enable_shared_from_this<MessageListener>
{
public:

  CommandProcessorInstance
  (
    const size_t bulkSize = 3,
    const char bulkOpenDelimiter = '{',
    const char bulkCloseDelimiter = '}',
    std::ostream& outputStream = std::cout,
    std::ostream& errorStream = std::cerr,
    std::ostream& metricsStream = std::cout
  ) :
    screenOutputLock{},
    /* creating buffers */
    externalBuffer{std::make_shared<InputReader::InputBufferType>("character buffer", errorStream, screenOutputLock)},
    inputBuffer{std::make_shared<InputProcessor::InputBufferType>("command buffer", errorStream, screenOutputLock)},
    outputBuffer{std::make_shared<InputProcessor::OutputBufferType>("bulk buffer", errorStream, screenOutputLock)},

    /* creating logger */
    logger{
      std::make_shared<Logger<loggingThreadCount>>(
      "logger", outputBuffer, errorStream, screenOutputLock, ""
    )},

    /* creating publisher */
    publisher{
      std::make_shared<Publisher>(
      "publisher", outputBuffer, outputStream, screenOutputLock,
      errorStream, screenOutputLock
    )},

    /* creating command processor */
    inputProcessor{
      std::make_shared<InputProcessor>(
      "input processor ", bulkSize,
      bulkOpenDelimiter, bulkCloseDelimiter,
      inputBuffer, outputBuffer,
      errorStream, screenOutputLock
    )},

    /* creating command reader */
    inputReader{
      std::make_shared<InputReader>(
      "input reader",
      externalBuffer, inputBuffer,
      errorStream, screenOutputLock
    )},

    dataReceived{false}, dataPublished{false},
    dataLogged{false}, shouldExit{false},
    errorOut{errorStream}, metricsOut{metricsStream}, globalMetrics{}
  {
    /* connect broadcasters and listeners */
    this->addMessageListener(externalBuffer);

    externalBuffer->addNotificationListener(inputReader);
    externalBuffer->addMessageListener(inputReader);

    inputReader->addMessageListener(inputBuffer);

    inputBuffer->addMessageListener(inputProcessor);
    inputBuffer->addNotificationListener(inputProcessor);

    inputProcessor->addMessageListener(outputBuffer);

    outputBuffer->addNotificationListener(publisher);
    outputBuffer->addMessageListener(publisher);
    outputBuffer->addNotificationListener(logger);
    outputBuffer->addMessageListener(logger);

    /* creating metrics*/
    globalMetrics["input reader"] = inputReader->getMetrics();
    globalMetrics["input processor"] = inputProcessor->getMetrics();
    globalMetrics["publisher"] = publisher->getMetrics();

    SharedMultyMetrics loggerMetrics{logger->getMetrics()};
    for (size_t idx{0}; idx < loggingThreadCount; ++idx)
    {
      auto threadName = std::string{"logger thread#"} + std::to_string(idx);
      globalMetrics[threadName] = loggerMetrics[idx];
    }
  }

  ~CommandProcessorInstance()
  {
    shouldExit.store(true);
    terminationNotifier.notify_all();
  }

  void reactMessage(MessageBroadcaster* sender, Message message)
  {
    if (messageCode(message) < 1000) // non error message
    {
      switch(message)
      {
      case Message::AllDataReceived :
        #ifdef NDEBUG
        #else
          //std::cout << "\n                     AllDataReceived received\n";
        #endif

        dataReceived.store(true);
        terminationNotifier.notify_all();
        break;

      case Message::AllDataLogged :
        #ifdef NDEBUG
        #else
          //std::cout << "\n                     AllDataLogged received\n";
        #endif

        dataLogged.store(true);
        terminationNotifier.notify_all();
        break;

      case Message::AllDataPublsihed :
        #ifdef NDEBUG
        #else
          //std::cout << "\n                     AllDataReceived received\n";
        #endif

        dataPublished.store(true);
        terminationNotifier.notify_all();
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
        errorMessage = message;
        terminationNotifier.notify_all();
      }
    }
  }

  SharedGlobalMetrics run() noexcept
  {
    try
    {
      inputReader->addMessageListener(shared_from_this());
      publisher->addMessageListener(shared_from_this());
      logger->addMessageListener(shared_from_this());


      externalBuffer->start();
      inputBuffer->start();
      outputBuffer->start();

      publisher->start();
      logger->start();

      inputProcessor->start();

      inputReader->startAndWait();

      /* wait for data processing termination */
      while (shouldExit.load() != true
             && ((dataReceived && dataLogged && dataPublished) != true))
      {
        #ifdef NDEBUG
        #else
          //std::cout << "\n                     CPInstance waiting\n";
        #endif

        std::unique_lock<std::mutex> lockNotifier{notifierLock};
        terminationNotifier.wait_for(lockNotifier, std::chrono::seconds{1}, [this]()
        {
          return (  shouldExit.load()

                    || (dataReceived.load()
                        && dataLogged.load()
                        && dataPublished.load())     );
        });
        lockNotifier.unlock();
      }

      #ifdef NDEBUG
      #else
        //std::cout << "\n                     CPInsatnce waiting ended\n";
      #endif

      if (shouldExit.load() == true)
      {
        sendMessage(errorMessage);
      }

      /* waiting for all workers to finish */
      while(inputReader->getWorkerState() != WorkerState::Finished
            && inputProcessor->getWorkerState() != WorkerState::Finished
            && inputBuffer->getWorkerState() != WorkerState::Finished
            && outputBuffer->getWorkerState() != WorkerState::Finished
            && logger->getWorkerState() != WorkerState::Finished
            && publisher->getWorkerState() != WorkerState::Finished)
      {}

      if (shouldExit.load() == true)
      {
        errorOut << "Abnormal termination\n";
        errorOut << "Error code: " << messageCode(errorMessage);
      }

      #ifdef NDEBUG
      #else
        //std::cout << "\n                     CP metrics output\n";
      #endif

      return globalMetrics;
    }
    catch(const std::exception& ex)
    {
      errorOut << ex.what();
      return globalMetrics;
    }
  }

  const std::shared_ptr<InputReader::InputBufferType>&
  getEntryPoint() const
  { return externalBuffer; }


  const std::shared_ptr<InputProcessor::InputBufferType>&
  getInputBuffer() const
  { return inputBuffer; }


  const std::shared_ptr<InputProcessor::OutputBufferType>&
  getOutputBuffer() const
  { return outputBuffer; }

  const SharedGlobalMetrics getMetrics()
  { return globalMetrics;}

  std::mutex& getScreenOutputLock()
  { return screenOutputLock; }


private:
  std::mutex screenOutputLock;

  std::shared_ptr<InputReader::InputBufferType> externalBuffer;
  std::shared_ptr<InputProcessor::InputBufferType> inputBuffer;
  std::shared_ptr<InputProcessor::OutputBufferType> outputBuffer;
  std::shared_ptr<InputReader> inputReader;
  std::shared_ptr<Logger<loggingThreadCount>> logger;
  std::shared_ptr<Publisher> publisher;
  std::shared_ptr<InputProcessor> inputProcessor;

  std::mutex inputStreamLock{};

  std::atomic_bool dataReceived;
  std::atomic_bool dataPublished;
  std::atomic_bool dataLogged;
  std::atomic_bool shouldExit;

  std::condition_variable terminationNotifier{};
  std::mutex notifierLock;

  std::ostream& errorOut;
  std::ostream& metricsOut;
  SharedGlobalMetrics globalMetrics;

  Message errorMessage{Message::SystemError};
};

