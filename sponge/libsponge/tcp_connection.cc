#include "tcp_connection.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "tcp_state.hh"

#include <cassert>
#include <cstdio>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active)
        return;

    _time_since_last_segment_received = 0;

    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }

    // 接受 segment
    _receiver.segment_received(seg);

    // listen get syn, 发送 syn+ack
    // 这里 receiver 为什么是 syn_recv 而不是 Listen？因为前面 receiver 已经receive了segment，因此进入了该状态
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // cout << "send syn_ack " << endl;
        connect();
        return;
    }
    // receiver 收到了 syn 的情况下收到 ack，通知 sender ackno/window 变换
    if (_receiver.ackno().has_value() && seg.header().ack) {
        bool ack_success = _sender.ack_received(seg.header().ackno, seg.header().win);
        // printf("ack_success:%u\n", ack_success);
        if (seg.length_in_sequence_space() && !_segments_out.empty()) {
            _push_segment_with_ack_and_win();
            return;
        }
        if (!ack_success) {
            _sender.send_empty_segment();
        }
    }

    // 如果数据包有数据（没数据不需要回复，防止无限 ack），且有 seq num，需要回复一个 ack 包
    // 这里 seq_num 可能为 0
    if (seg.length_in_sequence_space())
        _sender.send_empty_segment();

    // keep-alive，这里
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        seg.header().seqno != _receiver.ackno().value()) {
        _sender.send_empty_segment();
    }

    // lask_ack：收到对方发来的 fin 且自己发送了 fin
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_SENT) {
    }

    // 给待发送的包添加 ack和win
    _push_segment_with_ack_and_win();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    _push_segment_with_ack_and_win();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active)
        return;

    _time_since_last_segment_received += ms_since_last_tick;

    _sender.tick(ms_since_last_tick);

    // 超过了最大重传次数，要发送 rst
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        TCPSegment _seg;
        _seg.header().rst = true;
        _segments_out.push(_seg);
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }
    _push_segment_with_ack_and_win();
}

// 流结束，发送 fin 报文
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _push_segment_with_ack_and_win();
}

void TCPConnection::connect() {
    // syn_send
    _sender.fill_window();
    _push_segment_with_ack_and_win();
    _active = true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_push_segment_with_ack_and_win() {
    while (!_sender.segments_out().empty()) {
        cout << "push_segment_with_ack_and_win" << endl;
        TCPSegment seg = _sender.segments_out().front();
        // 这里是的 ackno() 返回的是 receiver 接受且 reassamble 好的数据量
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        segments_out().push(seg);
        _sender.segments_out().pop();
    }
}
