#include "DataBuffer.h"

#include "WindowsUtils.h"

namespace sci
{
    namespace {

        class VectorMemoryImpl : public MemoryBuffer::Impl
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

        class StringMemoryImpl : public MemoryBuffer::Impl
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

        class MappedFileImpl : public MemoryBuffer::Impl
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

        class MemoryImpl : public DataBuffer::Impl
        {
        public:
            explicit MemoryImpl(MemoryBuffer memory_buffer) : memory_buffer_(std::move(memory_buffer))
            {
            }

            absl::StatusOr<std::size_t> GetSize() const final
            {
                return memory_buffer_.GetSize();
            }

            absl::Status Read(std::size_t offset, absl::Span<uint8_t> dest_buffer) const final {
                auto data_span = memory_buffer_.GetData(offset, dest_buffer.size());
                if (!data_span.ok()) {
                    return data_span.status();
                }
                std::memcpy(dest_buffer.data(), data_span->data(), dest_buffer.size());
                return absl::OkStatus();
            }
        private:
            MemoryBuffer memory_buffer_;
        };
    }

    absl::StatusOr<MemoryBuffer> MemoryBuffer::CreateFromImpl(std::shared_ptr<Impl> impl)
    {
        auto buffer_size = impl->GetMemory().size();
        return MemoryBuffer(std::move(impl), 0, buffer_size);
    }

    MemoryBuffer MemoryBuffer::CreateFromVector(std::vector<uint8_t> data)
    {
        // This should never error.
        return CreateFromImpl(std::make_shared<VectorMemoryImpl>(std::move(data))).value();
    }

    MemoryBuffer MemoryBuffer::CreateFromString(std::string data)
    {
        // This should never error.
        return CreateFromImpl(std::make_shared<StringMemoryImpl>(std::move(data))).value();
    }

    absl::StatusOr<MemoryBuffer> MemoryBuffer::CreateMappedFile(std::string_view file_path)
    {
        auto mapped_file = MemoryMappedFile::FromFilename(std::string(file_path));
        if (!mapped_file.ok()) {
            return CreateFromImpl(nullptr);
        }
        return CreateFromImpl(std::make_shared<MappedFileImpl>(std::move(mapped_file).value()));
    }

    MemoryBuffer::MemoryBuffer() : impl_(nullptr), offset_(0), size_(0)
    {
    }

    std::size_t MemoryBuffer::GetSize() const
    {
        return size_;
    }

    absl::Span<const uint8_t> MemoryBuffer::GetAllData() const
    {
        return impl_->GetMemory();
    }

    absl::StatusOr<absl::Span<const uint8_t>> MemoryBuffer::GetData(std::size_t offset,
        std::optional<std::size_t> size) const
    {
        auto start = offset + offset_;
        auto end = size.has_value() ? size.value() + start : size_;
        if (end > size_)
        {
            return absl::InvalidArgumentError("GetData out of range");
        }

        return impl_->GetMemory().subspan(start, end - start);
    }

    absl::StatusOr<MemoryBuffer> MemoryBuffer::SubBuffer(std::size_t offset, std::optional<std::size_t> size) const
    {
        if (offset > size_)
        {
            return absl::InvalidArgumentError("SubBuffer out of range");
        }

        std::size_t end;
        if (size.has_value())
        {
            if (size.value() + offset > size_)
            {
                return absl::InvalidArgumentError("SubBuffer out of range");
            }
            end = offset_ + offset + size.value();
        }
        else
        {
            end = size_;
        }

        auto start = offset + offset_;
        return MemoryBuffer(impl_, start, end - start);
    }

    // DataBuffer

    absl::StatusOr<DataBuffer> DataBuffer::CreateFromImpl(std::shared_ptr<Impl> impl)
    {
        auto buffer_size = impl->GetSize();
        if (!buffer_size.ok()) {
            return buffer_size.status();
        }
        return DataBuffer(std::move(impl), 0, *buffer_size);
    }

    DataBuffer DataBuffer::CreateFromMemory(MemoryBuffer memory_buffer)
    {
        // This should never error.
        return CreateFromImpl(std::make_shared<MemoryImpl>(std::move(memory_buffer))).value();
    }

    DataBuffer DataBuffer::CreateFromVector(std::vector<uint8_t> data)
    {
        return CreateFromMemory(MemoryBuffer::CreateFromVector(std::move(data)));
    }

    DataBuffer DataBuffer::CreateFromString(std::string data)
    {
        return CreateFromMemory(MemoryBuffer::CreateFromString(std::move(data)));
    }

    absl::StatusOr<DataBuffer> DataBuffer::CreateMappedFile(std::string_view file_path)
    {
        auto data = MemoryBuffer::CreateMappedFile(file_path);
        if (!data.ok())
        {
            return data.status();
        }
        return CreateFromMemory(std::move(data).value());
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

    absl::StatusOr<DataBuffer> DataBuffer::SubBuffer(std::size_t offset, std::optional<std::size_t> size) const
    {
        if (offset > size_)
        {
            return absl::InvalidArgumentError("SubBuffer out of range");
        }

        std::size_t end;
        if (size.has_value())
        {
            if (size.value() + offset > size_)
            {
                return absl::InvalidArgumentError("SubBuffer out of range");
            }
            end = offset_ + offset + size.value();
        }
        else
        {
            end = size_;
        }
        auto start = offset + offset_;
        return DataBuffer(impl_, start, end - start);
    }
}
