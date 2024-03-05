#include "network_interface.hh"

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "parser.hh"

#include <iostream>
#include <utility>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    const auto &item = _arp_table.find(next_hop_ip);
    if (item != _arp_table.end()) {
        EthernetFrame frame;
        frame.header() = {item->second.eth, _ethernet_address, EthernetHeader::TYPE_IPv4};
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    } else {
        // 没有找到，发送 arp request,之前发送过了且在 5s 内不发送,即不等待队列中
        if (find_if(_wait_ipdata.begin(), _wait_ipdata.end(), [next_hop](const auto &tuple) {
                return std::get<0>(tuple).ipv4_numeric() == next_hop.ipv4_numeric();
            }) == _wait_ipdata.end()) {
            ARPMessage request;
            request.opcode = ARPMessage::OPCODE_REQUEST;
            request.sender_ethernet_address = _ethernet_address;
            request.sender_ip_address = _ip_address.ipv4_numeric();
            request.target_ethernet_address = {};
            request.target_ip_address = next_hop.ipv4_numeric();

            EthernetFrame frame;
            frame.header() = {ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
            frame.payload() = request.serialize();
            _frames_out.push(frame);
            _wait_ipdata.push_back({next_hop, 0, dgram});
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address)
        return nullopt;

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ipdata;
        if (ipdata.parse(frame.payload()) != ParseResult::NoError) {
            return nullopt;
        }
        return ipdata;
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) != ParseResult::NoError) {
            return nullopt;
        }

        bool valid_request =
            arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == _ip_address.ipv4_numeric();
        bool valid_replay =
            arp_msg.opcode == ARPMessage::OPCODE_REPLY && arp_msg.target_ethernet_address == _ethernet_address;

        if (valid_request) {
            ARPMessage replay;
            replay.opcode = ARPMessage::OPCODE_REPLY;
            replay.sender_ethernet_address = _ethernet_address;
            replay.sender_ip_address = _ip_address.ipv4_numeric();
            replay.target_ethernet_address = arp_msg.sender_ethernet_address;
            replay.target_ip_address = arp_msg.sender_ip_address;

            EthernetFrame replay_frame;
            replay_frame.header() = {arp_msg.sender_ethernet_address, _ethernet_address, EthernetHeader::TYPE_ARP};
            replay_frame.payload() = replay.serialize();
            _frames_out.push(replay_frame);
        }
        if (valid_replay || valid_request) {
            // 先更新 arp 表
            _arp_table.insert({arp_msg.sender_ip_address, {arp_msg.sender_ethernet_address, 0}});

            auto item = find_if(_wait_ipdata.begin(), _wait_ipdata.end(), [arp_msg](const auto &tuple) {
                return std::get<0>(tuple).ipv4_numeric() == arp_msg.sender_ip_address;
            });
            if (item != _wait_ipdata.end()) {
                send_datagram(std::get<2>(*item), std::get<0>(*item));
                _wait_ipdata.erase(item);
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto iter = _wait_ipdata.begin(); iter != _wait_ipdata.end(); iter++) {
        if (std::get<1>(*iter) + ms_since_last_tick >= _retry_interval) {
            iter = _wait_ipdata.erase(iter);
        } else {
            std::get<1>(*iter) += ms_since_last_tick;
        }
    }
    for (auto iter = _arp_table.begin(); iter != _arp_table.end();) {
        if (iter->second.ttl + ms_since_last_tick >= _ttl) {
            iter = _arp_table.erase(iter);
        } else {
            iter->second.ttl += ms_since_last_tick;
            ++iter;
        }
    }
}
