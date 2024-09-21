#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  } 
  if ( open == false ) {
    if ( !message.SYN )
      return;
    else {
      open = true;
      isn = message.seqno;
    }
  }
  Wrap32 seqno = message.seqno;
  uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  uint64_t ab_seqno = seqno.unwrap( isn, checkpoint );
  uint64_t index = message.SYN ? ab_seqno : ab_seqno - 1;
  reassembler_.insert( index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage message;
  if ( !open ) {
    message.ackno = nullopt;
  } else {
    uint64_t ab_seqno = reassembler_.writer().bytes_pushed() + 1 + ( reassembler_.writer().is_closed() ? 1 : 0 );
    message.ackno = Wrap32::wrap( ab_seqno, isn );
  }
  message.RST = reassembler_.reader().has_error();
  message.window_size = _capacity - reassembler_.reader().bytes_buffered();
  return message;
}
