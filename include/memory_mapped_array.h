#ifndef HPORDERBOOK_MEMORY_MAPPED_ARRAY_H
#define HPORDERBOOK_MEMORY_MAPPED_ARRAY_H

#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <stdexcept>
#include <span>

template<typename T>
class MemoryMappedArray {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

private:
    T* data_;
    size_t size_;
    int fd_;
    std::string filename_;

    void map_memory() {
        data_ = static_cast<T*>(mmap(
                nullptr, size_ * sizeof(T),
                PROT_READ | PROT_WRITE,
                MAP_SHARED, fd_, 0
        ));

        if (data_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("Failed to map memory");
        }
    }

public:
    MemoryMappedArray(const std::string& filename, size_t size)
            : size_(size), filename_(filename) {

        fd_ = open(filename.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd_ == -1) {
            throw std::runtime_error("Failed to open file");
        }

        if (ftruncate(fd_, size * sizeof(T)) == -1) {
            close(fd_);
            throw std::runtime_error("Failed to resize file");
        }

        map_memory();
    }

    ~MemoryMappedArray() {
        if (data_) {
            munmap(data_, size_ * sizeof(T));
            close(fd_);
        }
    }

    // No copying
    MemoryMappedArray(const MemoryMappedArray&) = delete;
    MemoryMappedArray& operator=(const MemoryMappedArray&) = delete;

    // Moving
    MemoryMappedArray(MemoryMappedArray&& other) noexcept
            : data_(other.data_), size_(other.size_),
              fd_(other.fd_), filename_(std::move(other.filename_)) {
        other.data_ = nullptr;
        other.fd_ = -1;
    }

    T& operator[](size_t index) noexcept {
        return data_[index];
    }

    const T& operator[](size_t index) const noexcept {
        return data_[index];
    }

    std::span<T> get_span() noexcept {
        return std::span<T>(data_, size_);
    }

    void flush() {
        msync(data_, size_ * sizeof(T), MS_SYNC);
    }
};

#endif //HPORDERBOOK_MEMORY_MAPPED_ARRAY_H
