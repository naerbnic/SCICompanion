#pragma once
#include <filesystem>
#include <absl/status/status.h>
#include <absl/status/statusor.h>

namespace sci
{
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

        static DataBuffer CreateFromMemory(std::vector<uint8_t> data);
        static DataBuffer CreateFromString(std::string data);
        static absl::StatusOr<DataBuffer> CreateMappedFile(std::filesystem::path file_path);
        DataBuffer();

        DataBuffer(const DataBuffer& other) = default;
        DataBuffer(DataBuffer&& other) noexcept = default;

        DataBuffer& operator=(const DataBuffer& other) = default;
        DataBuffer& operator=(DataBuffer&& other) noexcept = default;

        std::size_t GetSize() const;
        absl::Status Read(std::size_t offset, absl::Span<uint8_t> dest_buffer) const;

        absl::StatusOr<DataBuffer> SubBuffer(std::size_t offset, std::size_t size) const;

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
