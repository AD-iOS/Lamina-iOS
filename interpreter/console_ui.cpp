#include "console_ui.hpp"
#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "utils/repl_input.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <typeinfo>

#ifdef _WIN32
#include <windows.h>
#include <winnls.h>
#endif

#include "lamina_api/lamina.hpp"
#include "utils/color_style.hpp"
#include "version.hpp"


#include <string>

/* AD Library */
#include "include/ad_system.hpp"

int exec_block(const BlockStmt* block) {
    Interpreter interpreter;
    // 每个语句独立执行并捕获异常，但保持解释器状态
    int currentLine = 0;
    for (auto& stmt: block->statements) {
        currentLine++;
        try {
            Interpreter::execute(stmt);
        } catch (const RuntimeError& re) {
            // Use stack trace for runtime errors
            Interpreter::print_stack_trace(re, true);
        } catch (const ReturnException&) {
            // Catch and show meaningful info (return outside function causes this exception)
            Interpreter::print_warning("Return statement used outside function (line " + std::to_string(currentLine) + ")", true);
        } catch (const BreakException&) {
            // Catch and show meaningful info
            Interpreter::print_warning("Break statement used outside loop (line " + std::to_string(currentLine) + ")", true);
        } catch (const ContinueException&) {
            // Catch and show meaningful info
            Interpreter::print_warning("Continue statement used outside loop (line " + std::to_string(currentLine) + ")", true);
        } catch (const std::exception& e) {
            // Other standard exceptions
            Interpreter::print_error(std::string(e.what()) + " (line " + std::to_string(currentLine) + ")", true);
            std::cerr << "Type: " << typeid(e).name() << std::endl;
        } catch (...) {
            // Unknown exceptions
            Interpreter::print_error("Unknown error (line " + std::to_string(currentLine) + ")", true);
        }
    }
    return 0;
}


int run_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Unable to open file: " << path << std::endl;
        return 1;
    }
    std::cout << "Executing file: " << path << std::endl;
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    auto tokens = Lexer::tokenize(source);
    auto parser = std::make_shared<Parser>(tokens);
    auto ast = parser->parse_program();

    // 新的自動加載,自動加載 /usr/lib/lamina/module or /var/jb/usr/lib/lamina/module
    /*
    std::cout << "Loading minimal module..." << std::endl;
    try {
        std::unique_ptr<ModuleLoader> moduleLoader;
        moduleLoader = std::make_unique<ModuleLoader>("/usr/lib/lamina/module/minimal.dylib");

        if (!moduleLoader->isLoaded()) {
            moduleLoader = std::make_unique<ModuleLoader>("/var/jb/usr/lib/lamina/module/minimal.dylib");
        }

        if (!moduleLoader->isLoaded()) {
            moduleLoader = std::make_unique<ModuleLoader>("minimal.dylib");
        }

        if (moduleLoader->isLoaded()) {
            std::cout << "Module loaded successfully, registering to interpreter..." << std::endl;
            if (moduleLoader->registerToInterpreter(interpreter)) {
                std::cout << "Module registered successfully!" << std::endl;
            } else {
                std::cerr << "Failed to register module to interpreter!" << std::endl;
            }
        } else {
            std::cerr << "Failed to load module!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during module loading: " << e.what() << std::endl;
    }
    */

    // 舊版本實現
    // 注释掉自动加载minimal模块，改为按需加载
    // std::cout << "Loading minimal module..." << std::endl;
    // try {
    //     // 首先尝试从extensions目录加载
    //     auto moduleLoader = std::make_unique<ModuleLoader>("../extensions/minimal.dll");
    //     if (!moduleLoader->isLoaded()) {
    //         // 如果失败，尝试从当前目录加载
    //         moduleLoader = std::make_unique<ModuleLoader>("minimal.dll");
    //     }
    //     if (moduleLoader->isLoaded()) {
    //         std::cout << "Module loaded successfully, registering to interpreter..." << std::endl;
    //         if (moduleLoader->registerToInterpreter(interpreter)) {
    //             std::cout << "Module registered successfully!" << std::endl;
    //         } else {
    //             std::cerr << "Failed to register module to interpreter!" << std::endl;
    //         }
    //     } else {
    //         std::cerr << "Failed to load module!" << std::endl;
    //     }
    // } catch (const std::exception& e) {
    //     std::cerr << "Exception during module loading: " << e.what() << std::endl;
    // }

    if (ast.empty()) {
        // print_traceback(path, 1);
        // return 2;
        std::cout << "[Nothing to execute]" << std::endl;
    }

    // 只支持 BlockStmt
    auto block = std::make_unique<BlockStmt>(std::move(ast));
    if (!block) return 1;

    exec_block(block.get());
    std::cout << "\nProgram execution completed." << std::endl;
    return 0;
}

void enable_ansi_escape() {
#ifdef _WIN32
    // Windows 平台的颜色支持
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD outMode = 0;
        if (GetConsoleMode(hOut, &outMode)) {
            // 只有在成功获取控制台模式时才尝试设置
            if (!SetConsoleMode(hOut, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                // 设置失败只是警告，不影响程序运行
                std::cerr << "Warning: Console does not support ANSI color (STD_OUTPUT)" << std::endl;
            }
        } else {
            // 获取控制台模式失败（可能在测试环境或无控制台情况）
            // 静默失败，不输出错误信息，以免影响测试
        }
    }

    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD errMode = 0;
        if (GetConsoleMode(hErr, &errMode)) {
            if (!SetConsoleMode(hErr, errMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                std::cerr << "Warning: Console does not support ANSI color (STD_ERROR)" << std::endl;
            }
        }
        // 获取模式失败时静默处理
    }

    // UTF-8 编码设置
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

int argv_parser(const int argc, const char* const argv[]) {
    if (argc < 2) {
        return repl();
    }

    const std::vector<std::string> introduction = {"run", "version", "help", "repl"};
    std::vector<std::string> arguments;
    for (int i = 0; i < argc; ++i) {
        arguments.emplace_back(argv[i]);// char* => string
    }

    bool is_valid = false;
    for (const auto& cmd: introduction) {
        if (arguments[1] == cmd) {
            is_valid = true;
            break;
        }
    }

    if (!is_valid && arguments.size() == 2) {
        // if argv[1] not in introduction then run the argv[1](as a path)
        const std::string path = argv[1];
        arguments[1] = "run";
        arguments.emplace_back(path);
    }

    if (arguments[1] == "run") {
        if (arguments.size() != 3) {
            std::cout << "'run' command need 1 arguments but " << arguments.size() << "was given" << std::endl;
        }
        return run_file(arguments[2]);
    }

    if (arguments[1] == "version") {
        std::cout << LAMINA_VERSION << std::endl;
        return 0;
    }

    if (arguments[1] == "help") {
        print_help();
        return 0;
    }

    if (arguments[1] == "repl") {
        try {
            return repl();
        } catch (const CtrlCException&) {
            // Catch Ctrl+C exception and exit gracefully
            std::cout << "Program received Ctrl+C signal, exited safely." << std::endl;
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Program exception occurred: " << e.what() << std::endl;
            return 1;
        } catch (...) {
            std::cerr << "Program encountered an unknown exception." << std::endl;
            return 1;
        }
    }

    std::cout << "Unknown command: " << argv[1] << std::endl;
    print_help();
    return 1;
}


int repl() {
    std::cout << "Lamina REPL v" << LAMINA_VERSION << ".\nPress Ctrl+C or :exit to exit." << std::endl;
    std::cout << "Type :help for help." << std::endl;

    Interpreter interpreter;
    int lineno = 1; // 记录当前输入的行号，用于错误提示

    std::string code_buffer;
    int brace_level = 0;

    while (true) {
        try {
            // Set prompt style based on whether the terminal supports color
            std::string prompt;
            if (brace_level > 0) {
                prompt = ConClr::MAGENTA + "... " + ConClr::RESET ;    // 多行输入下等待输入的提示符
            } else {
                prompt = ConClr::MAGENTA + ">>> " + ConClr::RESET ;   // 带绿色高亮的提示符
            }
            std::string line;
            // repl readline有问题，这里暂时使用std::getline
            // std::string line = repl_readline(prompt,"");
            std::cout << prompt;
            std::getline(std::cin, line);

            if (line == "\x03") {   // Ctrl+C interrupt
                if (brace_level > 0) {
                    code_buffer.clear();
                    brace_level = 0;
                    std::cout << "\nKeyboardInterrupt" << std::endl;
                    continue;
                }
                std::cout << "\n";
                break;
            }

            bool execute_now = false;
            if (line == "\x04") {   // Ctrl+D (EOF)
                if (brace_level > 0) {
                    // In multi-line mode, Ctrl+D means end of input, execute code immediately
                    execute_now = true;
                } else {
                    // On an empty line at the main prompt, Ctrl+D exits
                    break;
                }
            } else {
                code_buffer += line + "\n";

                // Simply check if the code block is complete by counting braces
                for (const char c: line) {
                    if (c == '{') {
                        brace_level++;
                    } else if (c == '}') {
                        brace_level--;
                    }
                }

                if (brace_level <= 0) {
                    execute_now = true;
                }
            }

            if (!execute_now) {
                ++lineno;
                continue;
            }

            brace_level = 0;    // Reset

            if (code_buffer.empty() || code_buffer == "\n") {
                ++lineno;
                code_buffer.clear();
                continue;
            }

            std::string line_to_process = code_buffer;
            code_buffer.clear();

            // Only process REPL commands in single-line mode
            if (brace_level == 0) {
                std::string trimmed_line = line;
                trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t\n\r"));
                trimmed_line.erase(trimmed_line.find_last_not_of(" \t\n\r") + 1);

                if (trimmed_line == ":exit") {
                    break;
                }
                if (trimmed_line == ":help") {
                    std::cout << "Lamina Interpreter Commands:\n";
                    std::cout << "  :exit - Exit interpreter\n";
                    std::cout << "  :help - Show this help message\n";
                    std::cout << "  :vars - Show all variables\n";
                    std::cout << "  :clear - Clear screen\n";
                    std::cout << "  :nouse_color - no use the color for output\n";
                    std::cout << "  :use_color - use color for output(default option)\n";
                    ++lineno;
                    continue;
                }
                if (trimmed_line == ":vars") {
                    Interpreter::print_variables();
                    ++lineno;
                    continue;
                }
                if (trimmed_line == ":nouse_color"){
                    ConClr::init(false); // 设置不使用颜色
                }
                if (trimmed_line == ":use_color"){
                    ConClr::init(true); // 设置使用颜色
                }
                if (trimmed_line == ":clear") {
#ifdef _WIN32
                    [[maybe_unused]] auto result = ad_bash_system("cls");    // Windows下使用cls命令
#else
                    [[maybe_unused]] auto result = ad_bash_system("clear");  // Linux/macOS下使用clear命令
#endif
                    ++lineno;
                    continue;
                }
            }

            // frontend
            auto tokens = Lexer::tokenize(line_to_process);
            const auto parser = std::make_shared<Parser>(tokens);
            auto asts = parser->parse_program();

            // Check if AST generation succeeded
            if (asts.empty()) {
                // print_traceback("<stdin>", lineno);
                // return 1;
                std::cout << "[Nothing to execute]" << std::endl;
            }

            // Only support AST of type BlockStmt (block statement)
            auto block = std::make_unique<BlockStmt>(std::move(asts));

            try {
                // Execute each statement in the block
                for (auto& stmt: block->statements) {
                    try {
                        const auto result = Interpreter::execute(stmt);
                        if (! result.is_null()) {
                            std::cout << result.to_string() << std::endl;
                        }
                    } catch (const RuntimeError& re) {
                        Interpreter::print_stack_trace(re, true);
                        break;
                    } catch (const ReturnException&) {
                        Interpreter::print_warning(
                                "Return statement used outside function (line " +
                                        std::to_string(lineno) + ")",
                                true);
                        break;
                    }

                    // Catch break statement used outside loop
                    catch (const BreakException&) {
                        Interpreter::print_warning(
                                "Break statement used outside loop (line " + std::to_string(lineno) + ")",
                                true);
                        break;
                    }

                    // Catch continue statement used outside loop
                    catch (const ContinueException&) {
                        Interpreter::print_warning(
                                "Continue statement used outside loop (line " +
                                        std::to_string(lineno) + ")",
                                true);
                        break;
                    }

                    // Catch other standard exceptions
                    catch (const std::exception& e) {
                        Interpreter::print_error(e.what(), true);
                        break;
                    }
                }
            }

            // Catch top-level runtime errors during block execution
            catch (const RuntimeError& re) {
                Interpreter::print_stack_trace(re, true);
            }

            catch (StdLibException) {
                continue;
            }

            // Catch return exceptions escaping the inner loop
            catch (const ReturnException&) {
                Interpreter::print_warning(
                        "Return statement used outside function",
                        true);
            }
            // Catch all other unexpected exceptions
            catch (...) {
                Interpreter::print_error("Unknown error occurred", true);
            }
            // 保存AST以保持函数指针有效
            Interpreter::save_repl_ast(std::move(block));
        }
        catch (StdLibException& e) {
            Interpreter::print_error(
                    e.what(),
                    true);
        }
        // Catch and handle all exceptions in REPL loop to prevent crashes
        catch (...) {
            Interpreter::print_error(
                    "REPL environment caught exception, but recovered",
                    true);
        }


        ++lineno;   // 行号递增
    }

    return 0;
}
