#pragma once

#include <memory>
#include <vector>

template<typename T>
class PoolAllocator {
public:
    T *allocate(const size_t n) {
        if (currentIndex + n > BLOCK_SIZE) {
            blocks.push_back(std::make_unique<T[]>(BLOCK_SIZE));
            currentBlock = blocks.size() - 1;
            currentIndex = 0;
        }
        T *result = &blocks[currentBlock][currentIndex];
        currentIndex += n;
        return result;
    }

    void reset() {
        currentBlock = 0;
        currentIndex = 0;
    }

private:
    static constexpr size_t BLOCK_SIZE = 8192 * 2;
    std::vector<std::unique_ptr<T[]> > blocks;
    size_t currentBlock = 0;
    size_t currentIndex = BLOCK_SIZE;
};
