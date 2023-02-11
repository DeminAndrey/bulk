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
static const std::string START_BLOCK = "{";
static const std::string END_BLOCK = "}";

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
 * @brief класс обработчика команд
 */
class BatchCommandProcessor { // publisher
public:
  BatchCommandProcessor(int bulkSize)
    : m_bulkSize(bulkSize) {}

  ~BatchCommandProcessor() {
    if (!m_blockForced) {
      DumpBatch();
    }
    for (auto subscrube : m_subscribers) {
      unSubscribe(subscrube);
    }
  }

  void StartBlock() {
    m_blockForced = true;
    DumpBatch();
  }

  void FinishBlock() {
    m_blockForced = false;
    DumpBatch();
  }

  void ProcessCommand(const Command& command) {
    m_commands.push_back(command);
    notify();

    if (!m_blockForced &&
        (m_commands.size() >= static_cast<size_t>(m_bulkSize))) {
      DumpBatch();
    }
  }

  void subscribe(Output *o) noexcept {
    if (o) {
      m_subscribers.push_back(o);
    }
  }

  void unSubscribe(Output *o) noexcept {
    if (o) {
      m_subscribers.erase(
            std::remove(
              m_subscribers.begin(), m_subscribers.end(), o));
    }
  }

  void notify() noexcept {
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
  bool m_blockForced = false;
  std::vector<Command> m_commands;
  std::vector<Output*> m_subscribers;
};

/**
 * @brief класс вывода команд в консоль
 */
class ConsoleOutput : public Output { // subscriber
public:
  explicit ConsoleOutput(BatchCommandProcessor *processor) {
    if (processor) {
      processor->subscribe(this);
    }
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

/**
 * @brief класс записи команд в файл
 */
class ReportWriter : public Output { // subscriber
public:
  explicit ReportWriter(BatchCommandProcessor *processor) {
    if (processor) {
      processor->subscribe(this);
    }
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

/**
 * @brief класс работы с командами из консоли
 */
class BatchConsoleInput {
public:
  BatchConsoleInput(int bulkSize) {
    m_commandProcessor = std::make_unique<BatchCommandProcessor>(bulkSize);
    m_output.push_back(std::make_unique<ReportWriter>(m_commandProcessor.get()));
    m_output.push_back(std::make_unique<ConsoleOutput>(m_commandProcessor.get()));
  }

  void ProcessCommand(const Command& command) {
    if (m_commandProcessor) {
      if (command.text == START_BLOCK) {
        if (m_blockDepth++ == 0)
          m_commandProcessor->StartBlock();
      }
      else if (command.text == END_BLOCK) {
        if (--m_blockDepth == 0)
          m_commandProcessor->FinishBlock();
      }
      else
        m_commandProcessor->ProcessCommand(command);
    }
  }
private:
  int m_blockDepth = 0;
  std::unique_ptr<BatchCommandProcessor> m_commandProcessor;
  std::vector<std::unique_ptr<Output>> m_output;
};
