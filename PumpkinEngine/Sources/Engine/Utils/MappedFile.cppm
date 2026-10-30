module;

#include <cstddef>

export module Pumpkin.Utils.MappedFile;

export namespace Pumpkin {
    class MappedFile
    {
    public:
        MappedFile(const char* path);
        ~MappedFile();

        MappedFile(const MappedFile&)            = delete;
        MappedFile& operator=(const MappedFile&) = delete;

        const char* Data() const { return m_data; }
        std::size_t Size() const { return m_size; }
        bool        IsOpen() const { return m_data != nullptr; }

    private:
        const char* m_data = nullptr;
        std::size_t m_size = 0;
        void*       m_handle[2] = {};
    };
}
