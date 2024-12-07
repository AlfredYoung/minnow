#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return _sequence_numbers_in_flight;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.

  return _consecutive_retransmissions;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  uint32_t windows_size = max( _windows_size, (uint32_t)1 );
  while ( _sequence_numbers_in_flight < windows_size ) {
    TCPSenderMessage msg;
    if ( !SYN ) {
      msg.SYN = true;
      SYN = true;
    }
    auto payload_size
      = min( TCPConfig::MAX_PAYLOAD_SIZE,
             min( windows_size - _sequence_numbers_in_flight - msg.SYN, input_.writer().bytes_pushed() ) );
    string payload;
    for ( uint64_t i = 0; i < payload_size; ++i ) {
      payload += input_.reader().peek();
      input_.reader().pop( 1 );
    }
    msg.payload = move( payload );
    if ( !FIN && input_.reader().is_finished()
         && _sequence_numbers_in_flight + msg.sequence_length() < windows_size ) {
      msg.FIN = true;
      FIN = true;
    }
    int msg_len = msg.sequence_length();
    if ( msg.sequence_length() == 0 )
      break;
    msg.seqno = Wrap32::wrap( _next_seqno, isn_ );
    msg.RST = input_.has_error();
    transmit( msg );
    if ( !_timer.is_open() ) {
      _timer.restart();
    }
    _outstanding_seg.push( { _next_seqno, move( msg ) } );

    _sequence_numbers_in_flight += msg_len;
    _next_seqno += msg_len;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  TCPSenderMessage msg;
  msg.RST = input_.has_error();
  msg.seqno = Wrap32::wrap( _next_seqno, isn_ );
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  if ( !msg.ackno.has_value() ) {
    if ( msg.RST )
      input_.set_error();
    _windows_size = msg.window_size;
    return;
  }
  if ( msg.RST )
    input_.set_error();
  uint64_t abs_ackno = msg.ackno.value().unwrap( isn_, _next_seqno );
  if ( abs_ackno > _next_seqno )
    return;
  bool receive = false;
  while ( !_outstanding_seg.empty() ) {
    auto& [seqno, outstanding_msg] = _outstanding_seg.front();
    if ( seqno + outstanding_msg.sequence_length() - 1 < abs_ackno ) {
      receive = true;
      _sequence_numbers_in_flight -= outstanding_msg.sequence_length();
      _outstanding_seg.pop();
    } else
      break;
  }
  if ( receive ) {
    _consecutive_retransmissions = 0;
    _timer.set_time_out( initial_RTO_ms_ );
    _timer.restart();
  }
  if ( _sequence_numbers_in_flight == 0 ) {
    _timer.stop();
  }
  _windows_size = msg.window_size;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  _timer.tick( ms_since_last_tick );
  if ( _timer.check_time_out() ) {
    transmit( _outstanding_seg.front().second );
    if ( _windows_size > 0 ) {
      ++_consecutive_retransmissions;
      _timer.set_time_out( _timer.get_time_out() * 2 );
    }
    _timer.restart();
  }
}
