#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

class Timer
{
private:
  uint32_t _time_out = 0;
  uint32_t _time_cur = 0;
  bool open = false;

public:
  Timer() = default;
  Timer( const uint32_t time_out ) : _time_out( time_out ) {};
  void stop() { open = false; }
  void set_time_out( const uint32_t time_out ) { _time_out = time_out; }
  uint32_t get_time_out() { return _time_out; }
  void restart()
  {
    open = true;
    _time_cur = 0;
  }
  void tick( const size_t ms_since_last_tick ) { _time_cur += ms_since_last_tick; }
  bool check_time_out() { return open && _time_cur >= _time_out; }
  bool is_open() { return open; }
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), _timer( initial_RTO_ms_ )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  // 补充成员变量
  Timer _timer;                                                          // 定时器
  uint64_t _consecutive_retransmissions = 0;                             // 连续重传次数
  uint64_t _sequence_numbers_in_flight = 0;                              // 未被确认的字节大小
  uint32_t _windows_size = 1;                                            // 接收方窗口大小
  uint64_t _next_seqno { 0 };                                            // 下一个报文的序列号
  std::queue<std::pair<uint64_t, TCPSenderMessage>> _outstanding_seg {}; //已经发送但是没有被确认的segment队列
  bool SYN = false, FIN = false;
};
