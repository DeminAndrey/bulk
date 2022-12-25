#pragma once

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static const std::string BULK = "bulk: ";

struct Command {
  std::string text;
  std::chrono::system_clock::time_point timeStamp;
};

/**
 * @brief базовый класс для вывода
 */
class Output {
public:
  virtual void update(const std::vector<Command>& command) = 0;
  virtual void ProcessCommand() = 0;
  virtual ~Output() = default;

protected:
  std::string Join(const std::vector<Command>& v) const {
    return std::accumulate(v.begin(), v.end(), std::string(),
                           [](std::string &s, const Command &com) {
      return s.empty() ? s.append(com.text)
                       : s.append(", ").append(com.text);
    });
  }
};

/**
 * @brief базовый класс обработчика команд
 */
class CommandProcessor {
public:
  CommandProcessor(CommandProcessor* nextCommandProcessor = nullptr)
    : m_nextCommandProcessor(nextCommandProcessor) {}

  virtual ~CommandProcessor() = default;

  virtual void StartBlock() {}
  virtual void FinishBlock() {}

  virtual void ProcessCommand(const Command& command) = 0;

  virtual void subscribe(Output *) noexcept {}
  virtual void unSubscribe(Output *) noexcept {}
  virtual void notify() noexcept {}

protected:
  CommandProcessor* m_nextCommandProcessor;
  std::vector<Output*> m_subscribers;
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

class BatchCommandProcessor : public CommandProcessor { // publisher
public:
  BatchCommandProcessor(int bulkSize, CommandProcessor* nextCommandProcessor = nullptr)
    : CommandProcessor(nextCommandProcessor)
    , m_bulkSize(bulkSize)
    , m_blockForced(false) {}

  ~BatchCommandProcessor() override {
    if (!m_blockForced) {
      DumpBatch();
    }
    for (auto subscrube : m_subscribers) {
      unSubscribe(subscrube);
    }
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
    notify();

    if (!m_blockForced &&
        (m_commands.size() >= static_cast<size_t>(m_bulkSize))) {
      DumpBatch();
    }
  }

  void subscribe(Output *o) noexcept override {
    if (o) {
      m_subscribers.push_back(o);
    }
  }

  void unSubscribe(Output *o) noexcept override {
    if (o) {
      m_subscribers.erase(
            std::remove(
              m_subscribers.begin(), m_subscribers.end(), o));
    }
  }

  void notify() noexcept override {
    for (auto subscriber : m_subscribers) {
      if (subscriber) {
        subscriber->update(m_commands);
      }
    }
  }

private:
  void ClearBatch() {
    m_commands.clear();
    notify();
  }

  void DumpBatch() {
    if (!m_commands.empty()) {
      for (auto o : m_subscribers) {
        o->ProcessCommand();
      }
    }
    ClearBatch();
  }

  int m_bulkSize;
  bool m_blockForced;
  std::vector<Command> m_commands;
};

class ConsoleOutput : public Output { // subscriber
public:
  explicit ConsoleOutput(CommandProcessor *processor) {
    processor->subscribe(this);
  }

  void update(const std::vector<Command>& commands) override {
    m_commands = commands;
  }

  void ProcessCommand() override {
    auto output = BULK + Join(m_commands);
    std::cout << output << std::endl;
  }

private:
  std::vector<Command> m_commands;
};

class ReportWriter : public Output { // subscriber
public:
  explicit ReportWriter(CommandProcessor *processor) {
    processor->subscribe(this);
  }

  void update(const std::vector<Command>& commands) override {
    m_commands = commands;
  }

  void ProcessCommand() override {
    auto output = BULK + Join(m_commands);
    std::ofstream file(GetFilename(), std::ofstream::out);
    file << output;
    // wait
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1ms);
  }

private:
  std::vector<Command> m_commands;

  std::string GetFilename() {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
          m_commands[0].timeStamp.time_since_epoch()).count();
    std::stringstream filename;
    filename << "bulk" << seconds << ".log";

    return filename.str();
  }
};
