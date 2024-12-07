#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // 返回未被确认的字节大小
  return _sequence_numbers_in_flight;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // 返回连续重传的次数
  return _consecutive_retransmissions;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // 首先判断窗口大小
  uint32_t windows_size = max( _windows_size, (uint32_t)1 );
  // 接收方有足够大小的窗口才可以开始发送
  while ( _sequence_numbers_in_flight < windows_size ) {
    TCPSenderMessage msg;
    // SYN = false 说明还没有建立连接，先建立连接
    if ( !SYN ) {
      msg.SYN = true;
      SYN = true;
    }
    // 对于 payload_size的大小：
    // 首先不能超过MAX_PAYLOAD_SIZE
    // 其次接收方窗口必须有足够大小放下，用windows_size减去未被确认的字节数再减去SYN所占用的
    // 最后是不能超过input_中所存储的有效字节
    auto payload_size
      = min( TCPConfig::MAX_PAYLOAD_SIZE,
             min( windows_size - _sequence_numbers_in_flight - msg.SYN, input_.writer().bytes_pushed() ) );
    string payload;
    // 读取payload_size个字节，放入payload中
    for ( uint64_t i = 0; i < payload_size; ++i ) {
      payload += input_.reader().peek();
      input_.reader().pop( 1 );
    }
    msg.payload = move( payload );
    // 如果已经读取完input_中的数据，说明所有要发送的数据都已经发送，那么就将msg的FIN置为true
    // 需要注意的是要判断FIN是否能放下，因为之前选择payload的字段时没有考虑FIN，意味着如果SYN和payload就占满了windows_size，那么就不能传输FIN了
    if ( !FIN && input_.reader().is_finished()
         && _sequence_numbers_in_flight + msg.sequence_length() < windows_size ) {
      msg.FIN = true;
      FIN = true;
    }
    int msg_len = msg.sequence_length();
    if ( msg.sequence_length() == 0 )
      break;
    // 从absolute_seqno转换到seqno
    msg.seqno = Wrap32::wrap( _next_seqno, isn_ );
    msg.RST = input_.has_error();
    transmit( msg );
    // 发送数据后，如果没有打开计时器就打开计时器
    if ( !_timer.is_open() ) {
      _timer.restart();
    }
    // 将报文暂存在队列中，以便超时重传
    _outstanding_seg.push( { _next_seqno, move( msg ) } );

    // 更新未接收到的字节数和下一个报文的seqno
    _sequence_numbers_in_flight += msg_len;
    _next_seqno += msg_len;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // 空的message只需要有RST和seqno，不需要payload
  TCPSenderMessage msg;
  msg.RST = input_.has_error();
  msg.seqno = Wrap32::wrap( _next_seqno, isn_ );
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // 如果接收到的报文没有ackno，就只更新_windows_size
  if ( !msg.ackno.has_value() ) {
    if ( msg.RST )
      input_.set_error();
    _windows_size = msg.window_size;
    return;
  }
  // 接收到的报文有RST，那就置错
  if ( msg.RST )
    input_.set_error();
  // 将报文的ackno转成absolute_ackno
  uint64_t abs_ackno = msg.ackno.value().unwrap( isn_, _next_seqno );
  // 如果发现abs_ackno比_next_seqno还大，显然这是不合理的，就直接返回；
  if ( abs_ackno > _next_seqno )
    return;
  bool receive = false;
  // 将ackno之前的所有未确认的报文确认，并从队列中删除
  while ( !_outstanding_seg.empty() ) {
    auto& [seqno, outstanding_msg] = _outstanding_seg.front();
    if ( seqno + outstanding_msg.sequence_length() - 1 < abs_ackno ) {
      receive = true;
      _sequence_numbers_in_flight -= outstanding_msg.sequence_length();
      _outstanding_seg.pop();
    } else
      break;
  }
  // 如果有报文确认，那么就将连续重传次数清零，并且重置计时器
  if ( receive ) {
    _consecutive_retransmissions = 0;
    _timer.set_time_out( initial_RTO_ms_ );
    _timer.restart();
  }
  // 如果所有报文都被确认，那么计时器中止
  if ( _sequence_numbers_in_flight == 0 ) {
    _timer.stop();
  }
  _windows_size = msg.window_size;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  _timer.tick( ms_since_last_tick );
  // 超时重传，并将RTO乘2
  if ( _timer.check_time_out() ) {
    transmit( _outstanding_seg.front().second );
    if ( _windows_size > 0 ) {
      ++_consecutive_retransmissions;
      _timer.set_time_out( _timer.get_time_out() * 2 );
    }
    _timer.restart();
  }
}
