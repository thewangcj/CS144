#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
// 先合并再输出
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t len = data.length();
    // 容量满了，跳过
    if (index > _capacity + _output.bytes_read())
        return;
    if (eof)
        has_eof = eof;
    if (_buffer.empty() && data.empty() && has_eof) {
        _output.end_input();
        return;
    }
    // substring 整个已经输出过了，跳过
    if (index + len <= _first_unassemble)
        return;
    // 部分substring或刚好接上，直接输出
    if (index <= _first_unassemble) {
        size_t ret = _output.write(data.substr(_first_unassemble - index));
        _first_unassemble += ret;
    }

    // 将 substring 合并到 buffer 中
    /**
     * 一段一段合并需要考虑的情况太多
    // 1. substirng 和 buffer 在前半部重叠或完全不重叠
    if (index < _next_assemble) {
        // 完全不重叠，直接填充并合并
        if (len + index < _next_assemble) {
            size_t offset = _next_assemble - len - index;
            for (size_t i = 0; i < offset; i++) {
                _buffer.push_front(0);
                _wflags.push_back(false);
            }
            for (size_t i = 0; i < len; i++) {
                _buffer.push_front(data[len - i]);
                _wflags.push_front(true);
            }
        }
        // 部分重叠
        else {
            size_t offset = len + index - _next_assemble;
            for (size_t i = 0; i < offset; i++) {
                if (_wflags[i])
                    continue;
                else {
                    _buffer[i] = data[i + (len - offset)];
                    _wflags[i] = true;
                }
            }
            for (size_t i = 0; i < len - offset; i++) {
                _buffer.push_front(data[index + offset - i]);
                _wflags.push_front(true);
            }
        }
        _next_assemble = index;
    }

    // 2. substring 和 buffer 完全重叠
    if (index >= _next_assemble && index + len <= _next_assemble + _buffer.size()) {
        size_t offset = index - _next_assemble;
        for (size_t i = 0; i < len; i++) {
            if (_wflags[i + offset] == false) {
                _buffer[i + offset] = data[i];
                _wflags[i + offset] = true;
            }
        }
    }

    // 3. substirng 和 buffer 在后半部重叠或完全不重叠
    if (index >= _next_assemble && index + len > _next_assemble + _buffer.size()) {
        // 部分重叠
        if (index <= _next_assemble + _buffer.size()) {
            for (size_t i = index; i < _next_assemble + _buffer.size(); i++) {
                _buffer[i - _next_assemble] = data[i - index];
                _wflags[i - _next_assemble] = true;
            }
            for (size_t i = _next_assemble + _buffer.size(); i < index + len; i++) {
                _buffer.push_back(data[i - _next_assemble - _buffer.size()]);
                _wflags.push_back(true);
            }

        }
        // 完全不重叠
        else {
            for (size_t i = _next_assemble + _buffer.size(); i < index; i++) {
                _buffer.push_front(0);
                _wflags.push_back(false);
            }
            for (size_t i = 0; i < len; i++) {
                _buffer.push_front(data[i]);
                _wflags.push_front(true);
            }
        }
    }
    **/

    if (index > _first_unassemble) {
        // 1. 计算合并后的 buffer 应该多大
        size_t min_index = min(index, _next_assemble);
        size_t max_index = max(index + len, _next_assemble + _buffer.size());
        // 2. 将 buffer 向两边延展
        if (min_index < _next_assemble) {
            // 向左扩充
            for (size_t i = 0; i < _next_assemble - min_index; i++) {
                _buffer.push_front(0);
                _wflags.push_front(false);
            }
        }
        if (_next_assemble + _buffer.size() < max_index) {
            // 向右扩充
            size_t r_limit = max_index - _next_assemble - _buffer.size();
            for (size_t i = 0; i < r_limit; i++) {
                _buffer.push_back(0);
                _wflags.push_back(false);
            }
        }
        // 3. 用 subring 的内容覆盖 延展后的 buffer
        size_t offset = index - min_index;
        for (size_t i = 0; i < len; i++) {
            if (_wflags[i + offset])
                continue;
            _buffer[i + offset] = data[i];
            _wflags[i + offset] = true;
        }

        // 4. 更新 _next_assemble
        _next_assemble = min_index;
    }

    // 判断是否输出 _buffer 中的部分内容
    // a. buffer 不需要输出，返回
    if (_next_assemble > _first_unassemble)
        return;
    // b. buffer 中有数据且开头输出过了
    if (_buffer.size() && _next_assemble < _first_unassemble) {
        // 去掉输出过的部分
        size_t offset = min(_first_unassemble - _next_assemble, _buffer.size());
        for (size_t i = 0; i < offset; i++) {
            _buffer.pop_front();
            _wflags.pop_front();
            _next_assemble++;
        }
        if (!_buffer.empty()) {
            // 后面的内容需要输出
            while (!_wflags.empty() && _wflags.front()) {
                _output.write_char(_buffer.front());
                _buffer.pop_front();
                _wflags.pop_front();
                _next_assemble++;
                _first_unassemble++;
            }
        }
    }

    if (has_eof && _buffer.empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t res = 0;
    for (size_t i = 0; i < _wflags.size(); i++) {
        if (_wflags[i])
            res++;
    }
    return res;
}

bool StreamReassembler::empty() const { return {}; }
