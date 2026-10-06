/*
 * simplecpp - A simple and high-fidelity C/C++ preprocessor library
 * Copyright (C) 2016-2023 simplecpp team
 */

#define SIMPLECPP_TOKENLIST_ALLOW_PTR 0
#include "simplecpp.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

static bool isDir(const std::string& path)
{
    struct stat file_stat;
    if (stat(path.c_str(), &file_stat) == -1)
        return false;

    return (file_stat.st_mode & S_IFMT) == S_IFDIR;
}

int main(int argc, char **argv)
{
    bool error = false;
    const char *filename = nullptr;
    enum : std::uint8_t {
        File,
        Fstream,
        Sstream,
        CharBuffer
    } toklist_inf = File;
    bool fail_on_error = false;
    bool linenrs = false;

    // Settings..
    simplecpp::DUI dui;
    dui.removeComments = true;
    bool quiet = false;
    bool error_only = false;
    for (int i = 1; i < argc; i++) {
        const char * const arg = argv[i];
        if (*arg == '-') {
            bool found = false;
            const char c = arg[1];
            switch (c) {
            case 'D': { // define symbol
                found = true;
                const char * const value = arg[2] ? (argv[i] + 2) : argv[++i];
                if (!value) {
                    std::cout << "error: option -D with no value." << std::endl;
                    error = true;
                    break;
                }
                dui.defines.emplace_back(value);
                break;
            }
            case 'U': { // undefine symbol
                found = true;
                const char * const value = arg[2] ? (argv[i] + 2) : argv[++i];
                if (!value) {
                    std::cout << "error: option -U with no value." << std::endl;
                    error = true;
                    break;
                }
                dui.undefined.insert(value);
                break;
            }
            case 'I': { // include path
                found = true;
                const char * const value = arg[2] ? (arg + 2) : argv[++i];
                if (!value) {
                    std::cout << "error: option -I with no value." << std::endl;
                    error = true;
                    break;
                }
                dui.includePaths.emplace_back(value);
                break;
            }
            case 'i':
                if (std::strncmp(arg, "-include=",9)==0) {
                    found = true;
                    std::string value = arg + 9;
                    if (value.empty()) {
                        std::cout << "error: option -include with no value." << std::endl;
                        error = true;
                        break;
                    }
                    dui.includes.emplace_back(std::move(value));
                }
                else if (std::strncmp(arg, "-input=",7)==0) {
                    found = true;
                    const std::string input = arg + 7;
                    if (input.empty()) {
                        std::cout << "error: option -input with no value." << std::endl;
                        error = true;
                        break;
                    }
                    if (input == "file") {
                        toklist_inf = File;
                    } else if (input == "fstream") {
                        toklist_inf = Fstream;
                    } else if (input == "sstream") {
                        toklist_inf = Sstream;
                    } else if (input == "buffer") {
                        toklist_inf = CharBuffer;
                    } else {
                        std::cout << "error: unknown input type '" << input << "'." << std::endl;
                        error = true;
                        break;
                    }
                }
                break;
            case 's':
                if (std::strncmp(arg, "-std=",5)==0) {
                    found = true;
                    std::string value = arg + 5;
                    if (value.empty()) {
                        std::cout << "error: option -std with no value." << std::endl;
                        error = true;
                        break;
                    }
                    dui.std = std::move(value);
                }
                break;
            case 'q':
                if (std::strcmp(arg, "-q")==0) {
                    found = true;
                    quiet = true;
                }
                break;
            case 'e':
                if (std::strcmp(arg, "-e")==0) {
                    found = true;
                    error_only = true;
                }
                break;
            case 'f':
                if (std::strcmp(arg, "-f")==0) {
                    fail_on_error = true;
                    found = true;
                }
                break;
            case 'l':
                if (std::strcmp(arg, "-l")==0) {
                    linenrs = true;
                    found = true;
                }
                break;
            case 'P':
                if (std::strcmp(arg, "-P")==0) {
                    dui.ignoreLineDirectives = true;
                    found = true;
                }
                break;
            }
            if (!found) {
                std::cout << "error: option '" << arg << "' is unknown." << std::endl;
                error = true;
            }
        } else if (filename) {
            std::cout << "error: multiple filenames specified" << std::endl;
            return 1;
        } else {
            filename = arg;
        }
    }

    if (error)
        return 1;

    if (quiet && error_only) {
        std::cout << "error: -e cannot be used in conjunction with -q" << std::endl;
        return 1;
    }

    if (!filename) {
        std::cout << "Syntax:" << std::endl;
        std::cout << "simplecpp [options] filename" << std::endl;
        std::cout << "  -DNAME          Define NAME." << std::endl;
        std::cout << "  -IPATH          Include path." << std::endl;
        std::cout << "  -include=FILE   Include FILE." << std::endl;
        std::cout << "  -UNAME          Undefine NAME." << std::endl;
        std::cout << "  -std=STD        Specify standard." << std::endl;
        std::cout << "  -q              Quiet mode (no output)." << std::endl;
        std::cout << "  -input=INPUT    Specify input type - file (default), fstream, sstream, buffer." << std::endl;
        std::cout << "  -e              Output errors only." << std::endl;
        std::cout << "  -f              Fail when errors were encountered (exitcode 1)." << std::endl;
        std::cout << "  -l              Print lines numbers." << std::endl;
        std::cout << "  -P              Ignore #line / # <num> \"file\" directives (do not let them affect location tracking)." << std::endl;
        return 0;
    }

    // TODO: move this logic into simplecpp
    bool inp_missing = false;

    for (const std::string& inc : dui.includes) {
        std::ifstream f(inc);
        if (!f.is_open() || isDir(inc)) {
            inp_missing = true;
            std::cout << "error: could not open include '" << inc << "'" << std::endl;
        }
    }

    for (const std::string& inc : dui.includePaths) {
        if (!isDir(inc)) {
            inp_missing = true;
            std::cout << "error: could not find include path '" << inc << "'" << std::endl;
        }
    }

    std::ifstream f(filename);
    if (!f.is_open() || isDir(filename)) {
        inp_missing = true;
        std::cout << "error: could not open file '" << filename << "'" << std::endl;
    }

    if (inp_missing)
        return 1;

    // Perform preprocessing
    simplecpp::OutputList outputList;
    std::vector<std::string> files;
    simplecpp::TokenList outputTokens(files);
    {
        simplecpp::TokenList *rawtokens;
        if (toklist_inf == Fstream) {
            rawtokens = new simplecpp::TokenList(f,files,filename,dui,&outputList);
        }
        else if (toklist_inf == Sstream || toklist_inf == CharBuffer) {
            std::ostringstream oss;
            oss << f.rdbuf();
            f.close();
            const std::string s = oss.str();
            if (toklist_inf == Sstream) {
                std::istringstream iss(s);
                rawtokens = new simplecpp::TokenList(iss,files,filename,dui,&outputList);
            }
            else {
                rawtokens = new simplecpp::TokenList({s.data(),s.size()},files,filename,dui,&outputList);
            }
        } else {
            f.close();
            rawtokens = new simplecpp::TokenList(filename,files,dui,&outputList);
        }
        if (dui.removeComments)
            rawtokens->removeComments();
        simplecpp::FileDataCache filedata;
        simplecpp::preprocess(outputTokens, *rawtokens, files, filedata, dui, &outputList);
        simplecpp::cleanup(filedata);
        delete rawtokens;
    }

    // Output
    if (!quiet) {
        if (!error_only)
            std::cout << outputTokens.stringify(linenrs) << std::endl;

        for (const simplecpp::Output &output : outputList) {
            std::cerr << outputTokens.file(output.location) << ':' << output.location.line << ": ";
            switch (output.type) {
            case simplecpp::Output::ERROR:
                std::cerr << "#error: ";
                break;
            case simplecpp::Output::WARNING:
                std::cerr << "#warning: ";
                break;
            case simplecpp::Output::MISSING_HEADER:
                std::cerr << "missing header: ";
                break;
            case simplecpp::Output::INCLUDE_NESTED_TOO_DEEPLY:
                std::cerr << "include nested too deeply: ";
                break;
            case simplecpp::Output::SYNTAX_ERROR:
                std::cerr << "syntax error: ";
                break;
            case simplecpp::Output::DIRECTIVE_AS_MACRO_PARAMETER:
                std::cerr << "directive as macro parameter: ";
                break;
            case simplecpp::Output::PORTABILITY_BACKSLASH:
            case simplecpp::Output::PORTABILITY_LINE_DIRECTIVE:
            case simplecpp::Output::PORTABILITY_NO_EOF_NEWLINE:
                std::cerr << "portability: ";
                break;
            case simplecpp::Output::UNHANDLED_CHAR_ERROR:
                std::cerr << "unhandled char error: ";
                break;
            case simplecpp::Output::EXPLICIT_INCLUDE_NOT_FOUND:
                std::cerr << "explicit include not found: ";
                break;
            case simplecpp::Output::FILE_NOT_FOUND:
                std::cerr << "file not found: ";
                break;
            case simplecpp::Output::DUI_ERROR:
                std::cerr << "dui error: ";
                break;
            }
            std::cerr << output.msg << std::endl;
        }
    }

    if (fail_on_error && !outputList.empty())
        return 1;

    return 0;
}
