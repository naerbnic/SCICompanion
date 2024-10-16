#pragma once
#include <filesystem>
#include <absl/status/status.h>
#include <absl/status/statusor.h>

namespace sci
{
    class MemoryBuffer
    {
    public:
        class Impl
        {
        public:
            virtual ~Impl() = default;

            virtual absl::Span<const uint8_t> GetMemory() const = 0;
        };

        static absl::StatusOr<MemoryBuffer> CreateFromImpl(std::shared_ptr<Impl> impl);
        static MemoryBuffer CreateFromVector(std::vector<uint8_t> data);
        static MemoryBuffer CreateFromString(std::string data);
        static absl::StatusOr<MemoryBuffer> CreateMappedFile(std::string_view file_path);

        MemoryBuffer();

        MemoryBuffer(const MemoryBuffer& other) = default;
        MemoryBuffer(MemoryBuffer&& other) noexcept = default;

        MemoryBuffer& operator=(const MemoryBuffer& other) = default;
        MemoryBuffer& operator=(MemoryBuffer&& other) noexcept = default;

        std::size_t GetSize() const;
        // Returns all the data contained in this buffer. The returned value is owned by
        // the buffer and will be valid until the buffer is dropped.
        absl::Span<const uint8_t> GetAllData() const;
        absl::StatusOr<absl::Span<const uint8_t>> GetData(std::size_t offset = 0, std::optional<std::size_t> size = std::nullopt) const;

        absl::StatusOr<MemoryBuffer> SubBuffer(std::size_t offset = 0, std::optional<std::size_t> size = std::nullopt) const;

    private:
        explicit MemoryBuffer(std::shared_ptr<Impl> impl, std::size_t offset, std::size_t size) : impl_(std::move(impl)),
            offset_(offset), size_(size)
        {
        }

        std::shared_ptr<Impl> impl_;
        std::size_t offset_;
        std::size_t size_;
    };

    class DataBuffer
    {
    public:
        class Impl
        {
        public:
            virtual ~Impl() = default;

            virtual absl::StatusOr<std::size_t> GetSize() const = 0;
            virtual absl::Status Read(std::size_t offset, absl::Span<uint8_t> dest_buffer) const = 0;
        };

        static absl::StatusOr<DataBuffer> CreateFromImpl(std::shared_ptr<Impl> impl);

        static DataBuffer CreateFromMemory(MemoryBuffer memory_buffer);
        static DataBuffer CreateFromVector(std::vector<uint8_t> data);
        static DataBuffer CreateFromString(std::string data);
        static absl::StatusOr<DataBuffer> CreateMappedFile(std::string_view file_path);

        DataBuffer();

        DataBuffer(const DataBuffer& other) = default;
        DataBuffer(DataBuffer&& other) noexcept = default;

        DataBuffer& operator=(const DataBuffer& other) = default;
        DataBuffer& operator=(DataBuffer&& other) noexcept = default;

        std::size_t GetSize() const;
        absl::Status Read(std::size_t offset, absl::Span<uint8_t> dest_buffer) const;

        absl::StatusOr<DataBuffer> SubBuffer(std::size_t offset = 0, std::optional<std::size_t> size = std::nullopt) const;

    private:
        explicit DataBuffer(std::shared_ptr<Impl> impl, std::size_t offset, std::size_t size) : impl_(std::move(impl)),
            offset_(offset), size_(size)
        {
        }

        std::shared_ptr<Impl> impl_;
        std::size_t offset_;
        std::size_t size_;
    };
}
