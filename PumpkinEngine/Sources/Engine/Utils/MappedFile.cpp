module;

#ifdef _WIN32
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif


module Pumpkin.Utils.MappedFile;

namespace Pumpkin {
#ifdef _WIN32

    MappedFile::MappedFile(const char *path)
    {
        HANDLE hFile = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );

        if (hFile == INVALID_HANDLE_VALUE) return;

        HANDLE hMap = CreateFileMapping(
            hFile,
            NULL,
            PAGE_READONLY,
            0,
            0,
            NULL
            );

        if (hMap == NULL) { CloseHandle(hFile); return; }

        const char* data = (const char*)MapViewOfFile(
            hMap,
            FILE_MAP_READ,
            0,
            0,
            0
            );

        if (!data) { CloseHandle(hMap); CloseHandle(hFile); return; }

        m_data = data;
        LARGE_INTEGER fileSize;
        GetFileSizeEx(hFile, &fileSize);
        m_size = (size_t)fileSize.QuadPart;
        m_handle[0] = hFile;
        m_handle[1] = hMap;
    }

    MappedFile::~MappedFile()
    {
        UnmapViewOfFile(m_data);
        CloseHandle((HANDLE)m_handle[1]);
        CloseHandle((HANDLE)m_handle[0]);
        m_data      = nullptr;
        m_handle[0] = nullptr;
        m_handle[1] = nullptr;
    }

#else

    MappedFile::MappedFile(const char *path)
    {
        int fd = open(path, O_RDONLY);
        if (fd == -1) return;

        struct stat sb;
        if (fstat(fd, &sb) == -1) { close(fd); return; }

        const char* data = (const char*)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) { close(fd); return; }

        m_data = data;
        m_size = sb.st_size;
        m_handle[0] = (void*)(intptr_t)fd;
    }

    MappedFile::~MappedFile()
    {
        munmap((void*)m_data, m_size);
        close((int)(intptr_t)m_handle[0]);
        m_data = nullptr;
        m_handle[0] = nullptr;
        m_handle[1] = nullptr;
    }

#endif
}
