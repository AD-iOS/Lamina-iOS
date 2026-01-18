#pragma once
#include "lamina_api/value.hpp"
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>  // 新增：用于uintptr_t（32位地址转换需用）  // 关键修复1
#include <cstring>  // 用于 strstr/strcmp

// 平台宏适配：区分 Windows 与类 Unix 系统（Linux/macOS）
#ifdef _WIN32
#include <windows.h>
#include <imagehlp.h>  // Windows 符号表解析（PE 文件结构）
#include <tlhelp32.h>
#pragma comment(lib, "imagehlp.lib")

#define DYLIB_HANDLE HMODULE
#define DYLIB_LOAD(path) LoadLibraryA(path.c_str())
#define DYLIB_GETSYM(handle, name) reinterpret_cast<void*>(GetProcAddress(handle, name))
#define DYLIB_UNLOAD(handle) FreeLibrary(handle)
#define DYLIB_ERROR() GetLastError()

#else
#ifndef STT_DEBUG
#define STT_DEBUG 0x11  // 补充调试符号类型定义（部分系统可能未定义）
#endif

#define DYLIB_HANDLE void*
#define DYLIB_LOAD(path) dlopen(path.c_str(), RTLD_LAZY | RTLD_NOLOAD)
#define DYLIB_GETSYM(handle, name) dlsym(handle, name)
#define DYLIB_UNLOAD(handle) dlclose(handle)
#define DYLIB_ERROR() dlerror()

#include <dlfcn.h>

#if defined(__linux__)

#include <elf.h>
#include <link.h>

#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <mach-o/loader.h>

static std::string s_target_lib_path;
static std::vector<std::string>* s_macho_symbols = nullptr;

static void iterate_macho_images() {
    uint32_t count = _dyld_image_count();
    
    for (uint32_t i = 0; i < count; i++) {
        const char* image_path = _dyld_get_image_name(i);
        if (!image_path || std::string(image_path) != s_target_lib_path) {
            continue;
        }
        
        const struct mach_header* mh = _dyld_get_image_header(i);
        uintptr_t vmaddr_slide = _dyld_get_image_vmaddr_slide(i);
        
        void* handle = dlopen(image_path, RTLD_LAZY | RTLD_LOCAL);
        if (!handle) {
            std::cerr << "macOS dlopen failed: " << dlerror() << std::endl;
            continue;
        }
        
        const struct load_command* lc = nullptr;
        struct symtab_command* symtab_cmd = nullptr;
        uint32_t ncmds = 0;
        
        uint32_t magic = mh->magic;
        if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
            const struct mach_header_64* mh64 = reinterpret_cast<const struct mach_header_64*>(mh);
            lc = reinterpret_cast<const struct load_command*>(
                reinterpret_cast<const uint8_t*>(mh64) + sizeof(struct mach_header_64)
            );
            ncmds = mh64->ncmds;
        } else if (magic == MH_MAGIC || magic == MH_CIGAM) {
            lc = reinterpret_cast<const struct load_command*>(
                reinterpret_cast<const uint8_t*>(mh) + sizeof(struct mach_header)
            );
            ncmds = mh->ncmds;
        } else {
            dlclose(handle);
            continue;
        }
        
        // 查找符號表
        for (uint32_t j = 0; j < ncmds; ++j) {
            if (lc->cmd == LC_SYMTAB) {
                symtab_cmd = reinterpret_cast<struct symtab_command*>(const_cast<struct load_command*>(lc));
                break;
            }
            lc = reinterpret_cast<const struct load_command*>(
                reinterpret_cast<const uint8_t*>(lc) + lc->cmdsize
            );
        }
        
        if (!symtab_cmd) {
            dlclose(handle);
            continue;
        }
        
        const char* strtab = reinterpret_cast<const char*>(vmaddr_slide + symtab_cmd->stroff);
        const uint32_t symcount = symtab_cmd->nsyms;
        
        // 解析符號表
        if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
            const struct nlist_64* symtab = reinterpret_cast<const struct nlist_64*>(
                vmaddr_slide + symtab_cmd->symoff
            );
            for (uint32_t k = 0; k < symcount; ++k) {
                const struct nlist_64* sym = &symtab[k];
                if (sym->n_un.n_strx == 0 || !(sym->n_type & N_EXT) || (sym->n_type & N_STAB)) {
                    continue;
                }
                const char* sym_name = strtab + sym->n_un.n_strx;
                if (sym_name[0] == '_') sym_name++;
                if (strstr(sym_name, "__mh_") || strstr(sym_name, "_dyld_")) {
                    continue;
                }
                s_macho_symbols->emplace_back(sym_name);
            }
        } else {
            const struct nlist* symtab = reinterpret_cast<const struct nlist*>(
                vmaddr_slide + symtab_cmd->symoff
            );
            for (uint32_t k = 0; k < symcount; ++k) {
                const struct nlist* sym = &symtab[k];
                if (sym->n_un.n_strx == 0 || !(sym->n_type & N_EXT) || (sym->n_type & N_STAB)) {
                    continue;
                }
                const char* sym_name = strtab + sym->n_un.n_strx;
                if (sym_name[0] == '_') sym_name++;
                if (strstr(sym_name, "__mh_") || strstr(sym_name, "_dyld_")) {
                    continue;
                }
                s_macho_symbols->emplace_back(sym_name);
            }
        }
        
        dlclose(handle);
        break;
    }
}

std::vector<std::string> get_dylib_symbols(const std::string& lib_path) {
    std::vector<std::string> symbols;
    s_target_lib_path = lib_path;
    s_macho_symbols = &symbols;
    
    // 使用公共 API 替代私有 API
    // 這至少可以解決 arm64e 符號查找失敗的問題
    iterate_macho_images();
    
    s_target_lib_path.clear();
    s_macho_symbols = nullptr;
    return symbols;
}
#endif

#endif

using ModuleFunc = Value (*)(const std::vector<Value>&);

#ifdef _WIN32
inline std::vector<std::string> get_win_dylib_symbols(DYLIB_HANDLE hModule) {
    std::vector<std::string> symbols;
    if (!hModule) return symbols;

    char module_path[MAX_PATH] = {0};
    GetModuleFileNameA(hModule, module_path, MAX_PATH);

    LOADED_IMAGE loaded_img = {nullptr};
    if (!MapAndLoad(module_path, nullptr, &loaded_img, FALSE, TRUE)) {
        std::cerr << "Failed to map module: " << GetLastError() << std::endl;
        return symbols;
    }

    const PIMAGE_NT_HEADERS nt_headers = ImageNtHeader(loaded_img.MappedAddress);
    const auto export_dir =
        static_cast<PIMAGE_EXPORT_DIRECTORY>(
            ImageRvaToVa(nt_headers, loaded_img.MappedAddress, 
                        nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, 
                        nullptr)
        );
    if (!export_dir) {
        std::cerr << "No export directory found" << std::endl;
        UnMapAndLoad(&loaded_img);
        return symbols;
    }

    const auto* name_rvas = static_cast<DWORD*>(
        ImageRvaToVa(nt_headers, loaded_img.MappedAddress, export_dir->AddressOfNames, nullptr)
    );
    for (DWORD i = 0; i < export_dir->NumberOfNames; ++i) {
        auto sym_name = static_cast<const char*>(
            ImageRvaToVa(nt_headers, loaded_img.MappedAddress, name_rvas[i], nullptr)
        );
        if (sym_name && *sym_name != '\0') {
            symbols.emplace_back(sym_name);
        }
    }

    UnMapAndLoad(&loaded_img);
    return symbols;
}
#endif

std::vector<std::string> get_dylib_symbols(const std::string& lib_path);

#ifndef _WIN32
#if defined(__linux__)
static std::string s_linux_lib_path;

#if __SIZEOF_POINTER__ == 8
using Elf_Phdr = Elf64_Phdr;
using Elf_Dyn = Elf64_Dyn;
using Elf_Sym = Elf64_Sym;
using Elf_Word = Elf64_Word;
#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_ST_TYPE ELF64_ST_TYPE
#else
using Elf_Phdr = Elf32_Phdr;
using Elf_Dyn = Elf32_Dyn;
using Elf_Sym = Elf32_Sym;
using Elf_Word = Elf32_Word;
#define ELF_ST_BIND ELF32_ST_BIND
#define ELF_ST_TYPE ELF32_ST_TYPE
#endif

static int elf_symbol_callback(struct dl_phdr_info* info, size_t size, void* data) {
    std::vector<std::string>* symbols = reinterpret_cast<std::vector<std::string>*>(data);
    if (!symbols || !info->dlpi_name) return 1;

    if (std::string(info->dlpi_name) != s_linux_lib_path) return 0;

    void* handle = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) return 0;

    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const Elf_Phdr* phdr = &info->dlpi_phdr[i];
        if (phdr->p_type != PT_DYNAMIC) continue;

        Elf_Dyn* dyn = reinterpret_cast<Elf_Dyn*>(
            reinterpret_cast<uintptr_t>(info->dlpi_addr) + phdr->p_vaddr
        );
        Elf_Sym* symtab = nullptr;
        const char* strtab = nullptr;
        Elf_Word symcount = 0;

        for (; dyn->d_tag != DT_NULL; ++dyn) {
            switch (dyn->d_tag) {
                case DT_SYMTAB:
#if __SIZEOF_POINTER__ == 8
                    symtab = reinterpret_cast<Elf_Sym*>(dyn->d_un.d_ptr);
#else
                    symtab = reinterpret_cast<Elf_Sym*>(static_cast<uintptr_t>(dyn->d_un.d_val));
#endif
                    break;
                case DT_STRTAB:
#if __SIZEOF_POINTER__ == 8
                    strtab = reinterpret_cast<const char*>(dyn->d_un.d_ptr);
#else
                    strtab = reinterpret_cast<const char*>(static_cast<uintptr_t>(dyn->d_un.d_val));
#endif
                    break;
                case DT_SYMENT:
                    symcount = phdr->p_memsz / sizeof(Elf_Sym);
                    break;
            }
        }

        if (!symtab || !strtab || symcount == 0) continue;

        for (Elf_Word j = 0; j < symcount; ++j) {
            const Elf_Sym* sym = &symtab[j];
            if (ELF_ST_BIND(sym->st_info) != STB_GLOBAL || 
                ELF_ST_TYPE(sym->st_info) == STT_DEBUG || 
                sym->st_name == 0) {
                continue;
            }
            const char* sym_name = strtab + sym->st_name;
            if (strstr(sym_name, "_init") || strstr(sym_name, "_fini")) continue;
            symbols->emplace_back(sym_name);
        }
    }

    #undef ELF_ST_BIND
    #undef ELF_ST_TYPE
    dlclose(handle);
    return 0;
}

std::vector<std::string> get_dylib_symbols(const std::string& lib_path) {
    std::vector<std::string> symbols;
    s_linux_lib_path = lib_path;
    dl_iterate_phdr(elf_symbol_callback, &symbols);
    s_linux_lib_path.clear();
    return symbols;
}

/* 衝突了,舊實現
#elif defined(__APPLE__)
static std::string s_target_lib_path;
static std::vector<std::string>* s_macho_symbols = nullptr;

static bool macho_image_callback(const struct mach_header* mh, uintptr_t vmaddr_slide, 
                                 const char* image_path, void* data) {
    if (!image_path || std::string(image_path) != s_target_lib_path) {
        return false;
    }

    void* handle = dlopen(image_path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "macOS dlopen failed: " << dlerror() << std::endl;
        return false;
    }

    const struct load_command* lc = nullptr;
    struct symtab_command* symtab_cmd = nullptr;
    uint32_t ncmds = 0;

    uint32_t magic = mh->magic;
    if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
        const struct mach_header_64* mh64 = reinterpret_cast<const struct mach_header_64*>(mh);
        lc = reinterpret_cast<const struct load_command*>(
            reinterpret_cast<const uint8_t*>(mh64) + sizeof(struct mach_header_64)
        );
        ncmds = mh64->ncmds;
    } else if (magic == MH_MAGIC || magic == MH_CIGAM) {
        lc = reinterpret_cast<const struct load_command*>(
            reinterpret_cast<const uint8_t*>(mh) + sizeof(struct mach_header)
        );
        ncmds = mh->ncmds;
    } else {
        dlclose(handle);
        return false;
    }

    for (uint32_t i = 0; i < ncmds; ++i) {
        if (lc->cmd == LC_SYMTAB) {
            symtab_cmd = reinterpret_cast<struct symtab_command*>(const_cast<struct load_command*>(lc));
            break;
        }
        lc = reinterpret_cast<const struct load_command*>(
            reinterpret_cast<const uint8_t*>(lc) + lc->cmdsize
        );
    }

    if (!symtab_cmd) {
        dlclose(handle);
        return false;
    }

    const char* strtab = reinterpret_cast<const char*>(vmaddr_slide + symtab_cmd->stroff);
    const uint32_t symcount = symtab_cmd->nsyms;

    if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
        const struct nlist_64* symtab = reinterpret_cast<const struct nlist_64*>(
            vmaddr_slide + symtab_cmd->symoff
        );
        for (uint32_t j = 0; j < symcount; ++j) {
            const struct nlist_64* sym = &symtab[j];
            if (sym->n_un.n_strx == 0 || !(sym->n_type & N_EXT) || (sym->n_type & N_STAB)) {
                continue;
            }
            const char* sym_name = strtab + sym->n_un.n_strx;
            if (sym_name[0] == '_') sym_name++;
            if (strstr(sym_name, "__mh_") || strstr(sym_name, "_dyld_")) {
                continue;
            }
            s_macho_symbols->emplace_back(sym_name);
        }
    } else {
        const struct nlist* symtab = reinterpret_cast<const struct nlist*>(
            vmaddr_slide + symtab_cmd->symoff
        );
        for (uint32_t j = 0; j < symcount; ++j) {
            const struct nlist* sym = &symtab[j];
            if (sym->n_un.n_strx == 0 || !(sym->n_type & N_EXT) || (sym->n_type & N_STAB)) {
                continue;
            }
            const char* sym_name = strtab + sym->n_un.n_strx;
            if (sym_name[0] == '_') sym_name++;
            if (strstr(sym_name, "__mh_") || strstr(sym_name, "_dyld_")) {
                continue;
            }
            s_macho_symbols->emplace_back(sym_name);
        }
    }

    dlclose(handle);
    return true;
}

std::vector<std::string> get_dylib_symbols(const std::string& lib_path) {
    std::vector<std::string> symbols;
    s_target_lib_path = lib_path;
    s_macho_symbols = &symbols;
    _dyld_iterate_images(macho_image_callback, nullptr);
    s_target_lib_path.clear();
    s_macho_symbols = nullptr;
    return symbols;
}
*/

#endif
#endif

inline std::unordered_map<std::string, Value> load_cppmodule(const std::string& path) {
    std::unordered_map<std::string, Value> result;
    DYLIB_HANDLE handle = nullptr;
    std::vector<std::string> all_symbols;

    handle = DYLIB_LOAD(path);
    if (!handle) {
        std::cerr << "Load module failed: " << DYLIB_ERROR() << std::endl;
        return result;
    }

#ifdef _WIN32
    all_symbols = get_win_dylib_symbols(handle);
#else
    all_symbols = get_dylib_symbols(path);
#endif

    if (all_symbols.empty()) {
        std::cerr << "No exported symbols found in module" << std::endl;
        DYLIB_UNLOAD(handle);
        return result;
    }

    for (const auto& sym_name : all_symbols) {
        if (sym_name.starts_with("lamina_protect_")) continue;

        void* sym_addr = DYLIB_GETSYM(handle, sym_name.c_str());
        if (!sym_addr) continue;

        const auto* var_ptr = static_cast<Value*>(sym_addr);
        bool is_valid_value_var = (!var_ptr->is_lmCppFunction());
        if (is_valid_value_var) {
            result[sym_name] = *var_ptr;
            std::cout << "Found Value var: " << sym_name << std::endl;
            continue;
        }

        auto func_ptr = reinterpret_cast<ModuleFunc>(sym_addr);
        if (func_ptr != nullptr) {
            result[sym_name] = Value(std::make_shared<LmCppFunction>(func_ptr));
            std::cout << "Found ModuleFunc: " << sym_name << std::endl;
            continue;
        }
        std::cerr << "Skip unknown type symbol: " << sym_name << std::endl;
    }
    // DYLIB_UNLOAD(handle);
    return result;
}
