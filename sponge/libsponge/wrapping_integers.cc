#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + n; }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // 这里实际上就是把一个 32 位循环数转成一个 64 位真实值，checkout 是上一次的值
    // 两个 seq 之间不会超过 1<<32，除非发生了错误
    // 注意：最终结果是离 checkpoint 最近的 seq，seq 可能比 checkpoint 小
    /*
    // 找到n和checkpoint之间的最小步数
    int32_t min_step = n - wrap(checkpoint, isn);
    // 将步数加到checkpoint上
    int64_t ret = checkpoint + min_step;
    // 如果反着走的话要加2^32
    return ret >= 0 ? static_cast<uint64_t>(ret) : ret + (1ul << 32);
    */
    uint32_t offset = n - isn;
    // 先靠近 checkpoint
    uint64_t res = offset + ((checkpoint >> 32) << 32);
    // 判断是往前还是往后
    if (abs(int64_t(res + (1ul << 32) - checkpoint)) < abs(int64_t(res - checkpoint)))
        res += (1ul << 32);
    if (res >= (1ul << 32) && abs(int64_t(res - (1ul << 32) - checkpoint)) < abs(int64_t(res - checkpoint)))
        res -= (1ul << 32);
    return res;
}
