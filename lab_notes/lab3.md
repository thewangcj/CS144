1. RTO 每超时一次就翻倍，达到一定次数（连续重发数）后认为此连接已经断开了
2. 需要一个数据结构保存已发送的 segment，方便重传时使用（FIFO 队列？）
3. 需要实现一个软定时器，不能使用系统的 time/clock 等方法
4. 当收到 ackno 时
    1. 重置 RTO
    2. 重置连续重发数
5. fill_window：
TCPSender 从 ByteStream 中读取数据，并以 TCPSegement 的形式发送，尽可能地填充接收者的窗口。但每个TCP段的大小不得超过 TCPConfig::MAX PAYLOAD SIZE。
    若接收方的 Windows size 为 0，则发送方将按照接收方 window size 为 1 的情况进行处理，持续发包。
    因为虽然此时发送方发送的数据包可能会被接收方拒绝，但接收方可以在反向发送 ack 包时，将自己最新的 window size 返回给发送者。否则若双方停止了通信，那么当接收方的 window size 变大后，发送方仍然无法得知接收方可接受的字节数量。
    若远程没有 ack 这个在 window size 为 0 的情况下发送的一字节数据包，那么发送者重传时不要将 RTO 乘 2。这是因为将 RTO 翻倍的目的是为了避免网络拥堵，但此时的数据包丢弃并不是因为网络拥堵的问题，而是远程放不下了。
    length_in_sequence_space 中计算的结果包括 SYN 和 FLN，每一个各占 1 个 sequence number

- 开始时 TCPSender 认为 receiver 的 window size 为1
- 返回的 ackno 一定是某一个 segment 的边界，即不会出现 segment 部分确认的情况
6. fin
发送 Fin 包的条件
    - 从来没发送过 FIN。这是为了防止发送方在发送 FIN 包并接收到 FIN ack 包之后，循环用 FIN 包填充发送窗口的情况。
    - 输入字节流处于 EOF
    - window 减去 payload 大小后，仍然可以存放下 FIN