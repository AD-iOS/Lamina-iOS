/*    AD-DEV Public General License
 *       Version 1.0.0, December 2025
 *
 *  Copyright (c) 2025-2026 AD-iOS (1107154510@qq.com) All rights reserved.
 *
 *  (Note: AD, AD-dev, and AD-iOS refer to the same person. Email: 1107154510@qq.com)
 *
 *  Hereinafter, the AD-DEV Public General License is referred to as this Agreement or this License. The original source code, executable binaries, and related documentation are collectively referred to as the Software. Use for profit, including sales, leasing, advertising support, etc., is referred to as Commercial Use. Works modified or extended based on the Software are referred to as Derivative Products.
 *
 *  We hereby grant you the following rights, subject to the following conditions, a worldwide, royalty-free, non-exclusive, irrevocable license to:
 *    1. Use, copy, and modify the Software
 *    2. Create derivative works
 *    3. Distribute the Software and derivative works
 *
 *  Conditions:
 *    1. Retain the original author's license and copyright notices
 *    2. The original author assumes no liability
 *    3. Unauthorized commercial use is prohibited
 *    4. All modifications and derivative products based on the Software must use this License and disclose the original source code
 *    5. Disclosure of the original source code is not required when using the Software unmodified (e.g., via APIs, libraries, automatic linkers, etc.)
 *
 *  Anyone may distribute or copy this License file, but may not modify it.
 *  For commercial use, please contact the maintainer of the Software to obtain written and electronic authorization. Note that commercial licensing may involve fees; specific terms shall be negotiated separately.
 *  Violation of this Agreement will automatically terminate the license. Upon termination, all copies of the Software must be destroyed, use of the Software must cease, derivative products must be withdrawn from distribution channels, and distribution of derivative products must stop.
 *
 *  The software is provided "as is", without warranty of any kind, express or implied. In no event shall the authors or copyright holders be liable for any claim, damages, or other liability arising from the use of the software.
 *
 *  If any provision is invalid, it does not affect the validity of the other provisions.
 *  By using, distributing, or modifying the Software, you automatically agree to and intend to comply with this Agreement. If you cannot comply, you must stop using, distributing, or modifying the Software.
 *  This Agreement is governed by and construed in accordance with the laws of the People's Republic of China (without regard to conflict of law principles).
 */
/*
 * ad_system.hpp
 * AD‘s C++ private standard library
 * Created by AD on 5/11/25
 * Copyright (c) 2025-2026 AD All rights reserved.
 *
*/

#ifndef AD_SYSTEM_HPP
#define AD_SYSTEM_HPP

#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>

inline int ad_system_impl(const std::string& command, const char* shell_path, const char* shell_name);

inline int ad_bash_system(const std::string& command);
inline int ad_zsh_system(const std::string& command);
inline int ad_dash_system(const std::string& command);
inline int ad_fish_system(const std::string& command);

// Use inline functions to create aliases
inline int ad_bash_sys(const std::string& command) {
    return ad_bash_system(command);
}

inline int ad_zsh_sys(const std::string& command) {
    return ad_zsh_system(command);
}

inline int ad_dash_sys(const std::string& command) {
    return ad_dash_system(command);
}

inline int ad_fish_sys(const std::string& command) {
    return ad_fish_system(command);
}

inline int ad_system_impl(const std::string& command, const char* shell_path, const char* shell_name) {
    if (command.empty()) {
        return 1;
    }

    int status = 0;
    pid_t pid = fork();

    if (pid == -1) {
        return -1;
    } else if (pid == 0) {
        execl(shell_path, shell_name, "-c", command.c_str(), (char *)NULL);
        _exit(127);
    } else {
        if (waitpid(pid, &status, 0) == -1) {
            return -1;
        }
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return -1;
        }
    }
}

inline int ad_bash_system(const std::string& command) {
    const char* possible_paths[] = {
        /* jailbroken */
        "/var/jb/bin/bash",
        "/var/jb/usr/bin/bash",
        "/var/jb/usr/local/bin/bash",
        /* system */
        "/bin/bash",
        "/usr/bin/bash",
        "/usr/local/bin/bash",
        NULL
    };
    
    for (int i = 0; possible_paths[i] != NULL; i++) {
        if (access(possible_paths[i], F_OK) == 0) {
            return ad_system_impl(command, possible_paths[i], "bash");
        }
    }

    return ad_system_impl(command, "/bin/bash", "bash");
}

inline int ad_zsh_system(const std::string& command) {
    const char* possible_paths[] = {
        /* jailbroken */
        "/var/jb/bin/zsh",
        "/var/jb/usr/bin/zsh",
        "/var/jb/usr/local/bin/zsh",
        /* system */
        "/bin/zsh",
        "/usr/bin/zsh",
        "/usr/local/bin/zsh",
        NULL
    };
    
    for (int i = 0; possible_paths[i] != NULL; i++) {
        if (access(possible_paths[i], F_OK) == 0) {
            return ad_system_impl(command, possible_paths[i], "zsh");
        }
    }

    return ad_system_impl(command, "/bin/zsh", "zsh");
}

inline int ad_fish_system(const std::string& command) {
    const char* possible_paths[] = {
        /* jailbroken */
        "/var/jb/bin/fish",
        "/var/jb/usr/bin/fish",
        "/var/jb/usr/local/bin/fish",
        /* system */
        "/bin/fish",
        "/usr/bin/fish",
        "/usr/local/bin/fish",
        NULL
    };
    
    for (int i = 0; possible_paths[i] != NULL; i++) {
        if (access(possible_paths[i], F_OK) == 0) {
            return ad_system_impl(command, possible_paths[i], "fish");
        }
    }

    return ad_system_impl(command, "/bin/fish", "fish");
}

inline int ad_dash_system(const std::string& command) {
    const char* possible_paths[] = {
        /* jailbroken */
        "/var/jb/bin/dash",
        "/var/jb/usr/bin/dash",
        "/var/jb/usr/local/bin/dash",
        /* system */
        "/bin/dash",
        "/usr/bin/dash",
        "/usr/local/bin/dash",
        NULL
    };
    
    for (int i = 0; possible_paths[i] != NULL; i++) {
        if (access(possible_paths[i], F_OK) == 0) {
            return ad_system_impl(command, possible_paths[i], "dash");
        }
    }

    return ad_system_impl(command, "/bin/dash", "dash");
}

#endif
