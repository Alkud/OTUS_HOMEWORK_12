// otus_hw_12_test.cpp in Otus homework#12 project

#define BOOST_TEST_MODULE OTUS_HW_12_TEST

#include <boost/test/included/unit_test.hpp>
#include "./async_command_server/async_command_server.h"

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

using namespace std::chrono_literals;

enum class DebugOutput
{
  debug_on,
  debug_off
};

constexpr size_t MAX_TEST_MESSAGE_SIZE = 1280;

/* Helper functions */
/* By Béchu Jérôme. SOURCE: https://gist.github.com/bechu/2423333 */
void sendMessage(const asio::ip::address_v4 address, const uint16_t portNumber, std::string& message)
{
  asio::io_service service;

  asio::ip::tcp::endpoint endpoint{address, portNumber};

  asio::ip::tcp::socket socket{service};

  socket.connect(endpoint);

  std::array<char, MAX_TEST_MESSAGE_SIZE> sendBuffer;

  if (message.length() > MAX_TEST_MESSAGE_SIZE)
  {
    message.resize(MAX_TEST_MESSAGE_SIZE);
  }

  std::copy(message.begin(), message.end(), sendBuffer.begin());

  system::error_code error;

  socket.write_some(asio::buffer(sendBuffer, message.size()), error);

  socket.close();
}

std::array<std::vector<std::string>, 3>
getServerOutput
(
  std::vector<std::string>& inputStrings,
  const char openDelimiter,
  const char closeDelimiter,
  const size_t bulkSize,
  const DebugOutput debugOutput,
  SharedGlobalMetrics& metrics
)
{
  std::stringstream outputStream{};
  std::stringstream errorStream{};
  std::stringstream metricsStream{};

  const uint16_t portNumber{12345};

  auto serverAddress{asio::ip::address_v4::any()};

  { /* server working scope */

    AsyncCommandServer<4> testServer {
      serverAddress, portNumber,
      bulkSize, openDelimiter, closeDelimiter,
      outputStream, errorStream, metricsStream
    };

    metrics = testServer.getMetrics();

    testServer.start();

    std::vector<std::thread> sendingThreads{};

    for (auto& stringToSend : inputStrings)
    {
      sendingThreads.push_back(
        std::thread {[serverAddress, portNumber, &stringToSend]()
        {
          sendMessage(serverAddress, portNumber, stringToSend);
          std::this_thread::sleep_for(400ms);
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

    testServer.stop();

  } /* end of server working scope */

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
  BOOST_CHECK(metrics.size() == 2 + loggingThreadCount);

//  BOOST_CHECK(metrics.at("input reader")->totalReceptionCount == receptionCountExpected);
//  BOOST_CHECK(metrics.at("input reader")->totalCharacterCount == characterCountExpected);
//  BOOST_CHECK(metrics.at("input reader")->totalStringCount == stringCountExpected);

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

BOOST_AUTO_TEST_SUITE(BASIC_TESTS)

BOOST_AUTO_TEST_CASE(simple_test)
{
  try
  {
    std::vector<std::string> testStrings {"1\n2\n3\n4\n"};

    SharedGlobalMetrics metrics;

    auto serverOutput(getServerOutput(testStrings, '{', '}', 4, DebugOutput::debug_on, metrics));

    /* make sure server output is correct */
    if (serverOutput[0].size() == 1)
    {
      BOOST_CHECK(serverOutput[0][0] == "bulk: 1, 2, 3, 4");
    }
    else
    {
      std::cout << serverOutput[0].size() << std::endl;
      BOOST_FAIL("");
    }

    /* make sure no errors occured */    
    BOOST_CHECK(serverOutput[1].size() == 0);
    if (serverOutput[1].size() != 0)
    {
        for (const auto& errorString : serverOutput[1])
        {
            std::cout << errorString << std::endl;
        }
    }

    checkMetrics(metrics, 4, 8, 4, 4, 1, 2);
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what();
    BOOST_FAIL("");
  }
}

BOOST_AUTO_TEST_CASE(two_connections_no_mix_test)
{
  try
  {
    std::vector<std::string> testStrings {{"{\n1\n2\n3\n4\n}\n"}, {"{\n11\n12\n13\n14\n}\n"}};

    SharedGlobalMetrics metrics;

    auto serverOutput(getServerOutput(testStrings, '{', '}', 2, DebugOutput::debug_on, metrics));

    /* make sure server output is correct */
    if (serverOutput[0].size() == 2)
    {
      BOOST_CHECK(   (serverOutput[0][0] == "bulk: 1, 2, 3, 4"
                      && serverOutput[0][1] == "bulk: 11, 12, 13, 14")
                  || (serverOutput[0][0] == "bulk: 11, 12, 13, 14"
                      && serverOutput[0][1] == "bulk: 1, 2, 3, 4")     );
    }
    else
    {
      std::cout << serverOutput[0].size() << std::endl;
      BOOST_FAIL("");
    }

    /* make sure no errors occured */
    BOOST_CHECK(serverOutput[1].size() == 0);
    if (serverOutput[1].size() != 0)
    {
        for (const auto& errorString : serverOutput[1])
        {
            std::cout << errorString << std::endl;
        }
    }

    checkMetrics(metrics, 2, 28, 12, 8, 2, 2);
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what();
    BOOST_FAIL("");
  }
}

BOOST_AUTO_TEST_CASE(four_connections_mixing_test)
{
  try
  {
    std::vector<std::string> testStrings {{"11\n12\n13\n14\n"}, {"21\n22\n23\n24\n"},
                                          {"31\n32\n33\n34\n"}, {"41\n42\n43\n44\n"}};

    SharedGlobalMetrics metrics;

    auto serverOutput(getServerOutput(testStrings, '{', '}', 1, DebugOutput::debug_on, metrics));

    /* make sure server output is correct */
    if (serverOutput[0].size() == 16)
    {
      std::sort(serverOutput[0].begin(), serverOutput[0].end());

      size_t idx {};
      for (size_t decade{1}; decade < 5; ++decade)
      {
        for (size_t unit {1}; unit < 5; ++unit)
        {
          BOOST_CHECK(serverOutput[0][idx++] == std::string{"bulk: "} + std::to_string(unit + decade * 10));
        }
      }
    }
    else
    {
      BOOST_FAIL("");
    }

    /* make sure no errors occured */
    BOOST_CHECK(serverOutput[1].size() == 0);
    if (serverOutput[1].size() != 0)
    {
        for (const auto& errorString : serverOutput[1])
        {
            std::cout << errorString << std::endl;
        }
    }

    checkMetrics(metrics, 16, 48, 16, 16, 16, 2);
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what();
    BOOST_FAIL("");
  }
}

BOOST_AUTO_TEST_CASE(empty_command_test)
{
  try
  {
    std::vector<std::string> testStrings {"\n"};

    SharedGlobalMetrics metrics;

    auto serverOutput(getServerOutput(testStrings, '<', '>', 19, DebugOutput::debug_on, metrics));

    /* make sure server output is correct */
    if (serverOutput[0].size() == 1)
    {
      BOOST_CHECK(serverOutput[0][0] == "bulk: ");
    }
    else
    {
      BOOST_FAIL("");
    }

    /* make sure no errors occured */
    BOOST_CHECK(serverOutput[1].size() == 0);
    if (serverOutput[1].size() != 0)
    {
        for (const auto& errorString : serverOutput[1])
        {
            std::cout << errorString << std::endl;
        }
    }

    checkMetrics(metrics, 1, 1, 1, 1, 1, 2);
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what();
    BOOST_FAIL("");
  }
}

BOOST_AUTO_TEST_CASE(unterminated_command_test)
{
  try
  {
    std::vector<std::string> testStrings {"aSde\n1\n18\nfGthyuyuyhgf"};

    SharedGlobalMetrics metrics;

    auto serverOutput(getServerOutput(testStrings, '[', ']', 6, DebugOutput::debug_on, metrics));

    /* make sure server output is correct */
    if (serverOutput[0].size() == 1)
    {
      BOOST_CHECK(serverOutput[0][0] == "bulk: aSde, 1, 18");
    }
    else
    {
      BOOST_FAIL("");
    }

    /* make sure no errors occured */
    BOOST_CHECK(serverOutput[1].size() == 0);
    if (serverOutput[1].size() != 0)
    {
        for (const auto& errorString : serverOutput[1])
        {
            std::cout << errorString << std::endl;
        }
    }

    checkMetrics(metrics, 3, 10, 3, 3, 1, 2);
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what();
    BOOST_FAIL("");
  }
}

BOOST_AUTO_TEST_SUITE_END()
