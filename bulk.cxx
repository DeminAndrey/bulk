#include "CommandProcessor.h"


void RunBulk(int bulkSize) {
  BatchConsoleInput consoleInput(bulkSize);

  std::string text;
  while (std::getline(std::cin, text)) {
    consoleInput.ProcessCommand(Command{text, std::chrono::system_clock::now()});
  }
}

int main(int argc, char const** argv) {
//  try {
//    if (argc < 2) {
//      std::cerr << "Bulk size is not specified." << std::endl;
//      return 1;
//    }

//    int bulkSize = atoi(argv[1]);
//    if (bulkSize == 0) {
//      std::cerr << "Invalid bulk size." << std::endl;
//      return 1;
//    }

//    RunBulk(bulkSize);
//    return 0;
//  }
//  catch (const std::exception &e) {
//    std::cerr << e.what() << std::endl;
//  }

  RunBulk(3);

  return 0;
}
