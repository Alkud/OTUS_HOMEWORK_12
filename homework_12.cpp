// homework_12.cpp in Otus homework#12 project

#include <array>
#include <utility>
#include <csignal>
#include <atomic>
#include "homework_12.h"
#include "./async_command_server/async_command_server.h"


std::atomic<bool> shouldExit{false};

std::condition_variable terminationNotifier{};
std::mutex terminationLock{};


void terminationHandler(int)
{
//  struct sigaction terminationAction{};
//  terminationAction.sa_handler = terminationHandler;
//  sigset_t  signalSet;
//  sigemptyset(&signalSet);
//  sigaddset(&signalSet, SIGINT);
//  terminationAction.sa_mask = signalSet;
//  sigaction(SIGINT, &terminationAction, 0);

//  int sig;
//  sigwait(&signalSet, &sig);

  std::cout << "\nSIGINT cought\n";

  shouldExit.store(true);
  terminationNotifier.notify_all();

//  std::this_thread::sleep_for(10s);
}

int homework(int argc, char* argv[], std::ostream& outputStream,
              std::ostream& errorStream, std::ostream& metricsStream)
{
  std::signal(SIGINT, terminationHandler);
//  sigset_t base_mask;
//  sigfillset(&base_mask);
//  /* Block user interrupts while doing other processing. */
//  sigprocmask (SIG_SETMASK, &base_mask, NULL);

//  struct sigaction terminationAction{};
//  terminationAction.sa_handler = terminationHandler;
//  sigset_t  signalSet;
//  sigemptyset(&signalSet);
//  sigaddset(&signalSet, SIGINT);
//  terminationAction.sa_mask = signalSet;
//  sigaction(SIGINT, &terminationAction, 0);
//  sigset_t  signalSet;
//  sigfillset(&signalSet);
//  pthread_sigmask(SIG_BLOCK, &signalSet, nullptr);

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

  std::thread mainThread{[&server]()
   {
//      sigset_t  signalSet;
//      sigfillset(&signalSet);
//      pthread_sigmask(SIG_BLOCK, &signalSet, nullptr);

      server.start();

      while (shouldExit.load() != true)
      {
        std::unique_lock<std::mutex> lockTermination{terminationLock};
        terminationNotifier.wait_for(lockTermination, 100ms, []()
        {
          return shouldExit.load() == true;
        });
        lockTermination.unlock();
      }

      std::cout << "\nSIGINT STOP\n";

      server.stop();
   }};

  //std::thread terminationThread{terminationHandler, SIGINT};


//  for (;;)
//  {
//    /* After a while, check to see whether any signals are pending. */
//    sigpending (&waiting_mask);
//    if (shouldExit.load() != true && sigismember (&waiting_mask, SIGINT))
//    {
//      //terminationHandler(1);
//      shouldExit.store(true);
//      terminationNotifier.notify_all();

//      std::this_thread::sleep_for(12s);

//      //if (mainThread.joinable()) mainThread.join();

//      //break;
//    }
//    else
//    {
//      std::this_thread::sleep_for(1s);
//    }
//  }

//  char userInput{};

//  do
//  {
//    std::cin >> userInput;
//    std::cout << "\n" << userInput << "\n";
//  }
//  while (userInput != 3);

//  shouldExit.store(true);

  //terminationThread.join();
  mainThread.join();

  return 0;
}
