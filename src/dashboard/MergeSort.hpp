#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace ovc::dashboard {

// Stable merge sort, used to sort the class overview by any column.
// Equal rows keep their order, so sorting by one column after another works
// as expected.
template <typename T, typename Less>
void mergeSort(std::vector<T>& items, Less less)
{
    if (items.size() < 2)
        return;
    std::vector<T> buffer(items.size());
    for (std::size_t width = 1; width < items.size(); width *= 2) {
        for (std::size_t left = 0; left < items.size(); left += 2 * width) {
            const std::size_t middle = std::min(left + width, items.size());
            const std::size_t right = std::min(left + 2 * width, items.size());
            std::size_t i = left, j = middle, k = left;
            while (i < middle && j < right)
                buffer[k++] = less(items[j], items[i]) ? items[j++] : items[i++];
            while (i < middle)
                buffer[k++] = items[i++];
            while (j < right)
                buffer[k++] = items[j++];
        }
        items.swap(buffer);
    }
}

} // namespace ovc::dashboard
