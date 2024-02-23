#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!_has_syn_flag) {
        // syn 之前的包都不要
        if (seg.header().syn) {
            _isn = seg.header().seqno;
            _has_syn_flag = true;
        } else {
            return false;
        }
    } else {
        // reject second syn
        if (seg.header().syn)
            return false;
    }
    // if (seg.header().syn) {
    //     if (!_has_syn_flag) {
    //         _isn = seg.header().seqno;
    //         _has_syn_flag = true;
    //     } else
    //         return false;
    // }
    // 期望收到的下一个 seq_num
    uint64_t abs_ackno = _reassembler.stream_out().bytes_written() + 1;  // syn 也占用一位
    // 当前 seg 的 absolute seq num
    uint64_t curr_abs_seqno = unwrap(seg.header().seqno, _isn, abs_ackno);

    // 已经收到了 fin，再次收到，返回 false
    if (seg.header().fin) {
        if (_has_fin_flag)
            return false;
        else
            _has_fin_flag = true;
    }

    // 判断 segment 是否在 window 中，即是否有数据可以用于 reassembler
    if (!seg.header().syn) {
        // 1. seg 重复
        if (seg.length_in_sequence_space() + curr_abs_seqno <= abs_ackno)
            return false;
        // 2. seg 超出了 window 范围
        else if (curr_abs_seqno >= abs_ackno + window_size())
            return false;
    }

    // SYN 包中的 payload 不能被丢弃
    uint64_t stream_index = curr_abs_seqno - 1 + (seg.header().syn);
    _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    // 判断是否是在 LISTEN 状态
    if (!_has_syn_flag)
        return nullopt;
    // 如果不在 LISTEN 状态，则 ackno 还需要加上一个 SYN 标志的长度
    uint64_t abs_ack_no = _reassembler.stream_out().bytes_written() + 1;
    // 如果当前处于 FIN_RECV 状态，则还需要加上 FIN 标志长度
    if (_reassembler.stream_out().input_ended())
        ++abs_ack_no;
    return WrappingInt32(_isn) + abs_ack_no;
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
