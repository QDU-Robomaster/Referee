#pragma once

// Host-only substitutes for UART, RTOS, topics and CMD. CRC uses the real LibXR code.
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

namespace LibXR
{
enum class ErrorCode
{
  OK,
  ARG_ERR,
  BUSY,
  TIMEOUT
};
struct Semaphore
{
  explicit Semaphore(int = 0) {}
};
struct ReadOperation
{
  ReadOperation(Semaphore&, int) {}
};
using WriteOperation = ReadOperation;
struct Mutex
{
  std::mutex native;
  struct LockGuard
  {
    std::lock_guard<std::mutex> guard;
    explicit LockGuard(Mutex& mutex) : guard(mutex.native) {}
  };
};
struct Memory
{
  static void FastCopy(void* dst, const void* src, size_t size)
  {
    if (size != 0)
    {
      std::memcpy(dst, src, size);
    }
  }
};
struct Timebase
{
  static inline std::atomic<uint32_t> now{1};
  static uint64_t GetMilliseconds() { return now.load(); }
};
struct Thread
{
  enum class Priority
  {
    MEDIUM
  };
  template <typename... Args>
  void Create(Args...)
  {
  }
  static void Sleep(int duration) { Timebase::now += duration; }
};

struct Topic
{
  std::string name;
  static inline std::unordered_map<std::string, std::vector<uint8_t>> messages;
  template <typename T>
  static Topic CreateTopic(const char* name, void*, bool)
  {
    return {name};
  }
  template <typename T>
  void Publish(const T& value)
  {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    messages[name] = {bytes, bytes + sizeof(value)};
  }
};
struct UART
{
  enum class Parity
  {
    NO_PARITY
  };
  struct Configuration
  {
    uint32_t baudrate;
    Parity parity;
    int data_bits;
    int stop_bits;
  };
  struct ReadBuffer
  {
    void* data;
    size_t size;
  };
  struct WriteBuffer
  {
    const void* data;
    size_t size;
  };
  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;
  ErrorCode next_write_result = ErrorCode::OK;
  size_t write_calls = 0;
  size_t cursor = 0;
  size_t failed_reads = 0;
  void SetConfig(Configuration) {}
  ErrorCode Read(ReadBuffer buffer, ReadOperation&)
  {
    if (buffer.size > rx.size() - cursor)
    {
      // Fail quickly if a test accidentally asks the blocking header scanner to
      // wait forever after the input has been exhausted.
      assert(++failed_reads < 3);
      return ErrorCode::TIMEOUT;
    }
    std::memcpy(buffer.data, rx.data() + cursor, buffer.size);
    cursor += buffer.size;
    return ErrorCode::OK;
  }
  ErrorCode Write(WriteBuffer buffer, WriteOperation&)
  {
    const auto* bytes = static_cast<const uint8_t*>(buffer.data);
    tx.assign(bytes, bytes + buffer.size);
    ++write_calls;
    const auto RESULT = next_write_result;
    next_write_result = ErrorCode::OK;
    return RESULT;
  }
};
}  // namespace LibXR
