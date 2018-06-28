// homework_12.cpp in Otus homework#12 project

#include <array>
#include <utility>
#include "homework_12.h"
#include "./async_command_server/async_command_server.h"


int homework(int argc, char* argv[], std::istream& inputStream, std::ostream& outputStream,
              std::ostream& errorStream, std::ostream& metricsStream)
{
  if (argc < 3 || std::stoi(std::string{argv[2]}) < 1)
  {
    errorStream << "usage: bulkserver <port> <bulk size>" << std::endl;
    return 1;
  }

  uint16_t portNumber{static_cast<uint16_t>(std::stoull(std::string{argv[1]}))};
  size_t bulkSize{std::stoull(std::string{argv[2]})};

  AsyncCommandServer<2> server{
    asio::ip::address_v4::any(), portNumber,
    bulkSize, '{', '}',
    outputStream, errorStream, metricsStream
  };

  server.start();

  return 0;
}
