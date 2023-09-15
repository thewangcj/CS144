# 实验要求
1. 实现一个流重组器
2. 需要拼接的子串是连续的，即子串内不存在空洞
3. 子串之间可能交叉或者完全重叠
4. 子串可能是一个只包含EOF标志的空串

# 思路
1. 难点在于如何维护 reassembler 总已 assemble 的部分和 unassemble 部分，有点类似于内存管理(即一块连续内存中那些是使用的那些是未使用的)
2. first_unread = output.read()
3. first_unassembled = output.written()
4. first_unacceptable = first_unread + capacity
5. 如果使用 `<index, string>` map，那么需要考虑新加入的重叠子串的截断
6. 如果使用数组+标志位数组的方式实现，效率可能比较高，因为不需要各种字符串操作，难点在于如何合并子串和已有数据