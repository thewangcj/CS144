1. 之前代码中表示 index 是使用 64 位 int 表示,而 tcp header 为了节省开销考虑使用的是 32 位
    - index 32位可能导致环绕情况出现,即 int 越界然后重新从0开始
    - 之前默认 index 从 0 开始,实际上现实中 index 是随机数, index 表示的 SYN,所以第一个数据 index + 1
    - SYN 和 FIN 虽然占用了 sequence number,但是实际上没有数据
2. syn&ack
    - syn: seq = 2387613953 ack = 0
    - syn_ack: seq = 3344080264 ack = 2387613954
    - ack: seq = 2387613954 ack = 3344080265