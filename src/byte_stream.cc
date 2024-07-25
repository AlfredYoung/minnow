#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}
// 返回stream是否关闭
bool Writer::is_closed() const
{
  // Has the stream been closed?
  // Your code here.
  return closed;
}
// Writer将数据放入stream中
void Writer::push( string data )
{
  // Your code here.
  // (void)data;
  uint64_t len = data.length();
  if ( len > capacity_ - buffer.size() ) {
    len = capacity_ - buffer.size();
  }
  for ( uint64_t i = 0; i < len; ++i ) {
    buffer.push_back( data[i] );
    total_bytes_pushed++;
  }
  return;
}
// 关闭stream
void Writer::close()
{
  closed = true;
  // Your code here.
}
// 返回capacity - 已经用过的stream大小
uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - buffer.size();
}
// 返回总的push进stream的字节数
uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return total_bytes_pushed;
}

// 返回stream是否关闭或者pop完所有的元素
bool Reader::is_finished() const
{
  // Your code here.
  return closed && buffer.empty();
}
// 返回总的pop的stream的字节数
uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return total_bytes_poped;
}
// Peek at the next bytes in the buffer
// string_view: C++ 17引入，在不拷贝的情况下读取、查看和操作字符串
// peek函数作用：
string_view Reader::peek() const
{
  // Your code here.
  if ( !buffer.empty() ) {
    return std::string_view( &buffer.front(), 1 ); // 返回deque的front元素的string_view
  }
  return std::string_view(); // 返回一个默认构造的string_view（空的）
}
//
void Reader::pop( uint64_t len )
{
  // Your code here.
  if ( buffer.size() < len ) {
    len = buffer.size();
  }
  for ( uint64_t i = 0; i < len; ++i ) {
    buffer.pop_front();
    total_bytes_poped++;
  }
}
// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return buffer.size();
}