// command_processor.h in Otus homework#12 project

#pragma once

#include <memory>
#include <mutex>
#include <list>
#include <thread>
#include <functional>
#include <condition_variable>
#include "input_reader.h"
#include "input_processor.h"
#include "simple_buffer_mt.h"
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
    const size_t bulkSize,
    const char bulkOpenDelimiter,
    const char bulkCloseDelimiter,
    std::ostream& outputStream,
    std::ostream& errorStream,
    std::ostream& metricsStream,
    std::mutex& newScreenOutputLock
  ) :
    screenOutputLock{newScreenOutputLock},
    /* creating buffers */    
    inputBuffer{ new InputProcessor::InputBufferType ("command buffer", errorStream, screenOutputLock)},
    publisherBuffer{ new InputProcessor::OutputBufferType ("publisher buffer", errorStream, screenOutputLock)},
    loggerBuffers{},
    /* creating logger */
    logger{
      new Logger<loggingThreadCount> (
      "logger", loggerBuffers, errorStream, screenOutputLock, ""
    )},

    /* creating publisher */
    publisher{
      new Publisher (
      "publisher", publisherBuffer,
      outputStream, screenOutputLock,
      errorStream, screenOutputLock
    )},

    /* creating command processor */
    inputProcessor{
      new InputProcessor (
      "input processor ", bulkSize,
      bulkOpenDelimiter, bulkCloseDelimiter,
      inputBuffer,
      publisherBuffer, loggerBuffers,
      errorStream, screenOutputLock
    )},

    inputStreamLock{},

    dataReceived{false}, dataPublished{false},
    dataLogged{false}, shouldExit{false},

    terminationNotifier{}, notifierLock{},

    errorOut{errorStream}, metricsOut{metricsStream}, globalMetrics{}
  {
    /* create logger buffers */
    for (size_t idx{0}; idx < loggingThreadCount; ++idx)
    {
      loggerBuffers.push_back(
        std::make_shared<InputProcessor::OutputBufferType>(
          std::string{"logger buffer#"} + std::to_string(idx),
          errorStream, screenOutputLock
        )
      );
    }
    /* connect broadcasters and listeners */
    this->addMessageListener(inputBuffer);

    inputBuffer->addMessageListener(inputProcessor);
    inputBuffer->addNotificationListener(inputProcessor);

    inputProcessor->addMessageListener(publisherBuffer);
    for (const auto& loggerBuffer : loggerBuffers)
    {
      inputProcessor->addMessageListener(loggerBuffer);
    }

    publisherBuffer->addNotificationListener(publisher);
    publisherBuffer->addMessageListener(publisher);

    for (const auto& loggerBuffer : loggerBuffers)
    {
      loggerBuffer->addNotificationListener(logger);
      loggerBuffer->addMessageListener(logger);
    }

    /* creating metrics*/    
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

  void reactMessage(MessageBroadcaster* /*sender*/, Message message)
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
          //std::cout << "\n                     AllDataPublished received\n";
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
      inputProcessor->addMessageListener(shared_from_this());
      publisher->addMessageListener(shared_from_this());
      logger->addMessageListener(shared_from_this());


      inputBuffer->start();
      publisherBuffer->start();
      for (const auto& loggerBuffer : loggerBuffers)
      {
        loggerBuffer->start();
      }

      publisher->start();
      logger->start();

      inputProcessor->startAndWait();

      /* wait for data processing termination */
      while (shouldExit.load() != true
             && ((dataReceived && dataLogged && dataPublished) != true))
      {
        #ifdef NDEBUG
        #else
          //std::cout << "\n                     CPInstance waiting\n";
        #endif

        std::unique_lock<std::mutex> lockNotifier{notifierLock};
        terminationNotifier.wait_for(lockNotifier, std::chrono::milliseconds{100}, [this]()
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

      /* waiting for publisher and logger to finish */
      while(logger->getWorkerState() != WorkerState::Finished
            && publisher->getWorkerState() != WorkerState::Finished)
      {}

      if (shouldExit.load() == true)
      {
        std::lock_guard<std::mutex> lockOutput{screenOutputLock};

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
      std::lock_guard<std::mutex> lockOutput{screenOutputLock};

      errorOut << ex.what();
      return globalMetrics;
    }
  }

//  const std::shared_ptr<InputReader::InputBufferType>&
const std::shared_ptr<InputProcessor::InputBufferType>&
  getEntryPoint() const
//  { return externalBuffer; }
  { return inputBuffer; }


  const std::shared_ptr<InputProcessor::InputBufferType>&
  getInputBuffer() const
  { return inputBuffer; }


  const std::shared_ptr<InputProcessor::OutputBufferType>&
  getOutputBuffer() const
  { return publisherBuffer; }

  const SharedGlobalMetrics getMetrics()
  { return globalMetrics;}

  std::mutex& getScreenOutputLock()
  { return screenOutputLock; }


private:
  std::mutex& screenOutputLock;

  //std::shared_ptr<InputReader::InputBufferType> externalBuffer;
  std::shared_ptr<InputProcessor::InputBufferType> inputBuffer;
  std::shared_ptr<InputProcessor::OutputBufferType> publisherBuffer;
  std::vector<std::shared_ptr<InputProcessor::OutputBufferType>> loggerBuffers;

  std::shared_ptr<Logger<loggingThreadCount>> logger;
  std::shared_ptr<Publisher> publisher;
  std::shared_ptr<InputProcessor> inputProcessor;
  //std::shared_ptr<InputReader> inputReader;

  std::mutex inputStreamLock;

  std::atomic_bool dataReceived;
  std::atomic_bool dataPublished;
  std::atomic_bool dataLogged;
  std::atomic_bool shouldExit;

  std::condition_variable terminationNotifier;
  std::mutex notifierLock;

  std::ostream& errorOut;
  std::ostream& metricsOut;
  SharedGlobalMetrics globalMetrics;

  Message errorMessage{Message::SystemError};
};

