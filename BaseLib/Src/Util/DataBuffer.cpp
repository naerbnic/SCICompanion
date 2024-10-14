#include "DataBuffer.h"

#include "WindowsUtils.h"

namespace sci
{
    namespace {
        class MemoryImpl : public DataBuffer::Impl
        {
        public:
            absl::StatusOr<std::size_t> GetSize() const final
            {
                return GetMemory().size();
            }

            absl::Status Read(std::size_t offset, absl::Span<uint8_t> dest_buffer) const final {
                if (offset + dest_buffer.size() > GetMemory().size()) {
                    return absl::InvalidArgumentError("Read out of range");
                }
                std::memcpy(dest_buffer.data(), GetMemory().data() + offset, dest_buffer.size());
                return absl::OkStatus();
            }

            virtual absl::Span<const uint8_t> GetMemory() const = 0;
        };

        class VectorMemoryImpl : public MemoryImpl
        {
        public:
            explicit VectorMemoryImpl(std::vector<uint8_t> data) : data_(std::move(data))
            {
            }

            absl::Span<const uint8_t> GetMemory() const final
            {
                return absl::MakeSpan(data_);
            }

        private:
            std::vector<uint8_t> data_;
        };

        class StringMemoryImpl : public MemoryImpl
        {
        public:
            explicit StringMemoryImpl(std::string data) : data_(std::move(data))
            {
            }

            absl::Span<const uint8_t> GetMemory() const final
            {
                return absl::Span(reinterpret_cast<const uint8_t*>(data_.data()), data_.size());
            }
        private:
            std::string data_;
        };

        class MappedFileImpl : public MemoryImpl
        {
        public:
            explicit MappedFileImpl(MemoryMappedFile mapped_file) : mapped_file_(std::move(mapped_file))
            {
            }

            absl::Span<const uint8_t> GetMemory() const final
            {
                return mapped_file_.GetDataBuffer();
            }

        private:
            MemoryMappedFile mapped_file_;
        };
    }

    absl::StatusOr<DataBuffer> DataBuffer::CreateFromImpl(std::shared_ptr<Impl> impl)
    {
        auto buffer_size = impl->GetSize();
        if (!buffer_size.ok()) {
            return buffer_size.status();
        }
        return DataBuffer(std::move(impl), 0, *buffer_size);
    }

    DataBuffer DataBuffer::CreateFromMemory(std::vector<uint8_t> data)
    {
        // This should never error.
        return CreateFromImpl(std::make_shared<VectorMemoryImpl>(std::move(data))).value();
    }

    DataBuffer DataBuffer::CreateFromString(std::string data)
    {
        // This should never error.
        return CreateFromImpl(std::make_shared<StringMemoryImpl>(std::move(data))).value();
    }

    absl::StatusOr<DataBuffer> DataBuffer::CreateMappedFile(std::filesystem::path file_path)
    {
        auto mapped_file = MemoryMappedFile::FromFilename(file_path.string());
        if (!mapped_file.ok()) {
            return CreateFromImpl(nullptr);
        }
        return CreateFromImpl(std::make_shared<MappedFileImpl>(std::move(mapped_file).value()));
    }

    DataBuffer::DataBuffer() : impl_(nullptr), offset_(0), size_(0)
    {
    }

    std::size_t DataBuffer::GetSize() const
    {
        return size_;
    }

    absl::Status DataBuffer::Read(std::size_t offset, absl::Span<uint8_t> dest_buffer) const
    {
        return impl_->Read(offset, dest_buffer);
    }

    absl::StatusOr<DataBuffer> DataBuffer::SubBuffer(std::size_t offset, std::size_t size) const
    {
        if (offset + size > size_)
        {
            return absl::InvalidArgumentError("SubBuffer out of range");
        }
        return DataBuffer(impl_, offset_ + offset, size);
    }
}
