// bulkserver.cpp in Otus homework#12 project

#include "./homework_12.h"
#include <stdexcept>
#include <iostream>

int main(int argc, char* argv[])
{
  try
  {
    return(homework(argc, argv));
  }
  catch(const std::exception& ex)
  {
    std::cerr << ex.what();
  }
}
