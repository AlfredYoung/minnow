#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // Your code here.
  uint64_t start = max( _cur_index, first_index );
  uint64_t end
    = min( first_index + data.size(), min( _cur_index + output_.writer().available_capacity(), _eof_index ) );
  if ( is_last_substring ) {
    _eof_index = min( _eof_index, first_index + data.size() );
  }

  for ( uint64_t i = start, j = start - first_index; i < end; i++, j++ ) {
    auto& t = _stream[i % _capacity];
    if ( t.second == true ) {
      if ( t.first != data[j] )
        throw __throw_runtime_error;
    } else {
      t = make_pair( data[j], true );
      ++_unassembled_bytes_cnt;
    }
  }

  string str;
  while ( _cur_index < _eof_index && _stream[_cur_index % _capacity].second == true ) {
    str.push_back( _stream[_cur_index % _capacity].first );
    _stream[_cur_index % _capacity] = { 0, false };
    --_unassembled_bytes_cnt;
    ++_cur_index;
  }

  output_.writer().push( str );

  if ( _cur_index == _eof_index )
    output_.writer().close();
}
// 返回还有多少字节存储在Reassembler中
uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return _unassembled_bytes_cnt;
}
