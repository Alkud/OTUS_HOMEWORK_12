// otus_hw_12_test.cpp in Otus homework#12 project

#define BOOST_TEST_MODULE OTUS_HW_12_TEST

#include <boost/test/included/unit_test.hpp>
#include "./async_command_server/async_command_server.h"
#include "homework_12.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <algorithm>

using namespace boost;

enum class DebugOutput
{
  debug_on,
  debug_off
};

/* Helper functions */
/* By Béchu Jérôme. SOURCE: https://gist.github.com/bechu/2423333 */
void sendMessage(const asio::ip::address_v4 address, const uint16_t portNumber, const std::string& message)
{
  asio::io_service service;

  asio::ip::tcp::endpoint endpoint{address, portNumber};

  asio::ip::tcp::socket socket{service};

  socket.connect(endpoint);

  std::array<char, 1280> sendBuffer;

  std::copy(message.begin(), message.end(), sendBuffer.begin());

  system::error_code error;

  socket.write_some(asio::buffer(sendBuffer, message.size()), error);

  socket.close();
}

std::array<std::vector<std::string>, 3>
getServerOutput
(
  const std::vector<std::string>& inputStrings,
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

  uint16_t portNumber{12345};

  auto serverAddress{asio::ip::address_v4::any()};

  AsyncCommandServer<2> testServer {
    serverAddress, portNumber,
    bulkSize, openDelimiter, closeDelimiter,
    outputStream, errorStream, metricsStream
  };

  metrics = testServer.getMetrics();

  std::thread serverThread{[&testServer]()
  {
    testServer.start();
  }};

  std::vector<std::thread> sendingThreads{};

  for (const auto& stringToSend : inputStrings)
  {
    sendingThreads.push_back(
      std::thread {[serverAddress, portNumber, &stringToSend]()
      {
        sendMessage(serverAddress, portNumber, stringToSend);
      }}
    );
  }

  for (auto& thread : sendingThreads)
  {
    if (thread.joinable())
    {
      thread.join();
    }
  }

  serverThread.join();

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

BOOST_AUTO_TEST_CASE(simple_test)
{
  std::string testString {"1\n2\n3\n4\n"};
  auto serverOutput(getServerOutput(testString, '{', '}', ))
}

BOOST_AUTO_TEST_SUITE_END()
