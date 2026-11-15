#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ovc::editor {

// Byte storage with a movable gap at the editing position.
//
// Text before the gap lives at data_[0, gapStart_), text after it at
// data_[gapEnd_, data_.size()). Inserting at the gap only writes into it, so
// typing at the cursor costs O(1) amortised. Moving the edit position moves
// the gap, which costs the distance moved.
class GapBuffer {
public:
    explicit GapBuffer(std::size_t initialGap = 64);

    std::size_t size() const { return data_.size() - gapSize(); }
    bool empty() const { return size() == 0; }

    char at(std::size_t position) const;
    void insert(std::size_t position, std::string_view text);
    void erase(std::size_t position, std::size_t count);

    std::string substr(std::size_t position, std::size_t count) const;
    std::string toString() const;

private:
    std::size_t gapSize() const { return gapEnd_ - gapStart_; }
    void moveGapTo(std::size_t position);
    void ensureGap(std::size_t needed);

    std::vector<char> data_;
    std::size_t gapStart_ = 0;
    std::size_t gapEnd_ = 0;
};

} // namespace ovc::editor
