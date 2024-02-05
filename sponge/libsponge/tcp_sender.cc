#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout(retx_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return flight_size; }

void TCPSender::fill_window() {
    if (_fin)
        return;

    TCPSegment segment;

    // 还没有发送过 syn，这里是否要考虑重传？
    if (!_syn) {
        segment.header().syn = true;
        _syn = true;
        send_segment(segment);
        return;
    }

    // receiver window size 为0时当做1
    uint16_t receive_wsize = receive_win_size == 0 ? 1 : receive_win_size;

    if (_stream.eof() && receive_ackno + receive_wsize > _next_seqno) {
        segment.header().fin = true;
        _fin = true;
        send_segment(segment);
        return;
    }

    // 发送 segment 直到对方 win_size 满或者没有数据了
    // 这里之所以要满足 receive_wsize > flight_size 是因为 receiver 的 window 是用 ackno + receive_win_size 框出来的，
    // flight_size 是还没有确认的部分，实际上已经在接收窗口里了，flight_queue + 新发送的 seg 要在这个范围之内
    while (receive_wsize > flight_size && !_stream.buffer_empty()) {
        uint16_t seg_len = min(TCPConfig::MAX_PAYLOAD_SIZE, receive_wsize - flight_size);
        // 装入 payload
        string payload = _stream.read(seg_len);
        segment.payload() = Buffer(move(payload));
        if (!_fin && _stream.eof() && payload.size() + flight_size < receive_wsize) {
            segment.header().fin = true;
            _fin = true;
        }

        send_segment(segment);
    }
}

void TCPSender::send_segment(TCPSegment segment) {
    // 转换成 TCP 中的原始 seqno，syn/ack 都各占据一位
    segment.header().seqno = wrap(_next_seqno, _isn);
    // 发送
    _segments_out.push(segment);
    flight_queue.push(segment);
    flight_size += segment.length_in_sequence_space();
    _next_seqno += segment.length_in_sequence_space();
    if (!timer_running) {
        // 发送包后开启重传计时
        timer_running = true;
        _consecutive_retransmissions_count = 0;
    }
    // std::cout << "seqno: " << segment.header().seqno;
    // std::cout << "payload " << segment.payload().str();
    // std::cout << " seg_length " << segment.length_in_sequence_space() << "\n";
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
// 这里 ackno 是窗口的左边界，window_size + ackno 是窗口的右边界，可以发送的 seg 就在这段 window 之内
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 先转换为相对 ack
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    // 返回了比接下来要发送的更大的 ack，错误
    if (abs_ackno > _next_seqno)
        return false;
    // 过滤重复 ack
    // 这里没有考虑 ack 重复且 winsize 为 0 的情况，这种不能 double RTO
    if (receive_ackno <= abs_ackno) {
        receive_ackno = abs_ackno;
        receive_win_size = window_size;
    }
    // 处理已确认报文
    while (!flight_queue.empty()) {
        TCPSegment segment = flight_queue.front();
        // 这里是小于等于：比如当前发送了 syn，收到了 syn_ack，那么 segment + len = 1 receive_ackno = 1
        if (unwrap(segment.header().seqno, _isn, _next_seqno) + segment.length_in_sequence_space() <= receive_ackno) {
            flight_size -= segment.length_in_sequence_space();
            flight_queue.pop();
            // 收到一个完整的包，清空超时时间
            _retransmission_timeout = _initial_retransmission_timeout;
            passed_time = 0;
        } else {
            break;
        }
    }
    // 收到了 ACK，重置 RTO
    _consecutive_retransmissions_count = 0;

    // 没有待确认的包，停止超时机制
    if (flight_queue.empty())
        timer_running = false;
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
// ms_since_last_tick 是该函数距离上次被调用过去的时间
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!timer_running)
        return;

    passed_time += ms_since_last_tick;

    if (passed_time >= _retransmission_timeout && !flight_queue.empty()) {
        TCPSegment segment = flight_queue.front();
        _segments_out.push(segment);
        passed_time = 0;
        // syn 包发出去没有收到过 ack,但是也要考虑 RTO*2
        if (receive_win_size > 0 || segment.header().syn) {
            _consecutive_retransmissions_count++;
            _retransmission_timeout *= 2;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

// 用于发送 syn ack 这样的空包
void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}
