#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

// 自己实现的send函数用来简化后面的代码
template <typename DatagramType>
void NetworkInterface::send(const EthernetAddress &dst, const EthernetAddress &src, const uint16_t &type, const DatagramType& dgram) const
{
  EthernetFrame eth;
  // 补充以太网帧的头部
  eth.header.dst = dst;
  eth.header.src = src;
  eth.header.type = type;

  // 补充以太网帧的数据载荷
  Serializer serializer;
  dgram.serialize(serializer);
  eth.payload = serializer.output();

  transmit(eth);
}

void NetworkInterface::send_ip(const EthernetAddress &dst, const EthernetAddress &src, const IPv4Datagram &dgram) const {
  send(dst, src, EthernetHeader::TYPE_IPv4, dgram);
}

void NetworkInterface::send_arp(const EthernetAddress &dst, const EthernetAddress &src, const ARPMessage &arp) const {
  send(dst, src, EthernetHeader::TYPE_ARP, arp);
}


//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  (void)dgram;
  (void)next_hop;
  // 如果arp_table有目的eth 地址
  if (arp_table.find(next_hop.ipv4_numeric()) != arp_table.end()) {
    // EthernetFrame eth;
    auto entry = arp_table[next_hop.ipv4_numeric()];

    send_ip(entry.eth_addr, ethernet_address_, dgram);
  }

  // 否则发送ARP报文查询地址
  else {
    // 如果在等待队列中，没有出现查询该IP对应的地址，那么就发送ARP报文
    if (waiting_datagram.find(next_hop.ipv4_numeric()) == waiting_datagram.end()) {
      // 首先创建一个ARP报文
      ARPMessage arp_msg;
      arp_msg.opcode = arp_msg.OPCODE_REQUEST;
      arp_msg.sender_ethernet_address = ethernet_address_;
      arp_msg.target_ethernet_address = {};
      arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
      arp_msg.target_ip_address = next_hop.ipv4_numeric();

      // 然后创建一个 eth报文

      send_arp(ETHERNET_BROADCAST, ethernet_address_, arp_msg);

      response_time[next_hop.ipv4_numeric()] = arp_response_ttl;
    }
    waiting_datagram[next_hop.ipv4_numeric()].emplace_back(next_hop, dgram);
  }

}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  // 如果不是广播帧，或者不是目的地址是本机，忽略
  if (frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_) {
    return ;
  }
  // IP报文直接加入队列中
  else if (frame.header.type == frame.header.TYPE_IPv4) {
    Parser parser(frame.payload);
    InternetDatagram ip;
    ip.parse(parser);
    datagrams_received_.push(ip);
  }
  // 如果是ARP协议，要更新ARP表
  else if (frame.header.type == frame.header.TYPE_ARP) {
    ARPMessage arp_msg;
    Parser parser(frame.payload);
    arp_msg.parse(parser);

    uint32_t ip = ip_address_.ipv4_numeric();
    uint32_t src_ip = arp_msg.sender_ip_address;
    // 如果是发送给本机的ARP请求报文，要应答这个请求
    if (arp_msg.opcode == arp_msg.OPCODE_REQUEST && arp_msg.target_ip_address == ip) {
      ARPMessage arp_reply;
      arp_reply.opcode = arp_reply.OPCODE_REPLY;
      arp_reply.sender_ip_address = ip;
      arp_reply.sender_ethernet_address = ethernet_address_;
      arp_reply.target_ip_address = src_ip;
      arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;

      send_arp(arp_msg.sender_ethernet_address, ethernet_address_, arp_reply);
    } 
    // 其余就更新IP和MAC地址对应关系
    arp_table[src_ip] = {arp_msg.sender_ethernet_address, arp_entry_ttl};

    // 如果这个ip地址有没有发送完的数据报文
    auto it = waiting_datagram.find(src_ip);
    if (it != waiting_datagram.end()) {
      
      for (const auto &[next_hop, dgram] : it->second) {
        // send_datagram(dgram, next_hop);
        send_ip(arp_msg.sender_ethernet_address, ethernet_address_, dgram);
      }
      waiting_datagram.erase(it);
    }

  }

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  for (auto it = arp_table.begin(); it != arp_table.end();) {
    if (it->second.ttl <= ms_since_last_tick) {
      it = arp_table.erase(it);
    }
    else {
      it->second.ttl -= ms_since_last_tick;
      it = next(it);
    }
  }

  for (auto it = response_time.begin(); it != response_time.end();) {
    if (it->second <= ms_since_last_tick) {
      auto it2 = waiting_datagram.find(it->first);
      if (it2 != waiting_datagram.end()) {
        waiting_datagram.erase(it2);
      }

      it = response_time.erase(it);
    }
    else {
      it->second -= ms_since_last_tick;
      it = next(it);
    }
  }
}
