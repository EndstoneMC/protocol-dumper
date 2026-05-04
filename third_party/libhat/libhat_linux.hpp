#pragma once

#include <cstddef>

#include <libhat.hpp>

#ifdef LIBHAT_LINUX
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <unistd.h>

#include <stdexcept>

namespace hat::process {
inline std::span<std::byte> module::get_section_data(const std::string_view name) const
{
    if (elf_version(EV_CURRENT) == EV_NONE) {
        throw std::runtime_error("elf_version failed");
    }

    int fd = ::open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error("open(/proc/self/exe) failed");
    }

    Elf *e = elf_begin(fd, ELF_C_READ, nullptr);
    if (!e) {
        ::close(fd);
        throw std::runtime_error("elf_begin() failed.");
    }

    size_t shstrndx = 0;
    if (elf_getshdrstrndx(e, &shstrndx) != 0) {
        elf_end(e);
        ::close(fd);
        throw std::runtime_error("elf_getshdrstrndx() failed.");
    }

    GElf_Shdr shdr{};
    Elf_Scn *scn = nullptr;
    bool found = false;
    while ((scn = elf_nextscn(e, scn)) != nullptr) {
        if (!gelf_getshdr(scn, &shdr)) {
            throw std::runtime_error("gelf_getshdr() failed.");
        }
        const char *sh_name = elf_strptr(e, shstrndx, shdr.sh_name);
        if (sh_name && std::string_view(sh_name) == name) {
            found = true;
            break;
        }
    }

    elf_end(e);
    ::close(fd);

    if (!found) {
        return {};
    }
    auto *bytes = reinterpret_cast<std::byte *>(this->address());
    return {bytes + shdr.sh_addr, shdr.sh_size};
}
}  // namespace hat::process
#endif
