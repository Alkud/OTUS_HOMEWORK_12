// homework_12.cpp in Otus homework#12 project

#include <array>
#include <utility>
#include <csignal>
#include <atomic>
#include "homework_12.h"
#include "./async_command_server/async_command_server.h"


std::atomic<bool> shouldExit{false};

std::condition_variable terminationNotifier;
std::mutex terminationLock;

extern "C" void terminationHandler(int)
{
  std::cout << "\nServer stopped\n";
  shouldExit.store(true);
  terminationNotifier.notify_all();
}

int homework(int argc, char* argv[], std::ostream& outputStream,
              std::ostream& errorStream, std::ostream& metricsStream)
{
  if (argc < 3 || std::stoi(std::string{argv[2]}) < 1)
  {
    errorStream << "usage: bulkserver <port> <bulk size>" << std::endl;
    return 1;
  }

  std::signal(SIGINT, terminationHandler);
  std::signal(SIGTERM, terminationHandler);

  uint16_t portNumber{static_cast<uint16_t>(std::stoull(std::string{argv[1]}))};
  size_t bulkSize{std::stoull(std::string{argv[2]})};

  AsyncCommandServer<2> server{
    asio::ip::address_v4::any(), portNumber,
    bulkSize, '{', '}',
    outputStream, errorStream, metricsStream
  };

  std::cout << "\nServer started\n";

  server.start();

  while (shouldExit.load() != true)
  {
    std::unique_lock<std::mutex> lockTermination{terminationLock};
    terminationNotifier.wait(lockTermination, []()
    {
      return shouldExit.load() == true;
    });
    lockTermination.unlock();
  }

  server.stop();

  return 0;
}
