// otus_hw_12_test.cpp in Otus homework#12 project

#define BOOST_TEST_MODULE OTUS_HW_12_TEST

#include <boost/test/included/unit_test.hpp>
#include "homework_12.h"
#include "./async_command_server/async_command_server.h"



#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <vector>

enum class DebugOutput
{
  debug_on,
  debug_off
};

/* Helper functions */
std::array<std::vector<std::string>, 3>
getProcessorOutput
(
  const std::string& inputString,
  char openDelimiter,
  char closeDelimiter,
  size_t bulkSize,
  DebugOutput debugOutput,
  SharedGlobalMetrics metrics
)
{
  std::stringstream inputStream{inputString};
  std::stringstream outputStream{};
  std::stringstream errorStream{};
  std::stringstream metricsStream{};

  {
    AsyncCommandProcessor<2> testProcessor {
      "test processor",
      bulkSize, openDelimiter, closeDelimiter,
      outputStream, errorStream, metricsStream
    };

    testProcessor.run(true);

    metrics = testProcessor.getMetrics();
  }

  std::array<std::vector<std::string>, 3> result {};

  std::string tmpString{};
  while(std::getline(outputStream, tmpString))
  {
    if (DebugOutput::debug_on == debugOutput)
    {
      std::cout << tmpString << '\n';
    }
    result[0].push_back(tmpString);
  }

  while(std::getline(errorStream, tmpString))
  {
    result[1].push_back(tmpString);
  }

  while(std::getline(metricsStream, tmpString))
  {
    result[2].push_back(tmpString);
  }

  return result;
}


void*
mockConnect(
    size_t bulkSize,
    std::stringstream& outputStream,
    std::stringstream& errorStream,
    std::stringstream& metricsStream
)
{
  auto newCommandProcessor {new AsyncCommandProcessor<2>(
      "mock processor", bulkSize, '{', '}', outputStream, errorStream
    )
  };

  if (newCommandProcessor->connect() == true)
  {
    return reinterpret_cast<void*> (newCommandProcessor);
  }
  else
  {
    return nullptr;
  }
}

void checkMetrics(const SharedGlobalMetrics& metrics,
  const size_t receptionCountExpected,
  const size_t characterCountExpected,
  const size_t stringCountExpected,
  const size_t commandCountExpected,
  const size_t bulkCountExpected,
  const size_t loggingThreadCount
  )
{
  BOOST_CHECK(metrics.size() == 3 + loggingThreadCount);

  BOOST_CHECK(metrics.at("input reader")->totalReceptionCount == receptionCountExpected);
  BOOST_CHECK(metrics.at("input reader")->totalCharacterCount == characterCountExpected);
  BOOST_CHECK(metrics.at("input reader")->totalStringCount == stringCountExpected);

  BOOST_CHECK(metrics.at("input processor")->totalStringCount == stringCountExpected);
  BOOST_CHECK(metrics.at("input processor")->totalCommandCount == commandCountExpected);
  BOOST_CHECK(metrics.at("input processor")->totalBulkCount == bulkCountExpected);

  BOOST_CHECK(metrics.at("publisher")->totalCommandCount == commandCountExpected);
  BOOST_CHECK(metrics.at("publisher")->totalBulkCount == bulkCountExpected);

  ThreadMetrics loggingMetrics{"loggers all threads"};

  for (size_t idx{0}; idx < loggingThreadCount; ++idx)
  {
    auto threadName = std::string{"logger thread#"} + std::to_string(idx);
    loggingMetrics += *metrics.at(threadName);
  }

  BOOST_CHECK (loggingMetrics == *metrics.at("publisher"));
}

BOOST_AUTO_TEST_SUITE(homework_12_test)

BOOST_AUTO_TEST_CASE( test_1 )
{

}

BOOST_AUTO_TEST_SUITE_END()
