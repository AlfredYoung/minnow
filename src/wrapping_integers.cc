#include "wrapping_integers.hh"
#include <cmath>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { zero_point.raw_value_ + static_cast<uint32_t>( n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint32_t low32_bits = this->raw_value_ - zero_point.raw_value_;
  uint64_t high32_bits1 = ( checkpoint + ( 1 << 31 ) ) & 0xFFFFFFFF00000000;
  uint64_t high32_bits2 = ( checkpoint - ( 1 << 31 ) ) & 0xFFFFFFFF00000000;
  uint64_t aseqno1 = low32_bits | high32_bits1;
  uint64_t aseqno2 = low32_bits | high32_bits2;
  if ( max( aseqno1, checkpoint ) - min( aseqno1, checkpoint )
       < max( aseqno2, checkpoint ) - min( aseqno2, checkpoint ) )
    return aseqno1;
  return aseqno2;
}
