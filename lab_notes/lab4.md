1. sender 和 connection 中都有 segment_out，根据 lab 文档的说法需要将 sender 中的 segment 放到 connection 中的 outqueue

FSM_LISTEN = SENDER_CLOSED && RECEIVER_LISTEN
FSM_SYN_SENT = SENDER_SYN_SENT && RECEIVER_LISTEN
FSM_TIME_WAIT = SENDER_FIN_ACKED && RECEIVER_FIN_RECV

如果 FSM 处于LISTEN 状态时，只接收带有 SYN 的报文段，并且如果收到 SYN 要更新 receiver 和发送 ack segment，即 _receiver.segment_received + _sender.fill_window

什么时候发送空包
1. 有 seq num
2. 