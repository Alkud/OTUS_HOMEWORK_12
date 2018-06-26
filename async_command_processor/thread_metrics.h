#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

/// Thread metrics container
struct ThreadMetrics
{
  ThreadMetrics() = delete;

  /// Default constructor
  ThreadMetrics(const std::string& newThreadName) :
  threadName{newThreadName}{}

  /// Another constructor form
  ThreadMetrics(std::string&& newThreadName) :
  threadName{std::move(newThreadName)}{}

  /// Using default copy constructor
  ThreadMetrics(const ThreadMetrics& other) = default;

  /// Using default move constructor
  ThreadMetrics(ThreadMetrics&& other) = default;

  /// name of thread to measure
  const std::string threadName;

  /// count of bulks parsed
  size_t totalBulkCount{};
  /// count of chatacters totally received
  size_t totalCharacterCount{};
  /// count of commands received (except bulk delimeters)
  size_t totalCommandCount{};
  /// count data reception calls
  size_t totalReceptionCount{};
  /// count of strings totally received
  size_t totalStringCount{};

  /// Using default copying equal operator
  ThreadMetrics& operator=(const ThreadMetrics& other) = default;

  /// Using default moving equal operator
  ThreadMetrics& operator=(ThreadMetrics&& other) = default;

  /// Metrics summ operation
  ThreadMetrics& operator+(const ThreadMetrics& other)
  {
    totalBulkCount += other.totalBulkCount;
    totalCharacterCount +=other.totalCharacterCount;
    totalCommandCount += other.totalCommandCount;
    totalReceptionCount += other.totalReceptionCount;
    totalStringCount += other.totalStringCount;

    return *this;
  }

  ThreadMetrics& operator+=(const ThreadMetrics& other)
  {
    totalBulkCount += other.totalBulkCount;
    totalCharacterCount +=other.totalCharacterCount;
    totalCommandCount += other.totalCommandCount;
    totalReceptionCount += other.totalReceptionCount;
    totalStringCount += other.totalStringCount;

    return *this;
  }

  /// Metrics comparison operation
  bool operator==(const ThreadMetrics& other) const
  {
    return (totalBulkCount == other.totalBulkCount &&
            totalCharacterCount == other.totalCharacterCount &&
            totalCommandCount == other.totalCommandCount &&
            totalReceptionCount == other.totalReceptionCount &&
            totalStringCount == other.totalStringCount);
  }

  /// Just zero all values
  void clear()
  {
    totalBulkCount = 0;
    totalCharacterCount = 0;
    totalCommandCount = 0;
    totalReceptionCount = 0;
    totalStringCount = 0;
  }
};

using SharedMetrics = std::shared_ptr<ThreadMetrics>;

/// type definition for multiple thread metrics collections

using MultyMetrics = std::vector<ThreadMetrics>;

using SharedMultyMetrics = std::vector<std::shared_ptr<ThreadMetrics>>;

using GlobalMetrics = std::unordered_map<std::string, ThreadMetrics>;

using SharedGlobalMetrics = std::unordered_map<std::string, std::shared_ptr<ThreadMetrics>>;
