#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>


struct Command {
  std::string text;
  std::chrono::system_clock::time_point timeStamp;
};

class CommandProcessor {
public:
  CommandProcessor(CommandProcessor* nextCommandProcessor = nullptr)
    : m_nextCommandProcessor(nextCommandProcessor) {}

  virtual ~CommandProcessor() = default;

  virtual void StartBlock() {}
  virtual void FinishBlock() {}

  virtual void ProcessCommand(const Command& command) = 0;
protected:
  CommandProcessor* m_nextCommandProcessor;
};

class ConsoleInput : public CommandProcessor {
public:
  ConsoleInput(CommandProcessor* nextCommandProcessor = nullptr)
    : CommandProcessor(nextCommandProcessor)
    , m_blockDepth(0) {}

  void ProcessCommand(const Command& command) override {
    if (m_nextCommandProcessor) {
      if (command.text == "{") {
        if (m_blockDepth++ == 0)
          m_nextCommandProcessor->StartBlock();
      }
      else if (command.text == "}") {
        if (--m_blockDepth == 0)
          m_nextCommandProcessor->FinishBlock();
      }
      else
        m_nextCommandProcessor->ProcessCommand(command);
    }
  }

private:
  int m_blockDepth;
};

class ConsoleOutput : public CommandProcessor {
public:
  ConsoleOutput(CommandProcessor* nextCommandProcessor = nullptr)
    : CommandProcessor(nextCommandProcessor) {}

  void ProcessCommand(const Command& command) override {
    std::cout << command.text << std::endl;
    if (m_nextCommandProcessor)
      m_nextCommandProcessor->ProcessCommand(command);
  }
};

class ReportWriter : public CommandProcessor {
public:
  ReportWriter(CommandProcessor* nextCommandProcessor = nullptr)
    : CommandProcessor(nextCommandProcessor) {}

  void ProcessCommand(const Command& command) override {
    std::ofstream file(GetFilename(command), std::ofstream::out);
    file << command.text;
    // wait
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1ms);

    if (m_nextCommandProcessor)
      m_nextCommandProcessor->ProcessCommand(command);
  }

private:
  std::string GetFilename(const Command& command) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
          command.timeStamp.time_since_epoch()).count();
    std::stringstream filename;
    filename << "bulk" << seconds << ".log";
    return filename.str();
  }
};

class BatchCommandProcessor : public CommandProcessor {
public:
  BatchCommandProcessor(int bulkSize, CommandProcessor* nextCommandProcessor)
    : CommandProcessor(nextCommandProcessor)
    , m_bulkSize(bulkSize)
    , m_blockForced(false) {}

  ~BatchCommandProcessor() {
    if (!m_blockForced)
      DumpBatch();
  }

  void StartBlock() override {
    m_blockForced = true;
    DumpBatch();
  }

  void FinishBlock() override {
    m_blockForced = false;
    DumpBatch();
  }

  void ProcessCommand(const Command& command) override {
    m_commands.push_back(command);

    if (!m_blockForced && m_commands.size() >= m_bulkSize) {
      DumpBatch();
    }
  }
private:
  void ClearBatch() {
    m_commands.clear();
  }

  void DumpBatch() {
    if (m_nextCommandProcessor && !m_commands.empty()) {
      std::string output = "bulk: " + Join(m_commands);
      m_nextCommandProcessor->ProcessCommand(Command{output, m_commands[0].timeStamp});
    }
    ClearBatch();
  }

  static std::string Join(const std::vector<Command>& v) {
    std::stringstream ss;
    for(size_t i = 0; i < v.size(); ++i) {
      if(i != 0)
        ss << ", ";
      ss << v[i].text;
    }
    return ss.str();
  }
  int m_bulkSize;
  bool m_blockForced;
  std::vector<Command> m_commands;
};


