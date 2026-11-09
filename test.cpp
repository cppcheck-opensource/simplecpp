/*
 * simplecpp - A simple and high-fidelity C/C++ preprocessor library
 * Copyright (C) 2016-2023 simplecpp team
 */

#include "simplecpp.h"

#include <algorithm>

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef __cpp_lib_string_view
#include <string_view>
#endif

#ifndef SIMPLECPP_TEST_SOURCE_DIR
#error "SIMPLECPP_TEST_SOURCE_DIR is not defined."
#endif

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)

static const std::string testSourceDir = SIMPLECPP_TEST_SOURCE_DIR;

namespace {
    enum class Input : std::uint8_t {
        Stringstream,
        CharBuffer
    };
}

static Input USE_INPUT = Input::Stringstream;
/** the tests are run with the old macro expansion and with the new one with hide sets */
static bool USE_HIDESET_EXPANSION = false;
static int numberOfFailedAssertions = 0;
static int numberOfHideSetTodos = 0;

/** Skip the rest of the test when it is run with the new macro expansion, which can not handle it yet */
#define HIDESET_EXPANSION_TODO() do { if (USE_HIDESET_EXPANSION) { ++numberOfHideSetTodos; return; } } while (false)

#define ASSERT_EQUALS(expected, actual)  (assertEquals((expected), (actual), __LINE__))
#define ASSERT_THROW_EQUALS(stmt, e, expected) do { try { stmt; assertThrowFailed(__LINE__); } catch (const e& ex) { assertEquals((expected), (ex.what()), __LINE__); } } while (false)
#define TODO_ASSERT_EQUALS(wanted, current, actual)  (todoAssertEquals((wanted), (current), (actual), __LINE__))
/** A known bug of the old macro expansion that the hide-set expansion does not have */
#define TODO_OLD_EXPANSION_ASSERT_EQUALS(wanted, current, actual)  (todoOldExpansionAssertEquals((wanted), (current), (actual), __LINE__))

static std::string pprint(const std::string &in)
{
    std::string ret;
    for (std::string::size_type i = 0; i < in.size(); ++i) {
        if (in[i] == '\n')
            ret += "\\n";
        ret += in[i];
    }
    return ret;
}

static std::string inputString(Input input) {
    const std::string expansion = USE_HIDESET_EXPANSION ? ", hide-set expansion" : "";
    switch (input) {
    case Input::Stringstream:
        return "Stringstream" + expansion;
    case Input::CharBuffer:
        return "CharBuffer" + expansion;
    }
    return ""; // unreachable - needed for GCC and Visual Studio
}

static int assertEquals(const std::string &expected, const std::string &actual, int line)
{
    if (expected != actual) {
        numberOfFailedAssertions++;
        std::cerr << "------ assertion failed (" << inputString(USE_INPUT) << ")---------" << std::endl;
        std::cerr << "line test.cpp:" << line << std::endl;
        std::cerr << "expected:" << pprint(expected) << std::endl;
        std::cerr << "actual:" << pprint(actual) << std::endl;
    }
    return (expected == actual);
}

static int assertEquals(const long long &expected, const long long &actual, int line)
{
    return assertEquals(std::to_string(expected), std::to_string(actual), line);
}

/**
 * For a known bug: actual must be the current (wrong) value. When the bug is
 * fixed and actual is the wanted value, the assertion fails so that it is
 * changed to ASSERT_EQUALS.
 */
static void todoAssertEquals(const std::string &wanted, const std::string &current, const std::string &actual, int line)
{
    if (wanted == actual) {
        numberOfFailedAssertions++;
        std::cerr << "------ TODO assertion fixed (" << inputString(USE_INPUT) << ")---------" << std::endl;
        std::cerr << "line test.cpp:" << line << std::endl;
        std::cerr << "change TODO_ASSERT_EQUALS to ASSERT_EQUALS" << std::endl;
        return;
    }
    assertEquals(current, actual, line);
}

static void todoOldExpansionAssertEquals(const std::string &wanted, const std::string &current, const std::string &actual, int line)
{
    if (USE_HIDESET_EXPANSION)
        assertEquals(wanted, actual, line);
    else
        todoAssertEquals(wanted, current, actual, line);
}

static void assertThrowFailed(int line)
{
    numberOfFailedAssertions++;
    std::cerr << "------ assertion failed ---------" << std::endl;
    std::cerr << "line " << line << std::endl;
    std::cerr << "exception not thrown" << std::endl;
}

static void testcase(const std::string &name, void (*f)(), int argc, char * const *argv)
{
    if (argc == 1) {
        f();
    }
    else {
        for (int i = 1; i < argc; i++) {
            if (name == argv[i])
                f();
        }
    }
}

#define TEST_CASE(F)    (testcase(#F, F, argc, argv))

/** simplecpp::preprocess() with the macro expansion selected by USE_HIDESET_EXPANSION */
static void runPreprocess(simplecpp::TokenList &output, const simplecpp::TokenList &rawtokens, std::vector<std::string> &files, simplecpp::FileDataCache &cache, const simplecpp::DUI &dui, simplecpp::OutputList *outputList = nullptr, std::list<simplecpp::MacroUsage> *macroUsage = nullptr, std::list<simplecpp::IfCond> *ifCond = nullptr)
{
    simplecpp::DUI dui2(dui);
    dui2.hideSetExpansion = USE_HIDESET_EXPANSION;
    simplecpp::preprocess(output, rawtokens, files, cache, dui2, outputList, macroUsage, ifCond);
}

static simplecpp::TokenList makeTokenList(const char code[], std::size_t size, std::vector<std::string> &filenames, const std::string &filename=std::string(), const simplecpp::DUI &dui = {}, simplecpp::OutputList *outputList = nullptr)
{
    switch (USE_INPUT) {
    case Input::Stringstream: {
        std::istringstream istr(std::string(code, size));
        return {istr,filenames,filename,dui,outputList};
    }
    case Input::CharBuffer:
        return {{code, size}, filenames, filename, dui, outputList};
    }

    return simplecpp::TokenList{filenames}; // unreachable - needed for GCC and Visual Studio
}

static simplecpp::TokenList makeTokenList(const char code[], std::vector<std::string> &filenames, const std::string &filename=std::string(), const simplecpp::DUI &dui = {}, simplecpp::OutputList *outputList=nullptr)
{
    return makeTokenList(code, strlen(code), filenames, filename, dui, outputList);
}

static simplecpp::TokenList makeTokenList(const char code[], const simplecpp::DUI &dui = {}, simplecpp::OutputList *outputList=nullptr)
{
    std::vector<std::string> files;
    return makeTokenList(code, strlen(code), files, std::string(), dui, outputList);
}

static std::string readfile(const char code[], const simplecpp::DUI &dui = {}, simplecpp::OutputList *outputList = nullptr)
{
    std::vector<std::string> files;
    return makeTokenList(code,files,std::string(),dui,outputList).stringify();
}

static std::string readfile(const char code[], std::size_t size, const simplecpp::DUI &dui = {}, simplecpp::OutputList *outputList=nullptr)
{
    std::vector<std::string> files;
    return makeTokenList(code,size,files,std::string(),dui,outputList).stringify();
}

/** Write the code to a temporary file and read it with TokenList(filename), so FileStream is used */
static std::string readfileFromFile(const char code[], std::size_t size, simplecpp::OutputList *outputList = nullptr)
{
    const std::string filename = "testrunner_filestream.tmp";
    {
        std::ofstream f(filename, std::ios::binary);
        f.write(code, static_cast<std::streamsize>(size));
    }
    std::vector<std::string> files;
    std::string ret = simplecpp::TokenList(filename, files, {}, outputList).stringify();
    std::remove(filename.c_str());
    return ret;
}

static std::string preprocess(const char code[], std::size_t size, const simplecpp::DUI &dui, simplecpp::OutputList *outputList, std::list<simplecpp::MacroUsage> *macroUsage = nullptr, std::list<simplecpp::IfCond> *ifCond = nullptr, const std::string &file = std::string())
{
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    simplecpp::TokenList tokens = makeTokenList(code, size, files, file, dui, outputList);
    if (dui.removeComments)
        tokens.removeComments();
    simplecpp::TokenList tokens2(files);
    runPreprocess(tokens2, tokens, files, cache, dui, outputList, macroUsage, ifCond);
    simplecpp::cleanup(cache);
    return tokens2.stringify();
}

static std::string preprocess(const char code[], const simplecpp::DUI &dui, simplecpp::OutputList *outputList, std::list<simplecpp::MacroUsage> *macroUsage = nullptr, std::list<simplecpp::IfCond> *ifCond = nullptr, const std::string &file = std::string())
{
    return preprocess(code, strlen(code), dui, outputList, macroUsage, ifCond, file);
}

static std::string preprocess(const char code[])
{
    return preprocess(code, simplecpp::DUI(), nullptr);
}

static std::string preprocess(const char code[], const std::string &file)
{
    return preprocess(code, simplecpp::DUI(), nullptr, nullptr, nullptr, file);
}

static std::string preprocess(const char code[], const simplecpp::DUI &dui)
{
    return preprocess(code, dui, nullptr);
}

static std::string preprocess(const char code[], simplecpp::OutputList *outputList)
{
    return preprocess(code, simplecpp::DUI(), outputList);
}

static std::string preprocess(const char code[], std::size_t size, simplecpp::OutputList *outputList)
{
    return preprocess(code, size, simplecpp::DUI(), outputList);
}

static std::string preprocess(const char code[], std::list<simplecpp::IfCond> *ifCond)
{
    return preprocess(code, simplecpp::DUI(), nullptr, nullptr, ifCond);
}

static std::string preprocess(const char code[], std::list<simplecpp::MacroUsage> *macroUsage)
{
    return preprocess(code, simplecpp::DUI(), nullptr, macroUsage);
}

static std::string toString(const simplecpp::OutputList &outputList)
{
    std::ostringstream ostr;
    for (const simplecpp::Output &output : outputList) {
        ostr << "file" << output.location.fileIndex << ',' << output.location.line << ',';

        switch (output.type) {
        case simplecpp::Output::Type::ERROR:
            ostr << "#error,";
            break;
        case simplecpp::Output::Type::WARNING:
            ostr << "#warning,";
            break;
        case simplecpp::Output::Type::MISSING_HEADER:
            ostr << "missing_header,";
            break;
        case simplecpp::Output::Type::INCLUDE_NESTED_TOO_DEEPLY:
            ostr << "include_nested_too_deeply,";
            break;
        case simplecpp::Output::Type::SYNTAX_ERROR:
            ostr << "syntax_error,";
            break;
        case simplecpp::Output::DIRECTIVE_AS_MACRO_PARAMETER:
            ostr << "directive_as_macro_parameter,";
            break;
        case simplecpp::Output::Type::PORTABILITY_BACKSLASH:
            ostr << "portability_backslash,";
            break;
        case simplecpp::Output::Type::PORTABILITY_LINE_DIRECTIVE:
            ostr << "portability_line_directive,";
            break;
        case simplecpp::Output::Type::PORTABILITY_NO_EOF_NEWLINE:
            ostr << "portability_no_eof_newline,";
            break;
        case simplecpp::Output::Type::PORTABILITY_STRINGIFY:
            ostr << "portability_stringify,";
            break;
        case simplecpp::Output::Type::UNHANDLED_CHAR_ERROR:
            ostr << "unhandled_char_error,";
            break;
        case simplecpp::Output::Type::EXPLICIT_INCLUDE_NOT_FOUND:
            ostr << "explicit_include_not_found,";
            break;
        case simplecpp::Output::Type::FILE_NOT_FOUND:
            ostr << "file_not_found,";
            break;
        case simplecpp::Output::Type::DUI_ERROR:
            ostr << "dui_error,";
            break;
        }

        ostr << output.msg << '\n';
    }
    return ostr.str();
}

// Preprocess the code. Each output token is written as str(line:col), or
// str@macro(line:col) when it comes from a macro expansion.
static std::string expandedTokens(const char code[], const simplecpp::DUI &dui = simplecpp::DUI(), std::list<simplecpp::MacroUsage> *macroUsage = nullptr)
{
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    simplecpp::TokenList tokens(files);
    const simplecpp::TokenList rawtokens = makeTokenList(code, files);
    runPreprocess(tokens, rawtokens, files, cache, dui, nullptr, macroUsage);
    std::string ret;
    for (const simplecpp::Token *tok = tokens.cfront(); tok; tok = tok->next) {
        if (!ret.empty())
            ret += ' ';
        ret += tok->str();
        if (!tok->macro.empty())
            ret += '@' + tok->macro;
        ret += '(' + std::to_string(tok->location.line) + ':' + std::to_string(tok->location.col) + ')';
    }
    return ret;
}

/**
 * Each macro usage as "name line:col use line:col", with " unknown" if the value is not known.
 * The lines are sorted, the order of the macro usage depends on the standard library.
 */
static std::string toString(const std::list<simplecpp::MacroUsage> &macroUsage)
{
    std::vector<std::string> lines;
    for (const simplecpp::MacroUsage &mu : macroUsage) {
        std::string line = mu.macroName + ' ' + std::to_string(mu.macroLocation.line) + ':' + std::to_string(mu.macroLocation.col) +
                           " use " + std::to_string(mu.useLocation.line) + ':' + std::to_string(mu.useLocation.col);
        if (!mu.macroValueKnown)
            line += " unknown";
        lines.push_back(std::move(line));
    }
    std::sort(lines.begin(), lines.end());
    std::string ret;
    for (const std::string &line : lines)
        ret += line + '\n';
    return ret;
}

static void backslash()
{
    // <backslash><space><newline> preprocessed differently
    simplecpp::OutputList outputList;

    readfile("//123 \\\n456\n", {}, &outputList);
    ASSERT_EQUALS("", toString(outputList));
    readfile("//123 \\ \n456\n", {}, &outputList);
    ASSERT_EQUALS("file0,1,portability_backslash,Combination 'backslash space newline' is not portable.\n", toString(outputList));

    outputList.clear();
    readfile("#define A \\\n123\n", {}, &outputList);
    ASSERT_EQUALS("", toString(outputList));
    readfile("#define A \\ \n123\n", {}, &outputList);
    ASSERT_EQUALS("file0,1,portability_backslash,Combination 'backslash space newline' is not portable.\n", toString(outputList));
}

static void builtin()
{
    ASSERT_EQUALS("\"test.c\" 1 0", preprocess("__FILE__ __LINE__ __COUNTER__", "test.c"));
    ASSERT_EQUALS("\n\n3", preprocess("\n\n__LINE__"));
    ASSERT_EQUALS("\n\n0", preprocess("\n\n__COUNTER__"));
    ASSERT_EQUALS("\n\n0 1", preprocess("\n\n__COUNTER__ __COUNTER__"));

    ASSERT_EQUALS("\n0 + 0", preprocess("#define A(c)  c+c\n"
                                        "A(__COUNTER__)\n"));

    ASSERT_EQUALS("\n0 + 0 + 1", preprocess("#define A(c)  c+c+__COUNTER__\n"
                                            "A(__COUNTER__)\n"));
}

static std::string testConstFold(const char code[])
{
    try {
        std::vector<std::string> files;
        simplecpp::TokenList expr = makeTokenList(code, files);
        expr.constFold();
        return expr.stringify();
    } catch (std::exception &) {
        return "exception";
    }
}

static void characterLiteral()
{
    ASSERT_EQUALS('A', simplecpp::characterLiteralToLL("'A'"));

    ASSERT_EQUALS('\'', simplecpp::characterLiteralToLL("'\\''"));
    ASSERT_EQUALS('\"', simplecpp::characterLiteralToLL("'\\\"'"));
    ASSERT_EQUALS('\?', simplecpp::characterLiteralToLL("'\\?'"));
    ASSERT_EQUALS('\\', simplecpp::characterLiteralToLL("'\\\\'"));
    ASSERT_EQUALS('\a', simplecpp::characterLiteralToLL("'\\a'"));
    ASSERT_EQUALS('\b', simplecpp::characterLiteralToLL("'\\b'"));
    ASSERT_EQUALS('\f', simplecpp::characterLiteralToLL("'\\f'"));
    ASSERT_EQUALS('\n', simplecpp::characterLiteralToLL("'\\n'"));
    ASSERT_EQUALS('\r', simplecpp::characterLiteralToLL("'\\r'"));
    ASSERT_EQUALS('\t', simplecpp::characterLiteralToLL("'\\t'"));
    ASSERT_EQUALS('\v', simplecpp::characterLiteralToLL("'\\v'"));

    // GCC extension for ESC character
    ASSERT_EQUALS(0x1b, simplecpp::characterLiteralToLL("'\\e'"));
    ASSERT_EQUALS(0x1b, simplecpp::characterLiteralToLL("'\\E'"));

    // more obscure GCC extensions
    ASSERT_EQUALS('(', simplecpp::characterLiteralToLL("'\\('"));
    ASSERT_EQUALS('[', simplecpp::characterLiteralToLL("'\\['"));
    ASSERT_EQUALS('{', simplecpp::characterLiteralToLL("'\\{'"));
    ASSERT_EQUALS('%', simplecpp::characterLiteralToLL("'\\%'"));

    ASSERT_EQUALS('\0',   simplecpp::characterLiteralToLL("'\\0'"));
    ASSERT_EQUALS('\1',   simplecpp::characterLiteralToLL("'\\1'"));
    ASSERT_EQUALS('\10',  simplecpp::characterLiteralToLL("'\\10'"));
    ASSERT_EQUALS('\010', simplecpp::characterLiteralToLL("'\\010'"));
    ASSERT_EQUALS('\377', simplecpp::characterLiteralToLL("'\\377'"));

    ASSERT_EQUALS('\x0',  simplecpp::characterLiteralToLL("'\\x0'"));
    ASSERT_EQUALS('\x10', simplecpp::characterLiteralToLL("'\\x10'"));
    ASSERT_EQUALS('\xff', simplecpp::characterLiteralToLL("'\\xff'"));

    ASSERT_EQUALS('\u0012',     simplecpp::characterLiteralToLL("'\\u0012'"));
    ASSERT_EQUALS('\U00000012', simplecpp::characterLiteralToLL("'\\U00000012'"));

    ASSERT_EQUALS((static_cast<unsigned int>(static_cast<unsigned char>('b')) << 8) | static_cast<unsigned char>('c'),    simplecpp::characterLiteralToLL("'bc'"));
    ASSERT_EQUALS((static_cast<unsigned int>(static_cast<unsigned char>('\x23')) << 8) | static_cast<unsigned char>('\x45'), simplecpp::characterLiteralToLL("'\\x23\\x45'"));
    ASSERT_EQUALS((static_cast<unsigned int>(static_cast<unsigned char>('\11')) << 8) | static_cast<unsigned char>('\222'), simplecpp::characterLiteralToLL("'\\11\\222'"));
    ASSERT_EQUALS((static_cast<unsigned int>(static_cast<unsigned char>('\a')) << 8) | static_cast<unsigned char>('\b'),   simplecpp::characterLiteralToLL("'\\a\\b'"));
    if (sizeof(int) <= 4)
        ASSERT_EQUALS(-1, simplecpp::characterLiteralToLL("'\\xff\\xff\\xff\\xff'"));
    else
        ASSERT_EQUALS(0xffffffff, simplecpp::characterLiteralToLL("'\\xff\\xff\\xff\\xff'"));

    ASSERT_EQUALS('A', simplecpp::characterLiteralToLL("u8'A'"));
    ASSERT_EQUALS('A', simplecpp::characterLiteralToLL("u'A'"));
    ASSERT_EQUALS('A', simplecpp::characterLiteralToLL("L'A'"));
    ASSERT_EQUALS('A', simplecpp::characterLiteralToLL("U'A'"));

    ASSERT_EQUALS(0xff, simplecpp::characterLiteralToLL("u8'\\xff'"));
    ASSERT_EQUALS(0xff, simplecpp::characterLiteralToLL("u'\\xff'"));
    ASSERT_EQUALS(0xff, simplecpp::characterLiteralToLL("L'\\xff'"));
    ASSERT_EQUALS(0xff, simplecpp::characterLiteralToLL("U'\\xff'"));

    ASSERT_EQUALS(0xfedc,     simplecpp::characterLiteralToLL("u'\\xfedc'"));
    ASSERT_EQUALS(0xfedcba98, simplecpp::characterLiteralToLL("L'\\xfedcba98'"));
    ASSERT_EQUALS(0xfedcba98, simplecpp::characterLiteralToLL("U'\\xfedcba98'"));

    ASSERT_EQUALS(0x12,       simplecpp::characterLiteralToLL("u8'\\u0012'"));
    ASSERT_EQUALS(0x1234,     simplecpp::characterLiteralToLL("u'\\u1234'"));
    ASSERT_EQUALS(0x00012345, simplecpp::characterLiteralToLL("L'\\U00012345'"));
    ASSERT_EQUALS(0x00012345, simplecpp::characterLiteralToLL("U'\\U00012345'"));

#ifdef __GNUC__
    // BEGIN Implementation-specific results
    ASSERT_EQUALS('AB', simplecpp::characterLiteralToLL("'AB'"));
    ASSERT_EQUALS('ABC', simplecpp::characterLiteralToLL("'ABC'"));
    ASSERT_EQUALS('ABCD', simplecpp::characterLiteralToLL("'ABCD'"));
    ASSERT_EQUALS('\134t', simplecpp::characterLiteralToLL("'\\134t'")); // cppcheck ticket #7452
    // END Implementation-specific results
#endif

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("'\\9'"), std::runtime_error, "invalid escape sequence");

    // Input is manually encoded to (escaped) UTF-8 byte sequences
    // to avoid dependence on source encoding used for this file
    ASSERT_EQUALS(0xb5, simplecpp::characterLiteralToLL("U'\302\265'"));
    ASSERT_EQUALS(0x157, simplecpp::characterLiteralToLL("U'\305\227'"));
    ASSERT_EQUALS(0xff0f, simplecpp::characterLiteralToLL("U'\357\274\217'"));
    ASSERT_EQUALS(0x3042, simplecpp::characterLiteralToLL("U'\343\201\202'"));
    ASSERT_EQUALS(0x13000, simplecpp::characterLiteralToLL("U'\360\223\200\200'"));

    ASSERT_EQUALS(0xb5, simplecpp::characterLiteralToLL("L'\302\265'"));
    ASSERT_EQUALS(0x157, simplecpp::characterLiteralToLL("L'\305\227'"));
    ASSERT_EQUALS(0xff0f, simplecpp::characterLiteralToLL("L'\357\274\217'"));
    ASSERT_EQUALS(0x3042, simplecpp::characterLiteralToLL("L'\343\201\202'"));
    ASSERT_EQUALS(0x13000, simplecpp::characterLiteralToLL("L'\360\223\200\200'"));

    ASSERT_EQUALS(0xb5, simplecpp::characterLiteralToLL("u'\302\265'"));
    ASSERT_EQUALS(0x157, simplecpp::characterLiteralToLL("u'\305\227'"));
    ASSERT_EQUALS(0xff0f, simplecpp::characterLiteralToLL("u'\357\274\217'"));
    ASSERT_EQUALS(0x3042, simplecpp::characterLiteralToLL("u'\343\201\202'"));
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u'\360\223\200\200'"), std::runtime_error, "code point too large");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u8'\302\265'"), std::runtime_error, "code point too large");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u8'\305\227'"), std::runtime_error, "code point too large");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u8'\357\274\217'"), std::runtime_error, "code point too large");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u8'\343\201\202'"), std::runtime_error, "code point too large");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u8'\360\223\200\200'"), std::runtime_error, "code point too large");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("'\\U11111111"), std::runtime_error, "code point too large");

    ASSERT_EQUALS('\x89', simplecpp::characterLiteralToLL("'\x89'"));
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\x89'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xf4\x90\x80\x80'"), std::runtime_error, "code point too large");

    // following examples based on https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
    ASSERT_EQUALS(0x80, simplecpp::characterLiteralToLL("U'\xc2\x80'"));
    ASSERT_EQUALS(0x800, simplecpp::characterLiteralToLL("U'\xe0\xa0\x80'"));
    ASSERT_EQUALS(0x10000, simplecpp::characterLiteralToLL("U'\xf0\x90\x80\x80'"));

    ASSERT_EQUALS(0x7f, simplecpp::characterLiteralToLL("U'\x7f'"));
    ASSERT_EQUALS(0x7ff, simplecpp::characterLiteralToLL("U'\xdf\xbf'"));
    ASSERT_EQUALS(0xffff, simplecpp::characterLiteralToLL("U'\xef\xbf\xbf'"));

    ASSERT_EQUALS(0xd7ff, simplecpp::characterLiteralToLL("U'\xed\x9f\xbf'"));
    ASSERT_EQUALS(0xe000, simplecpp::characterLiteralToLL("U'\xee\x80\x80'"));
    ASSERT_EQUALS(0xfffd, simplecpp::characterLiteralToLL("U'\xef\xbf\xbd'"));
    ASSERT_EQUALS(0x10ffff, simplecpp::characterLiteralToLL("U'\xf4\x8f\xbf\xbf'"));

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\x80'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\x80\x8f'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\x80\x8f\x8f'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\x80\x8f\x8f\x8f'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xbf'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xbf\x8f'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xbf\x8f\x8f'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xbf\x8f\x8f\x8f'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xc0'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xc0 '"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xe0\x8f'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xe0\x8f '"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xf0\x8f\x8f'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xf0\x8f\x8f '"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xf8'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xff'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xc0\xaf'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xe0\x80\xaf'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xf0\x80\x80\xaf'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xc1\xbf'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xe0\x9f\xbf'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xf0\x8f\xbf\xbf'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xc0\x80'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xe0\x80\x80'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xf0\x80\x80\x80'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xed\xa0\x80'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U'\xed\xbf\xbf'"), std::runtime_error, "assumed UTF-8 encoded source, but sequence is invalid");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL(""), std::runtime_error, "expected a character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("LU"), std::runtime_error, "expected a character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL(";\n"), std::runtime_error, "expected a character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u8U"), std::runtime_error, "expected a character literal");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("'\n\n"), std::runtime_error, "raw single quotes and newlines not allowed in character literals");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("''&"), std::runtime_error, "raw single quotes and newlines not allowed in character literals");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("L'\fff"), std::runtime_error, "multiple characters only supported in narrow character literals");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("'\\\n"), std::runtime_error, "unexpected end of character literal");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("'"), std::runtime_error, "missing closing quote in character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u'"), std::runtime_error, "missing closing quote in character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("L'"), std::runtime_error, "missing closing quote in character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("'a"), std::runtime_error, "missing closing quote in character literal");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("''"), std::runtime_error, "empty character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("L''"), std::runtime_error, "empty character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("U''"), std::runtime_error, "empty character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u''"), std::runtime_error, "empty character literal");
    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u8''"), std::runtime_error, "empty character literal");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("'\\555"), std::runtime_error, "numeric escape sequence too large");

    ASSERT_THROW_EQUALS(simplecpp::characterLiteralToLL("u'Ó"), std::runtime_error, "assumed UTF-8 encoded source, but character literal ends unexpectedly");
}

static void combineOperators_floatliteral()
{
    ASSERT_EQUALS("1.", preprocess("1."));
    ASSERT_EQUALS("1.f", preprocess("1.f"));
    ASSERT_EQUALS(".1", preprocess(".1"));
    ASSERT_EQUALS(".1f", preprocess(".1f"));
    ASSERT_EQUALS("3.1", preprocess("3.1"));
    ASSERT_EQUALS("1E7", preprocess("1E7"));
    ASSERT_EQUALS("1E-7", preprocess("1E-7"));
    ASSERT_EQUALS("1E+7", preprocess("1E+7"));
    ASSERT_EQUALS("1.e+7", preprocess("1.e+7"));
    ASSERT_EQUALS("0x1E + 7", preprocess("0x1E+7"));
    ASSERT_EQUALS("0x1ffp10", preprocess("0x1ffp10"));
    ASSERT_EQUALS("0x0p-1", preprocess("0x0p-1"));
    ASSERT_EQUALS("0x1.p0", preprocess("0x1.p0"));
    ASSERT_EQUALS("0xf.p-1", preprocess("0xf.p-1"));
    ASSERT_EQUALS("0x1.2p3", preprocess("0x1.2p3"));
    ASSERT_EQUALS("0x1.ap3", preprocess("0x1.ap3"));
    ASSERT_EQUALS("0x1.2ap3", preprocess("0x1.2ap3"));
    ASSERT_EQUALS("0x1p+3", preprocess("0x1p+3"));
    ASSERT_EQUALS("0x1p+3f", preprocess("0x1p+3f"));
    ASSERT_EQUALS("0x1p+3L", preprocess("0x1p+3L"));
    ASSERT_EQUALS("1p + 3", preprocess("1p+3"));
    ASSERT_EQUALS("1.0_a . b", preprocess("1.0_a.b"));
    ASSERT_EQUALS("1_a . b", preprocess("1_a.b"));
    ASSERT_EQUALS("bool x = d != 0. and b ;", preprocess("bool x = d != 0. and b;"));
}

static void combineOperators_increment()
{
    ASSERT_EQUALS("; ++ x ;", preprocess(";++x;"));
    ASSERT_EQUALS("; x ++ ;", preprocess(";x++;"));
    ASSERT_EQUALS("1 + + 2", preprocess("1++2"));
}

static void combineOperators_coloncolon()
{
    ASSERT_EQUALS("x ? y : :: z", preprocess("x ? y : ::z"));
}

static void combineOperators_andequal()
{
    ASSERT_EQUALS("x &= 2 ;", preprocess("x &= 2;"));
    ASSERT_EQUALS("void f ( x & = 2 ) ;", preprocess("void f(x &= 2);"));
    ASSERT_EQUALS("f ( x &= 2 ) ;", preprocess("f(x &= 2);"));
    ASSERT_EQUALS("f ( ) { return new int ( i &= 1 ) ; }", preprocess("f () { return new int(i &= 1); }"));
}

static void combineOperators_ellipsis()
{
    ASSERT_EQUALS("void f ( int , ... ) ;", preprocess("void f(int, ...);"));
    ASSERT_EQUALS("void f ( ) { switch ( x ) { case 1 ... 4 : } }", preprocess("void f() { switch(x) { case 1 ... 4: } }"));
}

static void comment()
{
    simplecpp::DUI dui;
    dui.removeComments = true;

    ASSERT_EQUALS("// abc", readfile("// abc"));
    ASSERT_EQUALS("", preprocess("// abc", dui));
    ASSERT_EQUALS("/*\n\n*/abc", readfile("/*\n\n*/abc"));
    ASSERT_EQUALS("\n\nabc", preprocess("/*\n\n*/abc", dui));
    ASSERT_EQUALS("* p = a / * b / * c ;", readfile("*p=a/ *b/ *c;"));
    ASSERT_EQUALS("* p = a / * b / * c ;", preprocess("*p=a/ *b/ *c;", dui));
}

static void comment_multiline()
{
    simplecpp::DUI dui;
    dui.removeComments = true;

    const char code[] = "#define ABC {// \\\n"
                        "}\n"
                        "void f() ABC\n";
    ASSERT_EQUALS("\n\nvoid f ( ) {", preprocess(code, dui));

    const char code1[] = "#define ABC {// \\\r\n"
                         "}\n"
                         "void f() ABC\n";
    ASSERT_EQUALS("\n\nvoid f ( ) {", preprocess(code1, dui));

    const char code2[] = "#define A 1// \\\r"
                         "\r"
                         "2\r"
                         "A\r";
    ASSERT_EQUALS("\n\n2\n1", preprocess(code2, dui));

    const char code3[] = "void f() {// \\ \n}\n";
    ASSERT_EQUALS("void f ( ) {", preprocess(code3, dui));

    const char code4[] = "void f() {// \\\\\\\t\t\n}\n";
    ASSERT_EQUALS("void f ( ) {", preprocess(code4, dui));

    const char code5[] = "void f() {// \\\\\\a\n}\n";
    ASSERT_EQUALS("void f ( ) {\n}", preprocess(code5, dui));

    const char code6[] = "void f() {// \\\n\n\n}\n";
    ASSERT_EQUALS("void f ( ) {\n\n\n}", preprocess(code6, dui));

    // #471 ensure there is newline in comment so that line-splicing can be detected by tools
    ASSERT_EQUALS("// abc\ndef", readfile("// abc\\\ndef"));
}

static void keep_comments()
{
    simplecpp::DUI dui;
    dui.removeComments = false;

    {
        const char code[] = "/* comment */\n";
        ASSERT_EQUALS("/* comment */", preprocess(code,dui));
    }

    {
        const char code[] = "// comment\n";
        ASSERT_EQUALS("// comment", preprocess(code,dui));
    }

    {
        const char code[] = "#define MACRO /* comment */\nMACRO\n";
        ASSERT_EQUALS("\n/* comment */", preprocess(code,dui));
    }
}


static void constFold()
{
    ASSERT_EQUALS("7", testConstFold("1+2*3"));
    ASSERT_EQUALS("15", testConstFold("1+2*(3+4)"));
    ASSERT_EQUALS("123", testConstFold("+123"));
    ASSERT_EQUALS("1", testConstFold("-123<1"));
    ASSERT_EQUALS("6", testConstFold("14 & 7"));
    ASSERT_EQUALS("29", testConstFold("13 ^ 16"));
    ASSERT_EQUALS("25", testConstFold("24 | 1"));
    ASSERT_EQUALS("2", testConstFold("1?2:3"));
    ASSERT_EQUALS("24", testConstFold("010+020"));
    ASSERT_EQUALS("1", testConstFold("010==8"));
    ASSERT_EQUALS("exception", testConstFold("!1 ? 2 :"));
    ASSERT_EQUALS("exception", testConstFold("?2:3"));
    ASSERT_EQUALS("0", testConstFold("0 && 1/0"));
    ASSERT_EQUALS("1", testConstFold("1 || 1/0"));
    ASSERT_EQUALS("2", testConstFold("1 ? 2 : 1/0"));
    ASSERT_EQUALS("exception", testConstFold("1 && 1/0"));
    ASSERT_EQUALS("exception", testConstFold("1/0 && 0"));
    ASSERT_EQUALS("exception", testConstFold("(1/0) ? 1 : 2"));
    ASSERT_EQUALS("exception", testConstFold("-(1/0) + 1"));
    // ?: is right-associative
    ASSERT_EQUALS("1", testConstFold("1 ? 1 : 0 ? 2 : 3"));
    ASSERT_EQUALS("3", testConstFold("0 ? 1 : 0 ? 2 : 3"));
    ASSERT_EQUALS("2", testConstFold("1 ? 2 : 3 ? 4 : 5 ? 6 : 7"));
    ASSERT_EQUALS("7", testConstFold("0 ? 2 : 0 ? 4 : 0 ? 6 : 7"));
    ASSERT_EQUALS("6", testConstFold("1 ? 0 ? 5 : 6 : 7"));
    ASSERT_EQUALS("2", testConstFold("(1 ? 1 : 0) ? 2 : 3"));
    ASSERT_EQUALS("1", testConstFold("1 ? 1 : (1/0) ? 2 : 3"));
    ASSERT_EQUALS("exception", testConstFold("0 ? 1 : (1/0) ? 2 : 3"));
}

#ifdef __CYGWIN__
static void convertCygwinPath()
{
    // absolute paths
    ASSERT_EQUALS("X:\\", simplecpp::convertCygwinToWindowsPath("/cygdrive/x"));     // initial backslash
    ASSERT_EQUALS("X:\\", simplecpp::convertCygwinToWindowsPath("/cygdrive/x/"));
    ASSERT_EQUALS("X:\\dir", simplecpp::convertCygwinToWindowsPath("/cygdrive/x/dir"));
    ASSERT_EQUALS("X:\\dir\\file", simplecpp::convertCygwinToWindowsPath("/cygdrive/x/dir/file"));

    // relative paths
    ASSERT_EQUALS("file", simplecpp::convertCygwinToWindowsPath("file"));
    ASSERT_EQUALS("dir\\file", simplecpp::convertCygwinToWindowsPath("dir/file"));
    ASSERT_EQUALS("..\\dir\\file", simplecpp::convertCygwinToWindowsPath("../dir/file"));

    // incorrect Cygwin paths
    ASSERT_EQUALS("\\cygdrive", simplecpp::convertCygwinToWindowsPath("/cygdrive"));
    ASSERT_EQUALS("\\cygdrive\\", simplecpp::convertCygwinToWindowsPath("/cygdrive/"));
}
#endif

static void define1()
{
    const char code[] = "#define A 1+2\n"
                        "a=A+3;";
    ASSERT_EQUALS("# define A 1 + 2\n"
                  "a = A + 3 ;",
                  readfile(code));
    ASSERT_EQUALS("\na = 1 + 2 + 3 ;",
                  preprocess(code));
}

static void define2()
{
    const char code[] = "#define ADD(A,B) A+B\n"
                        "ADD(1+2,3);";
    ASSERT_EQUALS("# define ADD ( A , B ) A + B\n"
                  "ADD ( 1 + 2 , 3 ) ;",
                  readfile(code));
    ASSERT_EQUALS("\n1 + 2 + 3 ;",
                  preprocess(code));
}

static void define3()
{
    const char code[] = "#define A   123\n"
                        "#define B   A\n"
                        "A B";
    ASSERT_EQUALS("# define A 123\n"
                  "# define B A\n"
                  "A B",
                  readfile(code));
    ASSERT_EQUALS("\n\n123 123",
                  preprocess(code));
}

static void define4()
{
    const char code[] = "#define A      123\n"
                        "#define B(C)   A\n"
                        "A B(1)";
    ASSERT_EQUALS("# define A 123\n"
                  "# define B ( C ) A\n"
                  "A B ( 1 )",
                  readfile(code));
    ASSERT_EQUALS("\n\n123 123",
                  preprocess(code));
}

static void define5()
{
    const char code[] = "#define add(x,y) x+y\n"
                        "add(add(1,2),3)";
    ASSERT_EQUALS("\n1 + 2 + 3", preprocess(code));
}

static void define6()
{
    const char code[] = "#define A() 1\n"
                        "A()";
    ASSERT_EQUALS("\n1", preprocess(code));
}

static void define7()
{
    simplecpp::DUI dui;
    dui.removeComments = true;

    const char code[] = "#define A(X) X+1\n"
                        "A(1 /*23*/)";
    ASSERT_EQUALS("\n1 + 1", preprocess(code, dui));
}

static void define8()   // 6.10.3.10
{
    const char code[] = "#define A(X) \n"
                        "int A[10];";
    ASSERT_EQUALS("\nint A [ 10 ] ;", preprocess(code));
}

static void define9()
{
    const char code[] = "#define AB ab.AB\n"
                        "AB.CD\n";
    ASSERT_EQUALS("\nab . AB . CD", preprocess(code));
}

static void define10()   // don't combine prefix with space in macro
{
    const char code[] = "#define A u8 \"a b\"\n"
                        "A;";
    ASSERT_EQUALS("\nu8 \"a b\" ;", preprocess(code));
}

static void define11() // location of expanded argument
{
    const char code[] = "#line 4 \"version.h\"\n"
                        "#define A(x) B(x)\n"
                        "#define B(x) x\n"
                        "#define VER A(1)\n"
                        "\n"
                        "#line 10 \"cppcheck.cpp\"\n"
                        "VER;";
    ASSERT_EQUALS("\n#line 10 \"cppcheck.cpp\"\n1 ;", preprocess(code));
}

static void define12()
{
    const char code[] = "struct foo x = {\n"
                        "  #define V 0\n"
                        "  .x = V,\n"
                        "};\n";
    ASSERT_EQUALS("struct foo x = {\n"
                  "# define V 0\n"
                  ". x = V ,\n"
                  "} ;", readfile(code));
    ASSERT_EQUALS("struct foo x = {\n"
                  "\n"
                  ". x = 0 ,\n"
                  "} ;", preprocess(code));
}

static void define13()
{
    const char code[] = "#define M 180.\n"
                        "extern void g();\n"
                        "void f(double d) {\n"
                        "    if (d > M) {}\n"
                        "}\n";
    ASSERT_EQUALS("\nextern void g ( ) ;\n"
                  "void f ( double d ) {\n"
                  "if ( d > 180. ) { }\n"
                  "}", preprocess(code));
}

static void define14() // #296
{
    const char code[] = "#define bar(x) x % 2\n"
                        "#define foo(x) printf(#x \"\\n\")\n"
                        "\n"
                        " foo(bar(3));\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "printf ( \"bar(3)\" \"\\n\" ) ;", preprocess(code));
}

static void define15() // #231
{
    const char code[] = "#define CAT(a, b) CAT2(a, b)\n"
                        "#define CAT2(a, b) a ## b\n"
                        "#define FOO x\n"
                        "#define BAR() CAT(F, OO)\n"
                        "#define BAZ CAT(B, AR)()\n"
                        "BAZ\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "x", preprocess(code));
}

static void define16() // #201
{
    const char code[] = "#define ALL_COLORS(warm_colors) \\\n"
                        "  X(Blue) \\\n"
                        "  X(Green) \\\n"
                        "  X(Purple) \\\n"
                        "  warm_colors\n"
                        "\n"
                        "#define WARM_COLORS \\\n"
                        "  X(Red) \\\n"
                        "  X(Yellow) \\\n"
                        "  X(Orange)\n"
                        "\n"
                        "#define COLOR_SET ALL_COLORS(WARM_COLORS)\n"
                        "\n"
                        "#define X(color) #color,\n"
                        "\n"
                        "COLOR_SET\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\"Blue\" , \"Green\" , \"Purple\" , \"Red\" , \"Yellow\" , \"Orange\" ,", preprocess(code));
}

static void define17() // #185
{
    const char code[] = "#define at(x, y) x##y\n"
                        "#define b(...) \\\n"
                        "aa(__VA_ARGS__, , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , \\\n"
                        ", , , , , , , , 2)\n"
                        "#define aa(c, d, a, b, e, f, g, h, ab, ac, i, ad, j, k, l, m, n, o, p, ae, q, \\\n"
                        "r, s, t, u, v, w, x, y, z, af, ag, ah, ai, aj, ak, al, am, an, ao, \\\n"
                        "ap) \\\n"
                        "ap\n"
                        "#define aq(...) ar(b(__VA_ARGS__), __VA_ARGS__) static_assert(true, \" \")\n"
                        "#define ar(ap, ...) at(I_, ap)(__VA_ARGS__)\n"
                        "#define I_2(as, a)\n"
                        "aq(a, array);\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "static_assert ( true , \" \" ) ;", preprocess(code));
}

static void define18() // #130
{
    const char code[] = "#define MAC2STR(x) x[0],x[1],x[2],x[3],x[4],x[5]\n"
                        "#define FT_DEBUG(fmt, args...) if(pGlobalCtx && pGlobalCtx->debug_level>=2) printf(\"FT-dbg: \"fmt, ##args)\n"
                        "\n"
                        "FT_DEBUG(\"  %02x:%02x:%02x:%02x:%02x:%02x\\n\", MAC2STR(pCtx->wlan_intf_addr[i]));\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "if ( pGlobalCtx && pGlobalCtx -> debug_level >= 2 ) printf ( \"FT-dbg: \" \"  %02x:%02x:%02x:%02x:%02x:%02x\\n\" , pCtx -> wlan_intf_addr [ i ] [ 0 ] , pCtx -> wlan_intf_addr [ i ] [ 1 ] , pCtx -> wlan_intf_addr [ i ] [ 2 ] , pCtx -> wlan_intf_addr [ i ] [ 3 ] , pCtx -> wlan_intf_addr [ i ] [ 4 ] , pCtx -> wlan_intf_addr [ i ] [ 5 ] ) ;", preprocess(code));
}

static void define19() // #124
{
    const char code[] = "#define CONCAT(tok) tok##suffix\n"
                        "\n"
                        "CONCAT(Test);\n"
                        "CONCAT(const Test);\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "Testsuffix ;\n"
                  "const Testsuffix ;", preprocess(code));
}

static void define20() // #113
{
    const char code[] = "#define TARGS4 T1,T2,T3,T4\n"
                        "#define FOOIMPL(T__CLASS, TARGS) void foo(const T__CLASS<TARGS>& x) { }\n"
                        "#define FOOIMPL_4(T__CLASS)      FOOIMPL(T__CLASS, TARGS4)\n"
                        "FOOIMPL_4(y)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "void foo ( const y < T1 , T2 , T3 , T4 > & x ) { }", preprocess(code));
}

static void define21() // #66
{
    const char code[] = "#define GETMYID(a) ((a))+1\n"
                        "#define FIGHT_FOO(c, ...) foo(c, ##__VA_ARGS__)\n"
                        "FIGHT_FOO(1, GETMYID(a));\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "foo ( 1 , ( ( a ) ) + 1 ) ;", preprocess(code));
}

static void define22() // #40
{
    const char code[] = "#define COUNTER_NAME(NAME, ...) NAME##Count\n"
                        "#define COMMA ,\n"
                        "\n"
                        "#define DECLARE_COUNTERS(LIST) unsigned long LIST(COUNTER_NAME, COMMA);\n"
                        "\n"
                        "#define ACTUAL_LIST(FUNCTION, SEPARATOR) \\\n"
                        "FUNCTION(event1, int, foo) SEPARATOR \\\n"
                        "FUNCTION(event2, char, bar)\n"
                        "\n"
                        "DECLARE_COUNTERS(ACTUAL_LIST)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "unsigned long event1Count , event2Count ;", preprocess(code));
}

static void define23() // #40
{
    const char code[] = "#define COMMA ,\n"
                        "#define MULTI(SEPARATOR) A SEPARATOR B\n"
                        "\n"
                        "#define VARS MULTI(COMMA)\n"
                        "unsigned VARS;\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "\n"
                  "unsigned A , B ;", preprocess(code));
}

static void define24()
{
    // an expansion result that is a function-like macro name must be rescanned
    // repeatedly against the tokens that follow it
    const char code[] = "#define a(b, c) c\n"
                        "#define d() a\n"
                        "#define g(e) h(e, ) h(e, )\n"
                        "#define h(e, b) d()(, e)()\n"
                        "#define i()\n"
                        "g(i)\n";
    ASSERT_EQUALS("", preprocess(code));
}

static void define25()
{
    // a macro name that came from expanding that same macro must not be
    // re-expanded when rescanned with the tokens that follow it
    const char code[] = "#define f() f\n"
                        "#define wrap(x) x()\n"
                        "wrap(f())\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "f ( )", preprocess(code));
}

static void define26()
{
    // a macro name that came from expanding that same macro must not be
    // re-expanded with arguments taken from the raw token stream
    const char code[] = "#define f() f\n"
                        "f()()\n";
    ASSERT_EQUALS("\n"
                  "f ( )", preprocess(code));

    const char code2[] = "#define f() f\n"
                         "f()()()\n";
    ASSERT_EQUALS("\n"
                  "f ( ) ( )", preprocess(code2));
}

static void define27()
{
    // an expansion result inside a macro argument that is a function-like macro
    // name must be rescanned against the remaining argument tokens
    const char code1[] = "#define f(x) [x]\n"
                         "#define ID(x) x\n"
                         "#define G() f\n"
                         "ID(G()(1))\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "[ 1 ]", preprocess(code1));

    // the rescanned call's arguments are themselves rescanned
    const char code2[] = "#define f(x) [x]\n"
                         "#define ID(x) x\n"
                         "#define G() f\n"
                         "ID(G()(G()(2)))\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "[ [ 2 ] ]", preprocess(code2));

    // each rescan round must use the right set of already-expanded macros:
    // A(1)->B, B(2)->A, A(3)->B, B(4)->A
    const char code3[] = "#define ID(x) x\n"
                         "#define A(x) B\n"
                         "#define B(x) A\n"
                         "ID(A(1)(2)(3)(4))\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "A", preprocess(code3));

    // creduce'd fuzzer case: the missed rescan of o1(k) deferred the call to a
    // context where j was wrongly blocked from expanding, so b pasted ')' and
    // '1' into an invalid token that broke the parentheses of the m call
    const char code4[] = "#define o1(a) a()\n"
                         "#define b(c, d) c##d\n"
                         "#define l(c, d) b(c, 1)\n"
                         "#define m(e, f)\n"
                         "#define g(h, i) m(h, )\n"
                         "#define j(h, i) g(h, )\n"
                         "#define k() l(j(, ), )\n"
                         "#define n(a) j(l(o, )(a), )\n"
                         "n(k)\n";
    ASSERT_EQUALS("", preprocess(code4));
}


static void define_invalid_1()
{
    const char code[] = "#define  A(\nB\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, bad macro syntax\n", toString(outputList));
}

static void define_invalid_2()
{
    const char code[] = "#define\nhas#\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, bad macro syntax\n", toString(outputList));
}

static void define_invalid_3()
{
    const char code[] = "#define R()\n"
                        "R\n";
    simplecpp::OutputList outputList;
    // a function-like macro name at the end of the file is not a macro call, gcc outputs R.
    // TODO: the old macro expansion reports an error
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\nR", "", preprocess(code, &outputList));
    TODO_OLD_EXPANSION_ASSERT_EQUALS("", "file0,2,syntax_error,failed to expand 'R', Wrong number of parameters for macro 'R'.\n", toString(outputList));
}

static void define_wrong_number_of_arguments()
{
    {
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("", preprocess("#define f(a,b) a+b\nf(1)\n", &outputList));
        ASSERT_EQUALS("file0,2,syntax_error,failed to expand 'f', Wrong number of parameters for macro 'f'.\n", toString(outputList));
    }
    {
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("", preprocess("#define f(a,b) a+b\nf(1,2,3)\n", &outputList));
        ASSERT_EQUALS("file0,2,syntax_error,failed to expand 'f', Wrong number of parameters for macro 'f'.\n", toString(outputList));
    }
    {
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("", preprocess("#define f(a,b,...) a+b\nf(1)\n", &outputList));
        ASSERT_EQUALS("file0,2,syntax_error,failed to expand 'f', Wrong number of parameters for macro 'f'.\n", toString(outputList));
    }
    {
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("\n1", preprocess("#define f(a,...) a __VA_ARGS__\nf(1)\n", &outputList));
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        // a function-like macro name that is not followed by ( is not expanded
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("\nf ;", preprocess("#define f(a) a\nf;\n", &outputList));
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        // TODO: f takes no arguments, gcc reports an error
        simplecpp::OutputList outputList;
        TODO_ASSERT_EQUALS("", "\n1", preprocess("#define f() 1\nf(1)\n", &outputList));
        TODO_ASSERT_EQUALS("file0,2,syntax_error,failed to expand 'f', Wrong number of parameters for macro 'f'.\n", "", toString(outputList));
    }
}

static void define_invalid_4()
{
    const char code[] = "#define X(...)\n"
                        "X\n";
    simplecpp::OutputList outputList;
    // a function-like macro name at the end of the file is not a macro call, gcc outputs X.
    // TODO: the old macro expansion reports an error
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\nX", "", preprocess(code, &outputList));
    TODO_OLD_EXPANSION_ASSERT_EQUALS("", "file0,2,syntax_error,failed to expand 'X', Wrong number of parameters for macro 'X'.\n", toString(outputList));
}

static void define_define_1()
{
    const char code[] = "#define A(x) (x+1)\n"
                        "#define B    A(\n"
                        "B(i))";
    ASSERT_EQUALS("\n\n( ( i ) + 1 )", preprocess(code));
}

static void define_define_2()
{
    const char code[] = "#define A(m)    n=m\n"
                        "#define B(x)    A(x)\n"
                        "B(0)";
    ASSERT_EQUALS("\n\nn = 0", preprocess(code));
}

static void define_define_3()
{
    const char code[] = "#define ABC 123\n"
                        "#define A(B) A##B\n"
                        "A(BC)";
    ASSERT_EQUALS("\n\n123", preprocess(code));
}

static void define_define_4()
{
    const char code[] = "#define FOO1()\n"
                        "#define TEST(FOO) FOO FOO()\n"
                        "TEST(FOO1)";
    ASSERT_EQUALS("\n\nFOO1", preprocess(code));
}

static void define_define_5()
{
    const char code[] = "#define X() Y\n"
                        "#define Y() X\n"
                        "A: X()()()\n";
    // mcpp outputs "A: X()" and gcc/clang/vc outputs "A: Y"
    ASSERT_EQUALS("\n\nA : Y", preprocess(code)); // <- match the output from gcc/clang/vc
}

static void define_define_6()
{
    const char code1[] = "#define f(a) a*g\n"
                         "#define g f\n"
                         "a: f(2)(9)\n";
    ASSERT_EQUALS("\n\na : 2 * f ( 9 )", preprocess(code1));

    const char code2[] = "#define f(a) a*g\n"
                         "#define g(a) f(a)\n"
                         "a: f(2)(9)\n";
    ASSERT_EQUALS("\n\na : 2 * 9 * g", preprocess(code2));
}

static void define_define_7()
{
    const char code[] = "#define f(x) g(x\n"
                        "#define g(x) x()\n"
                        "f(f))\n";
    ASSERT_EQUALS("\n\nf ( )", preprocess(code));
}

static void define_define_8()   // line break in nested macro call
{
    const char code[] = "#define A(X,Y)  ((X)*(Y))\n"
                        "#define B(X,Y)  ((X)+(Y))\n"
                        "B(0,A(255,x+\n"
                        "y))\n";
    ASSERT_EQUALS("\n\n( ( 0 ) + ( ( ( 255 ) * ( x + y ) ) ) )", preprocess(code));
}

static void define_define_9()   // line break in nested macro call
{
    const char code[] = "#define A(X) X\n"
                        "#define B(X) X\n"
                        "A(\nB(dostuff(1,\n2)))\n";
    ASSERT_EQUALS("\n\ndostuff ( 1 , 2 )", preprocess(code));
}

static void define_define_10()
{
    const char code[] = "#define glue(a, b) a ## b\n"
                        "#define xglue(a, b) glue(a, b)\n"
                        "#define AB 1\n"
                        "#define B B 2\n"
                        "xglue(A, B)\n";
    ASSERT_EQUALS("\n\n\n\n1 2", preprocess(code));
}

static void define_define_11()
{
    const char code[] = "#define XY(x, y)   x ## y\n"
                        "#define XY2(x, y)  XY(x, y)\n"
                        "#define PORT       XY2(P, 2)\n"
                        "#define ABC        XY2(PORT, DIR)\n"
                        "ABC;\n";
    ASSERT_EQUALS("\n\n\n\nP2DIR ;", preprocess(code));
}

static void define_define_11a()
{
    const char code[] = "#define A_B_C              0x1\n"
                        "#define A_ADDRESS          0x00001000U\n"
                        "#define A                  ((uint32_t ) A_ADDRESS)\n"
                        "#define CONCAT(x, y, z)    x ## _ ## y ## _ ## z\n"
                        "#define TEST_MACRO         CONCAT(A, B, C)\n"
                        "TEST_MACRO\n";
    ASSERT_EQUALS("\n\n\n\n\n0x1", preprocess(code));

    const char code2[] = "#define ADDER_S(a, b) a + b\n" // #374
                         "#define ADDER(x) ADDER_S(x)\n"
                         "#define ARGUMENTS 1, 2\n"
                         "#define RUN ADDER(ARGUMENTS)\n"
                         "void f() { RUN; }\n";
    ASSERT_EQUALS("\n\n\n\nvoid f ( ) { 1 + 2 ; }", preprocess(code2));
}

static void define_define_12()
{
    const char code[] = "#define XY(Z)  Z\n"
                        "#define X(ID)  X##ID(0)\n"
                        "X(Y)\n";
    ASSERT_EQUALS("\n\n0", preprocess(code));
}

static void define_define_13() // issue #49 - empty macro
{
    const char code[] = "#define f()\n"
                        "#define t(a) a\n"
                        "(t(f))\n";
    ASSERT_EQUALS("\n\n( f )", preprocess(code));
}

static void define_define_14() // issue #58 - endless recursion
{
    const char code[] = "#define z f(w\n"
                        "#define f()\n"
                        "#define w f(z\n"
                        "w\n";
    ASSERT_EQUALS("\n\n\nf ( f ( w", preprocess(code)); // Don't crash
}

static void define_define_15() // issue #72 without __VA_ARGS__
{
    const char code[] = "#define a          f\n"
                        "#define foo(x,y)   a(x,y)\n"
                        "#define f(x, y)    x y\n"
                        "foo(1,2)";
    ASSERT_EQUALS("\n\n\n1 2", preprocess(code));
}

static void define_define_16() // issue #72 with __VA_ARGS__
{
    const char code[] = "#define ab(a, b)  a##b\n"
                        "#define foo(...) ab(f, 2) (__VA_ARGS__)\n"
                        "#define f2(x, y) x y\n"
                        "foo(1,2)";
    ASSERT_EQUALS("\n\n\n1 2", preprocess(code));
}

static void define_define_17()
{
    const char code[] = "#define Bar(x) x\n"
                        "#define Foo Bar(1)\n"
                        "Bar( Foo ) ;";
    ASSERT_EQUALS("\n\n1 ;", preprocess(code));
}

static void define_define_18()
{
    const char code[] = "#define FOO(v)      BAR(v, 0)\n"
                        "#define BAR(v, x)   (v)\n"
                        "#define var         (p->var)\n"
                        "FOO(var);";
    ASSERT_EQUALS("\n\n\n( ( p -> var ) ) ;", preprocess(code));
}

static void define_define_19() // #292
{
    const char code[] = "#define X 1,2,3\n"
                        "#define Foo(A, B) A\n"
                        "#define Bar Foo(X, 0)\n"
                        "Bar\n";
    ASSERT_EQUALS("\n\n\n1 , 2 , 3", preprocess(code));
}

static void define_define_20() // #384 arg contains comma
{
    const char code[] = "#define Z_IS_ENABLED1(config_macro) Z_IS_ENABLED2(_XXXX##config_macro)\n"
                        "#define _XXXX1 _YYYY,\n"
                        "#define Z_IS_ENABLED2(one_or_two_args) Z_IS_ENABLED3(one_or_two_args 1, 0)\n"
                        "#define Z_IS_ENABLED3(ignore_this, val, ...) val\n"
                        "#define IS_ENABLED(config_macro) Z_IS_ENABLED1(config_macro)\n"
                        "#define FEATURE 1\n"
                        "a = IS_ENABLED(FEATURE)\n";
    ASSERT_EQUALS("\n\n\n\n\n\na = 1", preprocess(code));
}

static void define_define_21() // #397 DEBRACKET macro
{
    const char code1[] = "#define A(val) B val\n"
                         "#define B(val) val\n"
                         "A((2))\n";
    ASSERT_EQUALS("\n\n2", preprocess(code1));

    const char code2[] = "#define x (2)\n"
                         "#define A B x\n"
                         "#define B(val) val\n"
                         "A\n";
    ASSERT_EQUALS("\n\n\nB ( 2 )", preprocess(code2));

    const char code3[] = "#define __GET_ARG2_DEBRACKET(ignore_this, val, ...) __DEBRACKET val\n"
                         "#define __DEBRACKET(...) __VA_ARGS__\n"
                         "#5 \"a.c\"\n"
                         "__GET_ARG2_DEBRACKET(432 (33), (B))\n";
    ASSERT_EQUALS("\n#line 5 \"a.c\"\nB", preprocess(code3));
}

static void define_define_22() // #400 inner macro not expanded after hash hash
{
    const char code[] = "#define FOO(a) CAT(DO, STUFF)(1,2)\n"
                        "#define DOSTUFF(a, b)  CAT(3, 4)\n"
                        "#define CAT(a, b) a##b\n"
                        "FOO(1)\n";
    ASSERT_EQUALS("\n\n\n34", preprocess(code));
}

static void define_define_23() // #403 crash (infinite recursion)
{
    const char code[] = "#define C_(x, y)       x ## y\n"
                        "#define C(x, y)        C_(x, y)\n"
                        "#define X(func)        C(Y, C(func, Z))\n"
                        "#define die X(die)\n"
                        "die(void);\n";
    ASSERT_EQUALS("\n\n\n\nYdieZ ( void ) ;", preprocess(code));
}

static void define_define_24() // #590
{
    const char code[] = "#define B A\n"
                        "#define A x(B)\n"
                        "#define C(s) s\n"
                        "#define D(s) C(s)\n"
                        "D(A)\n";
    ASSERT_EQUALS("\n\n\n\nx ( A )", preprocess(code));
}

static void define_va_args_1()
{
    const char code[] = "#define A(fmt...) dostuff(fmt)\n"
                        "A(1,2);";
    ASSERT_EQUALS("\ndostuff ( 1 , 2 ) ;", preprocess(code));
}

static void define_va_args_2()
{
    const char code[] = "#define A(X,...) X(#__VA_ARGS__)\n"
                        "A(f,123);";
    ASSERT_EQUALS("\nf ( \"123\" ) ;", preprocess(code));
}

static void define_va_args_3()   // min number of arguments
{
    const char code[] = "#define A(x, y, z...) 1\n"
                        "A(1, 2)\n";
    ASSERT_EQUALS("\n1", preprocess(code));
}

static void define_va_args_4() // cppcheck trac #9754
{
    const char code[] = "#define A(x, y, ...) printf(x, y, __VA_ARGS__)\n"
                        "A(1, 2)\n";
    ASSERT_EQUALS("\nprintf ( 1 , 2 )", preprocess(code));
}

static void define_va_opt_1()
{
    const char code[] = "#define p1(fmt, args...) printf(fmt __VA_OPT__(,) args)\n"
                        "p1(\"hello\");\n"
                        "p1(\"%s\", \"hello\");\n";

    ASSERT_EQUALS("\nprintf ( \"hello\" ) ;\n"
                  "printf ( \"%s\" , \"hello\" ) ;",
                  preprocess(code));
}

static void define_va_opt_2()
{
    const char code[] = "#define err(...)\\\n"
                        "__VA_OPT__(\\\n"
                        "printf(__VA_ARGS__);\\\n"
                        ")\n"
                        "#define err2(something, ...) __VA_OPT__(err(__VA_ARGS__))\n"
                        "err2(test)\n"
                        "err2(test, \"%d\", 2)\n";

    ASSERT_EQUALS("\n\n\n\n\n\nprintf ( \"%d\" , 2 ) ;", preprocess(code));
}

static void define_va_opt_3()
{
    // non-escaped newline without closing parenthesis
    const char code1[] = "#define err(...) __VA_OPT__(printf( __VA_ARGS__);\n"
                         ")\n"
                         "err()\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code1, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': Missing closing parenthesis for __VA_OPT__\n",
                  toString(outputList));

    outputList.clear();

    // non-escaped newline without open parenthesis
    const char code2[] = "#define err(...) __VA_OPT__\n"
                         "(something)\n"
                         "err()\n";

    ASSERT_EQUALS("", preprocess(code2, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': Missing opening parenthesis for __VA_OPT__\n",
                  toString(outputList));
}

static void define_va_opt_4()
{
    // missing parenthesis
    const char code1[] = "#define err(...) __VA_OPT__ something\n"
                         "err()\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code1, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': Missing opening parenthesis for __VA_OPT__\n",
                  toString(outputList));

    outputList.clear();

    // missing open parenthesis
    const char code2[] = "#define err(...) __VA_OPT__ something)\n"
                         "err()\n";

    ASSERT_EQUALS("", preprocess(code2, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': Missing opening parenthesis for __VA_OPT__\n",
                  toString(outputList));
}

static void define_va_opt_5()
{
    // parenthesis not directly proceeding __VA_OPT__
    const char code[] = "#define err(...) __VA_OPT__ something (something)\n"
                        "err()\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': Missing opening parenthesis for __VA_OPT__\n",
                  toString(outputList));
}

static void define_va_opt_6()
{
    // nested __VA_OPT__
    const char code[] = "#define err(...) __VA_OPT__(__VA_OPT__(something))\n"
                        "err()\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': __VA_OPT__ cannot be nested\n",
                  toString(outputList));
}

static void define_va_opt_7()
{
    // eof in __VA_OPT__
    const char code1[] = "#define err(...) __VA_OPT__\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code1, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': Missing opening parenthesis for __VA_OPT__\n",
                  toString(outputList));

    outputList.clear();

    const char code2[] = "#define err(...) __VA_OPT__(\n";

    ASSERT_EQUALS("", preprocess(code2, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': Missing closing parenthesis for __VA_OPT__\n",
                  toString(outputList));

    outputList.clear();

    const char code3[] = "#define err(...) __VA_OPT__(x\n";

    ASSERT_EQUALS("", preprocess(code3, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Failed to parse #define, In definition of 'err': Missing closing parenthesis for __VA_OPT__\n",
                  toString(outputList));
}

static void define_va_opt_8()
{
    const char code[] = "#define f(...) #__VA_OPT__(x)\n"
                        "const char* v1 = f();\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("\nconst char * v1 = \"\" ;", preprocess(code, &outputList));
    ASSERT_EQUALS("", toString(outputList));
}

static void define_va_opt_9()
{
    simplecpp::DUI dui;
    dui.defines.emplace_back("f(...)=__VA_OPT__");
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess("", dui, &outputList));
    ASSERT_EQUALS("file0,0,dui_error,In definition of 'f': Missing opening parenthesis for __VA_OPT__\n", toString(outputList));
}

static void define_ifdef()
{
    const char code[] = "#define A(X) X\n"
                        "A(1\n"
                        "#ifdef CFG\n"
                        "#endif\n"
                        ")\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,3,directive_as_macro_parameter,failed to expand 'A', it is invalid to use a preprocessor directive as macro parameter\n", toString(outputList));

}

static void if_invalid_1()
{
    const char code[] = "#if'\\u'\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, expected digit\n", toString(outputList));
}

static void if_invalid_2()
{
    const char code[] = "#if-0xBBB4444444444444%~B\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, division overflow\n", toString(outputList));
}

static void if_invalid_3()
{
    const char code[] = "#if@u'\\udefa'\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, surrogate code points not allowed in universal character names\n", toString(outputList));
}

static void pragma_backslash()
{
    const char code[] = "#pragma comment (longstring, \\\n"
                        "\"HEADER\\\n"
                        "This is a very long string that is\\\n"
                        "a multi-line string.\\\n"
                        "How much more do I have to say?\\\n"
                        "Well, be prepared, because the\\\n"
                        "story is just beginning. This is a test\\\n"
                        "string for demonstration purposes. \")\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
}

static void pragma_backslash_2() // #217
{
    const char code[] = "#pragma comment(linker, \"foo \\\n"
                        "bar\")\n";

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
}

static void dollar()
{
    ASSERT_EQUALS("$ab", readfile("$ab"));
    ASSERT_EQUALS("a$b", readfile("a$b"));
}

static void error1()
{
    const char code[] = "#error    hello world!\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,#error,#error hello world!\n", toString(outputList));
}

static void error2()
{
    const char code[] = "#error   it's an error\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,#error,#error it's an error\n", toString(outputList));
}

static void error3()
{
    const char code[] = "#error \"bla bla\\\n"
                        " bla bla.\"\n";
    std::vector<std::string> files;
    simplecpp::OutputList outputList;
    const simplecpp::TokenList rawtokens = makeTokenList(code, files, "test.c", {}, &outputList);
    ASSERT_EQUALS("", toString(outputList));
}

static void error4()
{
    // "#error x\n1"
    const char code[] = "\xFE\xFF\x00\x23\x00\x65\x00\x72\x00\x72\x00\x6f\x00\x72\x00\x20\x00\x78\x00\x0a\x00\x31";
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    const simplecpp::TokenList rawtoken = makeTokenList(code, sizeof(code)-1,files,"test.c");
    runPreprocess(tokens2, rawtoken, files, cache, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,#error,#error x\n", toString(outputList));
}

static void error5()
{
    // "#error x\n1"
    const char code[] = "\xFF\xFE\x23\x00\x65\x00\x72\x00\x72\x00\x6f\x00\x72\x00\x20\x00\x78\x00\x0a\x00\x78\x00\x31\x00";
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    const simplecpp::TokenList rawtokens = makeTokenList(code, sizeof(code)-1,files,"test.c");
    runPreprocess(tokens2, rawtokens, files, cache, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,#error,#error x\n", toString(outputList));
}

static void error6()
{
    // "#error\<LF>"
    const char code[] = "\xFF\xFE\x23\x00\x65\x00\x72\x00\x72\x00\x6f\x00\x72\x00\x20\x00\x5c\x00\x0a\x00";
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    const simplecpp::TokenList rawtokens = makeTokenList(code, sizeof(code)-1,files,"test.c");
    runPreprocess(tokens2, rawtokens, files, cache, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,#error,#error \n", toString(outputList));
}

static void error7()
{
    const char code[] = "#error bla\\\nbla\n";
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    const simplecpp::TokenList rawtokens = makeTokenList(code, sizeof(code)-1,files,"test.c");
    runPreprocess(tokens2, rawtokens, files, cache, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,#error,#error blabla\n", toString(outputList));
}

static void error8()
{
    const char code[] = "#error bla\\\r\nbla\n";
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    const simplecpp::TokenList rawtokens = makeTokenList(code, sizeof(code)-1,files,"test.c");
    runPreprocess(tokens2, rawtokens, files, cache, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,#error,#error blabla\n", toString(outputList));
}

static void garbage()
{
    simplecpp::OutputList outputList;

    outputList.clear();
    ASSERT_EQUALS("", preprocess("#ifdef\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,Syntax error in #ifdef\n", toString(outputList));

    outputList.clear();
    ASSERT_EQUALS("", preprocess("#define TEST2() A ##\nTEST2()\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'TEST2', Invalid ## usage when expanding 'TEST2': Unexpected newline\n", toString(outputList));

    outputList.clear();
    ASSERT_EQUALS("", preprocess("#define CON(a,b)  a##b##\nCON(1,2)\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'CON', Invalid ## usage when expanding 'CON': Unexpected newline\n", toString(outputList));
}

static void garbage_endif()
{
    simplecpp::OutputList outputList;

    outputList.clear();
    ASSERT_EQUALS("", preprocess("#elif A<0\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,#elif without #if\n", toString(outputList));

    outputList.clear();
    ASSERT_EQUALS("", preprocess("#else\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,#else without #if\n", toString(outputList));

    outputList.clear();
    ASSERT_EQUALS("", preprocess("#endif\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,#endif without #if\n", toString(outputList));
}

static void hash()
{
    ASSERT_EQUALS("x = \"1\"", preprocess("x=#__LINE__"));

    const char code[] = "#define a(x) #x\n"
                        "a(1)\n"
                        "a(2+3)";
    ASSERT_EQUALS("\n"
                  "\"1\"\n"
                  "\"2+3\"", preprocess(code));

    ASSERT_EQUALS("\n\"\\\"abc\\\\0\\\"\"", preprocess("#define str(x) #x\nstr(\"abc\\0\")\n"));

    ASSERT_EQUALS("\n\n( \"123\" )",
                  preprocess("#define A(x)  (x)\n"
                             "#define B(x)  A(#x)\n"
                             "B(123)"));

    ASSERT_EQUALS("\n\nprintf ( \"bar(3)\" \"\\n\" ) ;",
                  preprocess("#define bar(x) x % 2\n"
                             "#define foo(x) printf(#x \"\\n\")\n"
                             "foo(bar(3));"));

    ASSERT_EQUALS("\n\n\n\"Y Y\"",
                  preprocess("#define X(x,y)    x y\n"
                             "#define STR_(x)   #x\n"
                             "#define STR(x)    STR_(x)\n"
                             "STR(X(Y,Y))"));
}

static void hashEscape()
{
    // a \ is only inserted before " and \ inside string literals and character constants
    const char code[] = "#define str(x) #x\n"
                        "str(\\n)\n"
                        "str(: @\\n)\n"
                        "str(\"\\n\")\n"
                        "str('\\4')\n"
                        "str('\"')\n"
                        "str(L\"a\\\"b\")";
    ASSERT_EQUALS("\n"
                  "\"\\n\"\n"
                  "\": @\\n\"\n"
                  "\"\\\"\\\\n\\\"\"\n"
                  "\"'\\\\4'\"\n"
                  "\"'\\\"'\"\n"
                  "\"L\\\"a\\\\\\\"b\\\"\"", preprocess(code));
}

static void hashComment()
{
    // a comment is replaced by one space character
    simplecpp::DUI dui;
    dui.removeComments = true;
    const char code[] = "#define str(x) #x\n"
                        "#define xstr(x) str(x)\n"
                        "str(a/**/b)\n"
                        "str(a/**/ /**/b)\n"
                        "str(a/**/)\n"
                        "xstr(a/**/b)";
    ASSERT_EQUALS("\n\n"
                  "\"a b\"\n"
                  "\"a b\"\n"
                  "\"a\"\n"
                  "\"a b\"", preprocess(code, dui));

    // comments are kept in the string when they are not removed, like gcc -C
    ASSERT_EQUALS("\n\n"
                  "\"a/**/b\"\n"
                  "\"a/**/ /**/b\"\n"
                  "\"a/**/\"\n"
                  "\"a/**/b\"", preprocess(code));
}

static void hashMacroTrailingWhitespace()
{
    // whitespace at the end of a replacement list does not count
    simplecpp::DUI dui;
    dui.removeComments = true;
    const char code[] = "#define str(x) #x\n"
                        "#define xstr(x) str(x)\n"
                        "#define MAJOR 1/*major*/\n"
                        "#define MINOR 2 \n"
                        "#define F(x) x\n"
                        "#define G F(a)\n"
                        "xstr(MAJOR.MINOR)\n"
                        "xstr(F(a)b)\n"
                        "xstr(F(a) b)\n"
                        "xstr(G.b)\n"
                        "xstr(G b)";
    ASSERT_EQUALS("\n\n\n\n\n\n"
                  "\"1.2\"\n"
                  "\"ab\"\n"
                  "\"a b\"\n"
                  "\"a.b\"\n"
                  "\"a b\"", preprocess(code, dui));
}

static void hashCombinedOperator()
{
    // the whitespace after an operator of two characters is kept
    const char code[] = "#define str(x) #x\n"
                        "str(a == 0) str(a -> b) str(x... y)\n";
    ASSERT_EQUALS("\n\"a == 0\" \"a -> b\" \"x... y\"", preprocess(code));
}

static void hashEscapeTrailingBackslash()
{
    // an odd number of trailing backslashes would escape the closing quote,
    // the final backslash is dropped (like gcc and clang) and a warning is reported
    const char msg[] = "Invalid string literal when expanding 'str', ignoring final '\\'. "
                       "Undefined behavior according to C standard chapter 6.10.3.2, paragraph 2.\n";
    {
        const char code[] = "int x;\n"
                            "#define str(x) #x\n"
                            "str(\\)\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("int x ;\n\n\"\"", preprocess(code, &outputList));
        ASSERT_EQUALS(std::string("file0,3,portability_stringify,") + msg, toString(outputList));
    }
    {
        const char code[] = "#define str(x) #x\n"
                            "str(a\\)\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("\n\"a\"", preprocess(code, &outputList));
        ASSERT_EQUALS(std::string("file0,2,portability_stringify,") + msg, toString(outputList));
    }
    {
        // three backslashes
        const char code[] = "#define str(x) #x\n"
                            "str(\\\\\\)\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("\n\"\\\\\"", preprocess(code, &outputList));
        ASSERT_EQUALS(std::string("file0,2,portability_stringify,") + msg, toString(outputList));
    }
    {
        // # in a nested macro
        const char code[] = "#define str(x) #x\n"
                            "#define T(x) str(x)\n"
                            "T(\\)\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("\n\n\"\"", preprocess(code, &outputList));
        ASSERT_EQUALS(std::string("file0,3,portability_stringify,") + msg, toString(outputList));
    }
    {
        // in a #if expression
        const char code[] = "#define str(x) #x\n"
                            "#if 1 || str(\\)\n"
                            "A\n"
                            "#endif\n";
        simplecpp::OutputList outputList;
        preprocess(code, &outputList);
        ASSERT_EQUALS(std::string("file0,2,portability_stringify,") + msg +
                      "file0,2,syntax_error,failed to evaluate #if condition, invalid expression\n", toString(outputList));
    }
    {
        // even number of backslashes is fine
        const char code[] = "#define str(x) #x\n"
                            "str(\\\\)\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("\n\"\\\\\"", preprocess(code, &outputList));
        ASSERT_EQUALS("", toString(outputList));
    }
}

static void hashhash1()   // #4703
{
    const char code[] = "#define MACRO( A, B, C ) class A##B##C##Creator {};\n"
                        "MACRO( B\t, U , G )";
    ASSERT_EQUALS("\nclass BUGCreator { } ;", preprocess(code));
}

static void hashhash2()
{
    const char code[] = "#define A(x) a##x\n"
                        "#define B 0\n"
                        "A(B)";
    ASSERT_EQUALS("\n\naB", preprocess(code));
}

static void hashhash3()
{
    const char code[] = "#define A(B) A##B\n"
                        "#define a(B) A(B)\n"
                        "a(A(B))";
    ASSERT_EQUALS("\n\nAAB", preprocess(code));
}

static void hashhash4()    // nonstandard gcc/clang extension for empty varargs
{
    const char *code;

    code = "#define A(x,y...)  a(x,##y)\n"
           "A(1)\n";
    ASSERT_EQUALS("\na ( 1 )", preprocess(code));

    code = "#define A(x, ...)   a(x, ## __VA_ARGS__)\n"
           "#define B(x, ...)   A(x, ## __VA_ARGS__)\n"
           "B(1);";
    ASSERT_EQUALS("\n\na ( 1 ) ;", preprocess(code));
}

static void hashhash4a()
{
    const char code[] = "#define GETMYID(a) ((a))+1\n"
                        "#define FIGHT_FOO(c, ...) foo(c, ##__VA_ARGS__)\n"
                        "#define FIGHT_BAR(c, args...) bar(c, ##args)\n"
                        "FIGHT_FOO(1, GETMYID(a));\n"
                        "FIGHT_BAR(1, GETMYID(b));";
    ASSERT_EQUALS("\n\n\nfoo ( 1 , ( ( a ) ) + 1 ) ;\nbar ( 1 , ( ( b ) ) + 1 ) ;", preprocess(code));
}

static void hashhash5()
{
    ASSERT_EQUALS("x1", preprocess("x##__LINE__"));
}

static void hashhash6()
{
    const char *code;

    code = "#define A(X, ...) LOG(X, ##__VA_ARGS__)\n"
           "A(1,(int)2)";
    ASSERT_EQUALS("\nLOG ( 1 , ( int ) 2 )", preprocess(code));

    code = "#define A(X, ...) LOG(X, ##__VA_ARGS__)\n"
           "#define B(X, ...) A(X, ##__VA_ARGS__)\n"
           "#define C(X, ...) B(X, ##__VA_ARGS__)\n"
           "C(1,(int)2)";
    ASSERT_EQUALS("\n\n\nLOG ( 1 , ( int ) 2 )", preprocess(code));
}

static void hashhash7()   // # ## #  (C standard; 6.10.3.3.p4)
{
    const char *code;

    code = "#define hash_hash # ## #\n"
           "x hash_hash y";
    ASSERT_EQUALS("\nx ## y", preprocess(code));
}

static void hashhash8()
{
    const char code[] = "#define a(xy)    x##y = xy\n"
                        "a(123);";
    ASSERT_EQUALS("\nxy = 123 ;", preprocess(code));
}

static void hashhash9()
{
    const char * code = "#define ADD_OPERATOR(OP) void operator OP ## = (void) { x = x OP 1; }\n"
                        "ADD_OPERATOR(+);\n"
                        "ADD_OPERATOR(-);\n"
                        "ADD_OPERATOR(*);\n"
                        "ADD_OPERATOR(/);\n"
                        "ADD_OPERATOR(%);\n"
                        "ADD_OPERATOR(&);\n"
                        "ADD_OPERATOR(|);\n"
                        "ADD_OPERATOR(^);\n"
                        "ADD_OPERATOR(<<);\n"
                        "ADD_OPERATOR(>>);\n";
    const char expected[] = "\n"
                            "void operator += ( void ) { x = x + 1 ; } ;\n"
                            "void operator -= ( void ) { x = x - 1 ; } ;\n"
                            "void operator *= ( void ) { x = x * 1 ; } ;\n"
                            "void operator /= ( void ) { x = x / 1 ; } ;\n"
                            "void operator %= ( void ) { x = x % 1 ; } ;\n"
                            "void operator &= ( void ) { x = x & 1 ; } ;\n"
                            "void operator |= ( void ) { x = x | 1 ; } ;\n"
                            "void operator ^= ( void ) { x = x ^ 1 ; } ;\n"
                            "void operator <<= ( void ) { x = x << 1 ; } ;\n"
                            "void operator >>= ( void ) { x = x >> 1 ; } ;";
    ASSERT_EQUALS(expected, preprocess(code));

    simplecpp::OutputList outputList;

    code = "#define A +##x\n"
           "A\n";
    outputList.clear();
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'A', Invalid ## usage when expanding 'A': Combining '+' and 'x' yields an invalid token.\n", toString(outputList));

    code = "#define A 2##=\n"
           "A\n";
    outputList.clear();
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'A', Invalid ## usage when expanding 'A': Combining '2' and '=' yields an invalid token.\n", toString(outputList));

    code = "#define A <<##x\n"
           "A\n";
    outputList.clear();
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'A', Invalid ## usage when expanding 'A': Combining '<<' and 'x' yields an invalid token.\n", toString(outputList));
}

static void hashhash10()
{
    const char code[] = "#define x # #\n"
                        "x";
    // a space is added before the #, as gcc and clang do, so the output is not read as a directive.
    // TODO: the old macro expansion gives the tokens the location of the #define
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n # #", " # #", preprocess(code));
}

static void hashFromMacroAtLineStart()
{
    // clang testsuite hash_space.c: a space before the # so that the output is not read as a directive
    const char code[] = "#define HASH #\n"
                        "HASH define foo bar\n";
    // TODO: the old macro expansion gives the # the location of the #define
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n # define foo bar", " #\ndefine foo bar", preprocess(code));
}

static void hashhash11()
{
    const char code[] = "#define x # # #\n"
                        "x";
    // TODO: the old macro expansion gives the tokens the location of the #define
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n # # #", " # # #", preprocess(code));
}

static void hashhash12()
{
    {
        const char code[] = "#define MAX_FOO 1\n"
                            "#define MAX_FOO_AA 2\n"
                            "\n"
                            "#define M(UpperCaseName, b)                "
                            "  do {                                     "
                            "    int MaxValue = MAX_##UpperCaseName;    "
                            "    if (b) {                               "
                            "      MaxValue = MAX_##UpperCaseName##_AA; "
                            "    }                                      "
                            "  } while (0)"
                            "\n"
                            "static void f(bool b) { M(FOO, b); }\n";

        ASSERT_EQUALS("\n\n\n\nstatic void f ( bool b ) { do { int MaxValue = 1 ; if ( b ) { MaxValue = 2 ; } } while ( 0 ) ; }", preprocess(code));
    }

    {
        const char code[] = "#define MAX_FOO (1 * 1)\n"
                            "#define MAX_FOO_AA (2 * 1)\n"
                            "\n"
                            "#define M(UpperCaseName, b)                "
                            "  do {                                     "
                            "    int MaxValue = MAX_##UpperCaseName;    "
                            "    if (b) {                               "
                            "      MaxValue = MAX_##UpperCaseName##_AA; "
                            "    }                                      "
                            "  } while (0)"
                            "\n"
                            "static void f(bool b) { M(FOO, b); }\n";

        ASSERT_EQUALS("\n\n\n\nstatic void f ( bool b ) { do { int MaxValue = ( 1 * 1 ) ; if ( b ) { MaxValue = ( 2 * 1 ) ; } } while ( 0 ) ; }", preprocess(code));
    }
}

static void hashhash13()
{
    const char code[] = "#define X(x) x##U\n"
                        "X((1<<1)-1)";
    ASSERT_EQUALS("\n( 1 << 1 ) - 1U", preprocess(code));

    const char code2[] = "#define CONCAT(x, y) x##y\n"
                         "CONCAT(&a, b)";
    ASSERT_EQUALS("\n& ab", preprocess(code2));
}

static void hashhash_string_literal()
{
    const char code[] =
        "#define UL(x) x##_ul\n"
        "\"ABC\"_ul;\n"
        "UL(\"ABC\");";

    ASSERT_EQUALS("\n\"ABC\" _ul ;\n\"ABC\" _ul ;", preprocess(code));
}

static void hashhash_string_wrapped()
{
    const char code[] =
        "#define CONCAT(a,b) a##b\n"
        "#define STR(x) CONCAT(x,s)\n"
        "STR(\"ABC\");";

    ASSERT_EQUALS("\n\n\"ABC\" s ;", preprocess(code));
}

static void hashhash_char_literal()
{
    const char code[] =
        "#define CH(x) x##_ch\n"
        "CH('a');";

    ASSERT_EQUALS("\n'a' _ch ;", preprocess(code));
}

static void hashhash_multichar_literal()
{
    const char code[] =
        "#define CH(x) x##_ch\n"
        "CH('abcd');";

    ASSERT_EQUALS("\n'abcd' _ch ;", preprocess(code));
}

static void hashhash_char_escaped()
{
    const char code[] =
        "#define CH(x) x##_ch\n"
        "CH('\\'');";

    ASSERT_EQUALS("\n'\\'' _ch ;", preprocess(code));
}

static void hashhash_string_nothing()
{
    const char code[] =
        "#define CONCAT(a,b) a##b\n"
        "CONCAT(\"ABC\",);";

    ASSERT_EQUALS("\n\"ABC\" ;", preprocess(code));
}

static void hashhash_string_char()
{
    const char code[] =
        "#define CONCAT(a,b) a##b\n"
        "CONCAT(\"ABC\", 'c');";

    // This works, but maybe shouldn't since the result isn't useful.
    ASSERT_EQUALS("\n\"ABC\" 'c' ;", preprocess(code));
}

static void hashhash_string_name()
{
    const char code[] =
        "#define CONCAT(a,b) a##b\n"
        "#define LIT _literal\n"
        "CONCAT(\"string\", LIT);";

    // TODO is this correct? clang fails because that's not really a valid thing but gcc seems to accept it
    // see https://gist.github.com/patrickdowling/877a25294f069bf059f3b07f9b5b7039

    // "string" and LIT are not pasted, so the result is rescanned and LIT is expanded.
    // TODO: the old macro expansion does not rescan LIT
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n\n\"string\" _literal ;", "\n\n\"string\" LIT ;", preprocess(code));
}

static void hashhashhash_int_literal()
{
    const char code[] =
        "#define CONCAT(a,b,c) a##b##c\n"
        "#define PASTER(a,b,c) CONCAT(a,b,c)\n"
        "PASTER(\"123\",_i,ul);";

    ASSERT_EQUALS("\n\n\"123\" _iul ;", preprocess(code));
}

static void hashhash_int_literal()
{
    const char code[] =
        "#define PASTE(a,b) a##b\n"
        "PASTE(123,_i);\n"
        "1234_i;\n";

    ASSERT_EQUALS("\n123_i ;\n1234_i ;", preprocess(code));
}

static void hashhash_invalid_1()
{
    const char code[] = "#define  f(a)  (##x)\nf(1)\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'f', Invalid ## usage when expanding 'f': Combining '(' and 'x' yields an invalid token.\n", toString(outputList));
}

static void hashhash_invalid_2()
{
    const char code[] = "#define  f(a)  (x##)\nf(1)\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'f', Invalid ## usage when expanding 'f': Unexpected token ')'\n", toString(outputList));
}

static void hashhash_invalid_string_number()
{
    const char code[] =
        "#define BAD(x) x##12345\nBAD(\"ABC\")\n";

    simplecpp::OutputList outputList;
    preprocess(code, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'BAD', Invalid ## usage when expanding 'BAD': Combining '\"ABC\"' and '12345' yields an invalid token.\n", toString(outputList));
}

static void hashhash_invalid_missing_args()
{
    const char code[] =
        "#define BAD(x) ##x\nBAD()\n";

    simplecpp::OutputList outputList;
    preprocess(code, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'BAD', Invalid ## usage when expanding 'BAD': Missing first argument\n", toString(outputList));
}

static void hashhash_null_stmt()
{
    const char code[] =
        "# define B(x) C ## x\n"
        "#\n"
        "# define C0 1\n"
        "\n"
        "B(0);\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("\n\n\n\n1 ;", preprocess(code, &outputList));
}

static void hashhash_empty_va_args()
{
    // #395 hash hash with an empty __VA_ARGS__ in a macro
    const char code[] =
        "#define CAT(a, ...)        a##__VA_ARGS__\n"
        "#define X(a, ...)          CAT(a)\n"
        "#define LEVEL_2            (2)\n"
        "X(LEVEL_2)\n";
    ASSERT_EQUALS("\n\n\n( 2 )", preprocess(code));
}

static void hashhash_punctuator() // #724
{
    const char code[] =
        "#define cat(a,b) a##b\n"
        "#define MINUS_GT -##>\n"
        "#define VA(...) <##__VA_ARGS__\n"
        "cat(+,=) cat(<<,=) cat(-,>) cat(&,&) cat(|,|) cat(+,+) cat(<,=) cat(.,5) MINUS_GT VA(<) VA()\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("\n\n\n+= <<= -> && || ++ <= .5 -> << <", preprocess(code, &outputList));
    ASSERT_EQUALS("", toString(outputList));
}

static void hashhash_punctuator_invalid()
{
    const char code[] =
        "#define cat(a,b) a##b\n"
        "cat(+,x)\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'cat', Invalid ## usage when expanding 'cat': Combining '+' and 'x' yields an invalid token.\n", toString(outputList));
}

static void macro_c11_example3() // C11 6.10.3.5 EXAMPLE 3
{
    const char code[] =
        "#define x 3\n"
        "#define f(a) f(x * (a))\n"
        "#undef x\n"
        "#define x 2\n"
        "#define g f\n"
        "#define z z[0]\n"
        "#define h g(~\n"
        "#define m(a) a(w)\n"
        "#define w 0,1\n"
        "#define t(a) a\n"
        "#define p() int\n"
        "#define q(x) x\n"
        "#define r(x,y) x ## y\n"
        "#define str(x) # x\n"
        "f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);\n"
        "g(x+(3,4)-w) | h 5) & m\n"
        "(f)^m(m);\n"
        "p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };\n"
        "char c[2][6] = { str(hello), str() };\n";
    // TODO: the old macro expansion leaves an empty token for r(,)
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "f ( 2 * ( y + 1 ) ) + f ( 2 * ( f ( 2 * ( z [ 0 ] ) ) ) ) % f ( 2 * ( 0 ) ) + t ( 1 ) ;\n"
                                     "f ( 2 * ( 2 + ( 3 , 4 ) - 0 , 1 ) ) | f ( 2 * ( ~ 5 ) ) & f ( 2 * ( 0 , 1 ) )\n"
                                     "^ m ( 0 , 1 ) ;\n"
                                     "int i [ ] = { 1 , 23 , 4 , 5 , } ;\n"
                                     "char c [ 2 ] [ 6 ] = { \"hello\" , \"\" } ;",
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "\n"
                                     "f ( 2 * ( y + 1 ) ) + f ( 2 * ( f ( 2 * ( z [ 0 ] ) ) ) ) % f ( 2 * ( 0 ) ) + t ( 1 ) ;\n"
                                     "f ( 2 * ( 2 + ( 3 , 4 ) - 0 , 1 ) ) | f ( 2 * ( ~ 5 ) ) & f ( 2 * ( 0 , 1 ) )\n"
                                     "^ m ( 0 , 1 ) ;\n"
                                     "int i [ ] = { 1 , 23 , 4 , 5 ,  } ;\n"
                                     "char c [ 2 ] [ 6 ] = { \"hello\" , \"\" } ;",
                                     preprocess(code));
}

static void macro_c11_example4() // C11 6.10.3.5 EXAMPLE 4
{
    const char code[] =
        "#define str(s) # s\n"
        "#define xstr(s) str(s)\n"
        "#define debug(s, t) printf(\"x\" # s \"= %d, x\" # t \"= %s\", \\\n"
        " x ## s, x ## t)\n"
        "#define INCFILE(n) vers ## n\n"
        "#define glue(a, b) a ## b\n"
        "#define xglue(a, b) glue(a, b)\n"
        "#define HIGHLOW \"hello\"\n"
        "#define LOW LOW \", world\"\n"
        "debug(1, 2);\n"
        "fputs(str(strncmp(\"abc\\0d\", \"abc\", '\\4') // this goes away\n"
        " == 0) str(: @\\n), s);\n"
        "xstr(INCFILE(2).h)\n"
        "glue(HIGH, LOW);\n"
        "xglue(HIGH, LOW)\n";
    // the comments are removed before preprocessing, like cppcheck and the simplecpp program do
    simplecpp::DUI dui;
    dui.removeComments = true;
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "\n"
                  "printf ( \"x\" \"1\" \"= %d, x\" \"2\" \"= %s\" , x1 , x2 ) ;\n"
                  "fputs ( \"strncmp(\\\"abc\\\\0d\\\", \\\"abc\\\", '\\\\4') == 0\"\n"
                  "\": @\\n\" , s ) ;\n"
                  "\"vers2.h\"\n"
                  "\"hello\" ;\n"
                  "\"hello\" \", world\"",
                  preprocess(code, dui));
}

static void macro_c11_example5() // C11 6.10.3.5 EXAMPLE 5
{
    const char code[] =
        "#define t(x,y,z) x ## y ## z\n"
        "int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,),\n"
        " t(10,,), t(,11,), t(,,12), t(,,) };\n";
    // TODO: the old macro expansion leaves an empty token for t(,,)
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n"
                                     "int j [ ] = { 123 , 45 , 67 , 89 ,\n"
                                     "10 , 11 , 12 , } ;",
                                     "\n"
                                     "int j [ ] = { 123 , 45 , 67 , 89 ,\n"
                                     "10 , 11 , 12 ,  } ;",
                                     preprocess(code));
}

static void macro_c11_example7() // C11 6.10.3.5 EXAMPLE 7
{
    const char code[] =
        "#define debug(...) fprintf(stderr, __VA_ARGS__)\n"
        "#define showlist(...) puts(#__VA_ARGS__)\n"
        "#define report(test, ...) ((test)?puts(#test):\\\n"
        " printf(__VA_ARGS__))\n"
        "debug(\"Flag\");\n"
        "debug(\"X = %d\\n\", x);\n"
        "showlist(The first, second, and third items.);\n"
        "report(x>y, \"x is %d but y is %d\", x, y);\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "\n"
                  "fprintf ( stderr , \"Flag\" ) ;\n"
                  "fprintf ( stderr , \"X = %d\\n\" , x ) ;\n"
                  "puts ( \"The first, second, and third items.\" ) ;\n"
                  "( ( x > y ) ? puts ( \"x>y\" ) : printf ( \"x is %d but y is %d\" , x , y ) ) ;",
                  preprocess(code));
}

static void macro_c23_va_opt_examples() // C23 6.10.5.1 EXAMPLE
{
    const char code[] =
        "#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)\n"
        "#define G(X, ...) f(0, X __VA_OPT__(,) __VA_ARGS__)\n"
        "#define SDEF(sname, ...) S sname __VA_OPT__(= { __VA_ARGS__ })\n"
        "#define EMP\n"
        "F(a,b,c)\n"
        "F()\n"
        "F(EMP)\n"
        "G(a,b,c)\n"
        "G(a,)\n"
        "G(a)\n"
        "SDEF(foo);\n"
        "SDEF(bar, 1, 2);\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "\n"
                  "f ( 0 , a , b , c )\n"
                  "f ( 0 )\n"
                  "f ( 0 )\n"
                  "f ( 0 , a , b , c )\n"
                  "f ( 0 , a )\n"
                  "f ( 0 , a )\n"
                  "S foo ;\n"
                  "S bar = { 1 , 2 } ;",
                  preprocess(code));
}

static void hashhash_empty_placemarker()
{
    const char code[] =
        "#define r(x,y) x ## y\n"
        "{ r(,) }\n";
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    simplecpp::TokenList tokens(files);
    const simplecpp::TokenList rawtokens = makeTokenList(code, files);
    runPreprocess(tokens, rawtokens, files, cache, simplecpp::DUI());
    std::string str;
    for (const simplecpp::Token *tok = tokens.cfront(); tok; tok = tok->next)
        str += "[" + tok->str() + "]";
    // TODO: with the old macro expansion, pasting two empty arguments gives an empty token
    TODO_OLD_EXPANSION_ASSERT_EQUALS("[{][}]", "[{][][}]", str);
}

static void hashhash_invalid_dotdot()
{
    const char code[] =
        "#define cat(a,b) a##b\n"
        "cat(.,.)\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'cat', Invalid ## usage when expanding 'cat': Combining '.' and '.' yields an invalid token.\n", toString(outputList));
}

static void hashhash_invalid_name_string()
{
    const char code[] =
        "#define cat(a,b) a##b\n"
        "cat(a,\"s\")\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'cat', Invalid ## usage when expanding 'cat': Combining 'a' and '\"s\"' yields an invalid token.\n", toString(outputList));
}

static void hashhash_invalid_number_string()
{
    const char code[] =
        "#define cat(a,b) a##b\n"
        "cat(1,\"s\")\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'cat', Invalid ## usage when expanding 'cat': Combining '1' and '\"s\"' yields an invalid token.\n", toString(outputList));
}

static void hashhash_encoding_prefix()
{
    const char code[] =
        "#define cat(a,b) a##b\n"
        "#define WIDE(s) L ## s\n"
        "cat(L,\"s\") cat(u8,\"s\") cat(R,\"(x)\") cat(L,'c') WIDE(\"s\")\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("\n\nL\"s\" u8\"s\" R\"(x)\" L'c' L\"s\"", preprocess(code, &outputList));
    ASSERT_EQUALS("", toString(outputList));
}

static void hashhash_invalid_prefixed_literal()
{
    const char *const codes[] = {"cat(a,L\"s\")", "cat(a,u8\"s\")", "cat(L,L\"s\")", "cat(R,'c')", "cat(u8R,'c')"};
    for (const char *const line : codes) {
        const std::string code = std::string("#define cat(a,b) a##b\n") + line + "\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("", preprocess(code.c_str(), &outputList));
        ASSERT_EQUALS(true, toString(outputList).find("yields an invalid token") != std::string::npos);
    }
}

static void hashhash_va_args_unexpected()
{
    const char code[] =
        "#define C(...)!##__VA_ARGS__\n"
        "C(1)\n";
    simplecpp::OutputList outputList;
    preprocess(code, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'C', Invalid ## usage when expanding 'C': Combining '!' and '1' yields an invalid token.\n", toString(outputList));
}

static void hashhash_universal_character()
{
    const char code[] =
        "#define A(x,y) x##y\nint A(\\u01,04);\n";
    simplecpp::OutputList outputList;
    preprocess(code, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'A', Invalid ## usage when expanding 'A': Combining '\\u01' and '04' yields universal character '\\u0104'. This is undefined behavior according to C standard chapter 5.1.1.2, paragraph 4.\n", toString(outputList));
}

static void hashhash_universal_character_2()
{
    const char code[] =
        "#define A(x,y) x##y\nint A(\\U0104, 0104);\n";
    simplecpp::OutputList outputList;
    preprocess(code, simplecpp::DUI(), &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,failed to expand 'A', Invalid ## usage when expanding 'A': Combining '\\U0104' and '0104' yields universal character '\\U01040104'. This is undefined behavior according to C standard chapter 5.1.1.2, paragraph 4.\n", toString(outputList));
}


static void has_include_1()
{
    const char code[] = "#ifdef __has_include\n"
                        "  #if __has_include(\"simplecpp.h\")\n"
                        "    A\n"
                        "  #else\n"
                        "    B\n"
                        "  #endif\n"
                        "#endif";
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(testSourceDir);
    ASSERT_EQUALS("\n\nA", preprocess(code, dui)); // we default to latest standard internally
    dui.std = "c++14";
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "c++17";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
    dui.std = "c++20";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
}

static void has_include_2()
{
    const char code[] = "#if defined( __has_include)\n"
                        "  #if /*comment*/ __has_include /*comment*/(\"simplecpp.h\") // comment\n"
                        "    A\n"
                        "  #else\n"
                        "    B\n"
                        "  #endif\n"
                        "#endif";
    simplecpp::DUI dui;
    dui.removeComments = true; // TODO: remove this
    dui.includePaths.emplace_back(testSourceDir);
    ASSERT_EQUALS("\n\nA", preprocess(code, dui)); // we default to latest standard internally
    dui.std = "c++14";
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "c++17";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
    dui.std = "c++20";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
}

static void has_include_3()
{
    const char code[] = "#ifdef __has_include\n"
                        "  #if __has_include(<realFileName1.cpp>)\n"
                        "    A\n"
                        "  #else\n"
                        "    B\n"
                        "  #endif\n"
                        "#endif";
    simplecpp::DUI dui;

    // Test file not found...
    ASSERT_EQUALS("\n\n\n\nB", preprocess(code, dui)); // we default to latest standard internally
    dui.std = "c++14";
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "c++17";
    ASSERT_EQUALS("\n\n\n\nB", preprocess(code, dui));

    // Unless -I is set (preferably, we should differentiate -I and -isystem...)
    dui.includePaths.emplace_back(testSourceDir + "/testsuite");
    dui.std = "";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui)); // we default to latest standard internally
    dui.std = "c++14";
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "c++17";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
    dui.std = "c++20";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
}

static void has_include_4()
{
    const char code[] = "#ifdef __has_include\n"
                        "  #if __has_include(\"testsuite/realFileName1.cpp\")\n"
                        "    A\n"
                        "  #else\n"
                        "    B\n"
                        "  #endif\n"
                        "#endif";
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(testSourceDir); // we default to latest standard internally
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
    dui.std = "c++14";
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "c++17";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
    dui.std = "c++20";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
}

static void has_include_5()
{
    const char code[] = "#if defined( __has_include)\n"
                        "  #if !__has_include(<testsuite/unrealFileName2.abcdef>)\n"
                        "    A\n"
                        "  #else\n"
                        "    B\n"
                        "  #endif\n"
                        "#endif";
    simplecpp::DUI dui;
    ASSERT_EQUALS("\n\nA", preprocess(code, dui)); // we default to latest standard internally
    dui.includePaths.emplace_back(testSourceDir);
    dui.std = "c++14";
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "c++17";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
    dui.std = "c++20";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
}

static void has_include_6()
{
    const char code[] = "#if defined( __has_include)\n"
                        "  #if !__has_include(<testsuite/unrealFileName2.abcdef>)\n"
                        "    A\n"
                        "  #else\n"
                        "    B\n"
                        "  #endif\n"
                        "#endif";
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(testSourceDir);
    ASSERT_EQUALS("\n\nA", preprocess(code, dui)); // we default to latest standard internally
    dui.std = "c++99";
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "gnu99";
    ASSERT_EQUALS("\n\nA", preprocess(code, dui));
}

static void define_has_include_invalid_1()
{
    const char code[] = "#define A)__has_include\n"
                        "#if\u000BA\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,2,syntax_error,failed to evaluate #if condition, missing __has_include argument\n", toString(outputList));
}

static void define_has_include_invalid_2()
{
    const char code[] = "#define f __has_include\n"
                        "#if#f<\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,2,syntax_error,failed to evaluate #if condition, missing __has_include argument\n", toString(outputList));
}

static void define_has_include_invalid_3()
{
    const char code[] = "#define\u0000X\u0007__has_include(\n"
                        "#if%X&\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, sizeof(code)-1, &outputList));
    ASSERT_EQUALS("file0,2,syntax_error,failed to evaluate #if condition, invalid __has_include expression\n", toString(outputList));
}

static void define_has_include_invalid_4()
{
    const char code[] = "#define\u0000X\u0000__has_include<2\n"
                        "#if*X\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, sizeof(code)-1, &outputList));
    ASSERT_EQUALS("file0,2,syntax_error,failed to evaluate #if condition, invalid __has_include expression\n", toString(outputList));
}

static void strict_ansi_1()
{
    const char code[] = "#if __STRICT_ANSI__\n"
                        " A\n"
                        "#endif";
    simplecpp::DUI dui;
    dui.std = "gnu99";
    ASSERT_EQUALS("", preprocess(code, dui));
}

static void strict_ansi_2()
{
    const char code[] = "#if __STRICT_ANSI__\n"
                        " A\n"
                        "#endif";
    simplecpp::DUI dui;
    dui.std = "c99";
    ASSERT_EQUALS("\nA", preprocess(code, dui));
}

static void strict_ansi_3()
{
    const char code[] = "#if __STRICT_ANSI__\n"
                        " A\n"
                        "#endif";
    simplecpp::DUI dui;
    dui.std = "c99";
    dui.undefined.insert("__STRICT_ANSI__");
    ASSERT_EQUALS("", preprocess(code, dui));
}

static void strict_ansi_4()
{
    const char code[] = "#if __STRICT_ANSI__\n"
                        " A\n"
                        "#endif";
    simplecpp::DUI dui;
    dui.std = "gnu99";
    dui.defines.emplace_back("__STRICT_ANSI__");
    ASSERT_EQUALS("\nA", preprocess(code, dui));
}

static void ifdef1()
{
    const char code[] = "#ifdef A\n"
                        "1\n"
                        "#else\n"
                        "2\n"
                        "#endif";
    ASSERT_EQUALS("\n\n\n2", preprocess(code));
}

static void ifdef2()
{
    const char code[] = "#define A\n"
                        "#ifdef A\n"
                        "1\n"
                        "#else\n"
                        "2\n"
                        "#endif";
    ASSERT_EQUALS("\n\n1", preprocess(code));
}

static void ifndef()
{
    const char code1[] = "#define A\n"
                         "#ifndef A\n"
                         "1\n"
                         "#endif";
    ASSERT_EQUALS("", preprocess(code1));

    const char code2[] = "#ifndef A\n"
                         "1\n"
                         "#endif";
    ASSERT_EQUALS("\n1", preprocess(code2));
}

static void ifA()
{
    const char code[] = "#if A==1\n"
                        "X\n"
                        "#endif";
    ASSERT_EQUALS("", preprocess(code));

    simplecpp::DUI dui;
    dui.defines.emplace_back("A=1");
    ASSERT_EQUALS("\nX", preprocess(code, dui));
}

static void ifFloatingPointLiteral() // #612
{
    simplecpp::OutputList outputList;
    simplecpp::DUI dui;
    dui.defines.emplace_back("A=1");

    const char issue[] = "#if A == 1.0\n"
                         "int i;\n"
                         "#endif\n";
    ASSERT_EQUALS("", preprocess(issue, dui, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, floating point literal in preprocessor expression\n", toString(outputList));

    outputList.clear();

    const char otherSpelling[] = "#if 0\n"
                                 "#elif .5\n"
                                 "int i;\n"
                                 "#endif\n";
    ASSERT_EQUALS("", preprocess(otherSpelling, &outputList));
    ASSERT_EQUALS("file0,2,syntax_error,failed to evaluate #elif condition, floating point literal in preprocessor expression\n", toString(outputList));

    outputList.clear();

    const char exponent[] = "#if 1e3\n"
                            "int i;\n"
                            "#endif\n";
    ASSERT_EQUALS("", preprocess(exponent, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, floating point literal in preprocessor expression\n", toString(outputList));

    outputList.clear();

    const char suffix[] = "#if 1.f\n"
                          "int i;\n"
                          "#endif\n";
    ASSERT_EQUALS("", preprocess(suffix, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, floating point literal in preprocessor expression\n", toString(outputList));

    outputList.clear();

    const char hexadecimal[] = "#if 0x1p3\n"
                               "int i;\n"
                               "#endif\n";
    ASSERT_EQUALS("", preprocess(hexadecimal, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, floating point literal in preprocessor expression\n", toString(outputList));

    outputList.clear();

    const char integer[] = "#if A == 1\n"
                           "int i;\n"
                           "#endif\n";
    ASSERT_EQUALS("\nint i ;", preprocess(integer, dui, &outputList));
    ASSERT_EQUALS("", toString(outputList));
}

static void ifCharLiteral()
{
    const char code[] = "#if ('A'==0x41)\n"
                        "123\n"
                        "#endif";
    ASSERT_EQUALS("\n123", preprocess(code));
}

static void ifDefined()
{
    const char code[] = "#if defined(A)\n"
                        "X\n"
                        "#endif";
    simplecpp::DUI dui;
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.defines.emplace_back("A=1");
    ASSERT_EQUALS("\nX", preprocess(code, dui));
}

static void ifDefinedNoPar()
{
    const char code[] = "#if defined A\n"
                        "X\n"
                        "#endif";
    simplecpp::DUI dui;
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.defines.emplace_back("A=1");
    ASSERT_EQUALS("\nX", preprocess(code, dui));
}

static void ifDefinedNested()
{
    const char code[] = "#define FOODEF defined(FOO)\n"
                        "#if FOODEF\n"
                        "X\n"
                        "#endif";
    simplecpp::DUI dui;
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.defines.emplace_back("FOO=1");
    ASSERT_EQUALS("\n\nX", preprocess(code, dui));
}

static void ifDefinedNestedNoPar()
{
    const char code[] = "#define FOODEF defined FOO\n"
                        "#if FOODEF\n"
                        "X\n"
                        "#endif";
    simplecpp::DUI dui;
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.defines.emplace_back("FOO=1");
    ASSERT_EQUALS("\n\nX", preprocess(code, dui));
}

static void ifDefinedInvalid1()   // #50 - invalid unterminated defined
{
    const char code[] = "#if defined(A\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition\n", toString(outputList));
}

static void ifDefinedInvalid2()
{
    const char code[] = "#if defined\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition\n", toString(outputList));
}

static void ifDefinedHashHash()
{
    const char code[] = "#define ENABLE(FEATURE)  defined ENABLE_##FEATURE\n"
                        "#define ENABLE_FOO 1\n"
                        "#if ENABLE(FOO)\n"
                        "#error FOO is enabled\n" // <-- expected result
                        "#else\n"
                        "#error FOO is not enabled\n"
                        "#endif\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,4,#error,#error FOO is enabled\n", toString(outputList));
}

static void ifDefinedHashHash2()
{
    // #409
    // do not crash when expanding P()  (as ## rhs is "null")
    // note: gcc outputs "defined E"
    const char code[] = "#define P(p)defined E##p\n"
                        "P()\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("\n0", preprocess(code, &outputList));
}

static void definedAtEndOfFile()
{
    // "defined" from a macro expansion is undefined behavior; gcc outputs "defined" here.
    // The operand is the last token of the file.
    const char code[] = "#define X\n"
                        "#define D defined X\n"
                        "D";
    ASSERT_EQUALS("\n\n1", preprocess(code));
}

static void ifLogical()
{
    const char code[] = "#if defined(A) || defined(B)\n"
                        "X\n"
                        "#endif";
    simplecpp::DUI dui;
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.defines.clear();
    dui.defines.emplace_back("A=1");
    ASSERT_EQUALS("\nX", preprocess(code, dui));
    dui.defines.clear();
    dui.defines.emplace_back("B=1");
    ASSERT_EQUALS("\nX", preprocess(code, dui));
}

static void ifLogicalPrecedence()
{
    // && has higher precedence than ||
    const char code1[] = "#if 1 || 1 && 0\n"
                         "X\n"
                         "#endif";
    ASSERT_EQUALS("\nX", preprocess(code1));
    const char code2[] = "#if 1 or 1 and 0\n"
                         "X\n"
                         "#endif";
    ASSERT_EQUALS("\nX", preprocess(code2));
    const char code3[] = "#if 0 && 1 || 1\n"
                         "X\n"
                         "#endif";
    ASSERT_EQUALS("\nX", preprocess(code3));
    const char code4[] = "#if 0 || 1 && 0\n"
                         "X\n"
                         "#endif";
    ASSERT_EQUALS("", preprocess(code4));
}

static void ifSizeof()
{
    const char code[] = "#if sizeof(unsigned short)==2\n"
                        "X\n"
                        "#else\n"
                        "Y\n"
                        "#endif";
    ASSERT_EQUALS("\nX", preprocess(code));
}

static void elif()
{
    const char code1[] = "#ifndef X\n"
                         "1\n"
                         "#elif 1<2\n"
                         "2\n"
                         "#else\n"
                         "3\n"
                         "#endif";
    ASSERT_EQUALS("\n1", preprocess(code1));

    const char code2[] = "#ifdef X\n"
                         "1\n"
                         "#elif 1<2\n"
                         "2\n"
                         "#else\n"
                         "3\n"
                         "#endif";
    ASSERT_EQUALS("\n\n\n2", preprocess(code2));

    const char code3[] = "#ifdef X\n"
                         "1\n"
                         "#elif 1>2\n"
                         "2\n"
                         "#else\n"
                         "3\n"
                         "#endif";
    ASSERT_EQUALS("\n\n\n\n\n3", preprocess(code3));

    // #elif after a taken group must not be evaluated
    const char code4[] = "#if 1\n"
                         "1\n"
                         "#elif 1/0\n"
                         "2\n"
                         "#elif garbage(\n"
                         "3\n"
                         "#endif";
    ASSERT_EQUALS("\n1", preprocess(code4));
}

static void elifdef()
{
    {
        const char code[] = "#if 1\n"
                            "1\n"
                            "#elifdef X\n"
                            "2\n"
                            "#else\n"
                            "3\n"
                            "#endif";
        ASSERT_EQUALS("\n1", preprocess(code));
    }
    {
        const char code[] = "#define X\n"
                            "#if 0\n"
                            "1\n"
                            "#elifdef X\n"
                            "2\n"
                            "#else\n"
                            "3\n"
                            "#endif";
        ASSERT_EQUALS("\n\n\n\n2", preprocess(code));
    }
    {
        const char code[] = "#if 0\n"
                            "1\n"
                            "#elifdef X\n"
                            "2\n"
                            "#else\n"
                            "3\n"
                            "#endif";
        ASSERT_EQUALS("\n\n\n\n\n3", preprocess(code));
    }
}

static void elifndef()
{
    {
        const char code[] = "#if 1\n"
                            "1\n"
                            "#elifndef X\n"
                            "2\n"
                            "#else\n"
                            "3\n"
                            "#endif";
        ASSERT_EQUALS("\n1", preprocess(code));
    }
    {
        const char code[] = "#if 0\n"
                            "1\n"
                            "#elifndef X\n"
                            "2\n"
                            "#else\n"
                            "3\n"
                            "#endif";
        ASSERT_EQUALS("\n\n\n2", preprocess(code));
    }
    {
        const char code[] = "#define X\n"
                            "#if 0\n"
                            "1\n"
                            "#elifndef X\n"
                            "2\n"
                            "#else\n"
                            "3\n"
                            "#endif";
        ASSERT_EQUALS("\n\n\n\n\n\n3", preprocess(code));
    }
}

static void ifif()
{
    // source code from LLVM
    const char code[] = "#if defined(__has_include)\n"
                        "#if __has_include(<sanitizer / coverage_interface.h>)\n"
                        "#endif\n"
                        "#endif\n";
    ASSERT_EQUALS("", preprocess(code));
}

static void ifoverflow()
{
    // source code from CLANG
    const char code[] = "#if 0x7FFFFFFFFFFFFFFF*2\n"
                        "#endif\n"
                        "#if 0xFFFFFFFFFFFFFFFF*2\n"
                        "#endif\n"
                        "#if 0x7FFFFFFFFFFFFFFF+1\n"
                        "#endif\n"
                        "#if 0xFFFFFFFFFFFFFFFF+1\n"
                        "#endif\n"
                        "#if 0x7FFFFFFFFFFFFFFF--1\n"
                        "#endif\n"
                        "#if 0xFFFFFFFFFFFFFFFF--1\n"
                        "#endif\n"
                        "123";
    (void)preprocess(code);
}

static void ifdiv0()
{
    const char code[] = "#if 1000/0\n"
                        "#endif\n"
                        "123";
    ASSERT_EQUALS("", preprocess(code));
}

static void ifShortCircuit()
{
    // operands that are not evaluated must not cause errors
    const char * const exprs[] = {
        "0 && (1/0)",
        "!(1 || (1/0))",
        "0 ? 1/0 : 0",
        "1 ? 0 : 1/0",
        "0 && 1 % 0 && 1",
        "(1 || 1/0) && !(0 && 1/0)  && 0",
        "0 && (2 + (3 * (1/0)))",
        "0 && -(1/0)",
        "0 and (1/0)",
        "!(1 or 1/0)",
        "defined(X) and X/0",
        "0 && 1 bitor (1/0)",
        "!(1 || (1/0) bitand 1)",
        "0 and 1 xor (1/0)",
        "0 && not (1/0)",
        "0 && compl (1/0)",
        "!(1 || not not (1/0))"
    };
    for (const char * const expr : exprs) {
        const std::string code = std::string("#if ") + expr + "\n"
                                 "1\n"
                                 "#else\n"
                                 "2\n"
                                 "#endif\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("\n\n\n2", preprocess(code.c_str(), &outputList));
        ASSERT_EQUALS("", toString(outputList));
    }

    // evaluated operands are still checked
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess("#if 1 && (1/0)\n#endif\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, division/modulo by zero\n", toString(outputList));
    outputList.clear();
    ASSERT_EQUALS("", preprocess("#if not (1/0)\n#endif\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, division/modulo by zero\n", toString(outputList));
}

static void ifZeroCondition()
{
    // a zero value that is not written as 0 is false
    const char * const exprs[] = {
        "00 ? 1 : 0",
        "-0 ? 1 : 0",
        "-X ? 1 : 0",
        "0L ? 1 : 0",
        "!00 ? 0 : 1",
        "!0L ? 0 : 1"
    };
    for (const char * const expr : exprs) {
        const std::string code = std::string("#if ") + expr + "\n"
                                 "1\n"
                                 "#else\n"
                                 "2\n"
                                 "#endif\n";
        ASSERT_EQUALS("\n\n\n2", preprocess(code.c_str()));
    }

    // the selected operand is evaluated
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess("#if 00 ? 1 : 1/0\n#endif\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, division/modulo by zero\n", toString(outputList));
}

static void ifUnaryOperators()
{
    // unary operators in a row
    const char * const exprs[] = {
        "!!1",
        "!-0",
        "!+0",
        "!~-1",
        "- -1",
        "-!0",
        "-~0",
        "~!0",
        "+-1 == -1",
        "1 && !-0",
        "!-X",
        "-(-1) == 1",
        "- - -1 == -1",
        "1 - -1 == 2"
    };
    for (const char * const expr : exprs) {
        const std::string code = std::string("#if ") + expr + "\n"
                                 "1\n"
                                 "#else\n"
                                 "2\n"
                                 "#endif\n";
        ASSERT_EQUALS("\n1", preprocess(code.c_str()));
    }
}

static void ifInvalidExpression()
{
    const char * const exprs[] = {
        // missing operand
        "1 +",
        "(1 +)",
        "(1 +) + 1",
        "* 1",
        "(* 1)",
        "1 + * 2",
        "1 <<",
        "(1 ==)",
        "1 &",
        "(1 &&)",
        "|| 1",
        "!",
        "(!)",
        "-",
        "~",
        "(? 1 : 2)",
        "(1 ? 2 :)",
        "(1 ? 2 3)",
        "(1 ? : 2)",
        // missing operator
        "1 2",
        "(1 2)",
        // empty parentheses
        "()",
        "1 + ()",
        // syntax errors are reported also in operands that are not evaluated
        "0 && (1 +)"
    };
    for (const char * const expr : exprs) {
        const std::string code = std::string("#if ") + expr + "\n"
                                 "1\n"
                                 "#endif\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("", preprocess(code.c_str(), &outputList));
        ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, invalid expression\n", toString(outputList));
    }
}

static void ifTrueFalse()
{
    const char code[] = "#if true && !false\n"
                        "1\n"
                        "#else\n"
                        "2\n"
                        "#endif\n";
    simplecpp::DUI dui;
    ASSERT_EQUALS("\n1", preprocess(code, dui, nullptr));
    dui.std = "c++11";
    ASSERT_EQUALS("\n1", preprocess(code, dui, nullptr));
    dui.std = "c23";
    ASSERT_EQUALS("\n1", preprocess(code, dui, nullptr));
    // before C23, true and false are ordinary identifiers that evaluate to 0
    dui.std = "c17";
    ASSERT_EQUALS("\n\n\n2", preprocess(code, dui, nullptr));
}

static void ifalt()   // using "and", "or", etc
{
    const char *code;

    code = "#if 1 and 1\n"
           "1\n"
           "#else\n"
           "2\n"
           "#endif\n";
    ASSERT_EQUALS("\n1", preprocess(code));

    code = "#if 1 or 0\n"
           "1\n"
           "#else\n"
           "2\n"
           "#endif\n";
    ASSERT_EQUALS("\n1", preprocess(code));
}

static void ifAltUnaryOperators()   // "not" and "compl" anywhere a unary operator is allowed
{
    const char * const exprs[] = {
        // at the start of the expression
        "not 0",
        "(not 0)",
        "not 0 && 1",
        "compl 0",
        // after a binary operator, "?" or ":"
        "1 && not 0",
        "1 and not 0",
        "0 || not 0",
        "1 == not 0",
        "0 ? 0 : not 0",
        "1 ? not 0 : 0",
        // next to another unary operator
        "not not 1",
        "not -0",
        "not ~-1",
        "not !0 == 0",
        "compl compl 1 == 1",
        "compl -1 == 0",
        "!(compl -1 + 1 == 0)",
        "-compl 0 == 1",
        // followed by "("
        "not (0)",
        "not(0)",
        "compl (0) == -1"
    };
    for (const char * const expr : exprs) {
        const std::string code = std::string("#if ") + expr + "\n"
                                 "1\n"
                                 "#else\n"
                                 "2\n"
                                 "#endif\n";
        ASSERT_EQUALS("\n1", preprocess(code.c_str()));
    }
}

static void ifexpr()
{
    const char code[] = "#define MACRO()  (1)\n"
                        "#if ~MACRO() & 8\n"
                        "1\n"
                        "#endif";
    ASSERT_EQUALS("\n\n1", preprocess(code));
}

static void ifUndefFuncStyleMacro()
{
    const char code[] = "#if A(<dir/file.h>)\n"
                        "#endif\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, undefined function-like macro invocation: A( ... )\n", toString(outputList));
}

static void location1()
{
    const char *code;

    code =  "# 1 \"main.c\"\n\n\n"
           "x";
    ASSERT_EQUALS("\n#line 3 \"main.c\"\nx", preprocess(code));
}

static void location2()
{
    const char *code;

    code = "{ {\n"
           "#line 40 \"abc.y\"\n"
           "{\n"
           "}\n"
           "#line 42 \"abc.y\"\n"
           "{\n"
           "}\n"
           "} }";

    ASSERT_EQUALS("{ {\n"
                  "#line 40 \"abc.y\"\n"
                  "{\n"
                  "}\n"
                  "{\n"
                  "}\n"
                  "} }", preprocess(code));
}

static void location3()
{
    const char *code;
    code = "#line 1 \"x\" \n"
           "a\n"
           "#line 1 \"x\" \n"
           "b\n";
    ASSERT_EQUALS("\n#line 1 \"x\"\na b", preprocess(code));
}

static void location4()
{
    const char *code;
    code = "#line 1 \"abc\\\\def.g\" \n"
           "a\n";
    ASSERT_EQUALS("\n#line 1 \"abc\\def.g\"\na", preprocess(code));
}

static void location5()
{
    // https://sourceforge.net/p/cppcheck/discussion/general/thread/eccf020a13/
    const char *code;
    code = "#line 10 \"/a/Attribute/parser/FilterParser.y\" // lalr1.cc:377\n"
           "int x;\n";
    ASSERT_EQUALS("\n#line 10 \"/a/Attribute/parser/FilterParser.y\"\n"
                  "int x ;", preprocess(code));
}

static void location6()
{
    const char code[] =
        "#line 3\n"
        "__LINE__ __FILE__\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "3 \"\"",
                  preprocess(code));
}

static void location7()
{
    const char code[] =
        "#line 3 \"file.c\"\n"
        "__LINE__ __FILE__\n";
    ASSERT_EQUALS("\n"
                  "#line 3 \"file.c\"\n"
                  "3 \"file.c\"",
                  preprocess(code));
}

static void location8()
{
    const char code[] =
        "# 3\n"
        "__LINE__ __FILE__\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "3 \"\"",
                  preprocess(code));
}

static void location9()
{
    const char code[] =
        "# 3 \"file.c\"\n"
        "__LINE__ __FILE__\n";
    ASSERT_EQUALS("\n"
                  "#line 3 \"file.c\"\n"
                  "3 \"file.c\"",
                  preprocess(code));
}

static void location10()
{
    const char code[] =
        "#line 3\n"
        "__LINE__ __FILE__\n";
    ASSERT_EQUALS("\n"
                  "\n" // TODO: should this have the #line marker?
                  "3 \"\"",
                  preprocess(code));
}

static void location11()
{
    const char code[] =
        "#line 3 \"file.c\"\n"
        "__LINE__ __FILE__\n"
        "#line 33 \"file2.c\"\n"
        "__LINE__ __FILE__\n";
    ASSERT_EQUALS("\n"
                  "#line 3 \"file.c\"\n"
                  "3 \"file.c\"\n"
                  "#line 33 \"file2.c\"\n"
                  "33 \"file2.c\"",
                  preprocess(code));
}

static void location12()
{
    simplecpp::DUI dui;
    dui.removeComments = true;

    const char code[] =
        "/**//**/#/**//**/line/**//**/3/**//**/\"file.c\"/**/\n"
        "__LINE__ __FILE__\n";
    ASSERT_EQUALS("\n"
                  "#line 3 \"file.c\"\n"
                  "3 \"file.c\"",
                  preprocess(code, dui));
}


static void missingHeader1()
{
    const char code[] = "#include \"notexist.h\"\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,missing_header,Header not found: \"notexist.h\"\n", toString(outputList));
}

static void missingHeader2()
{
    const char code[] = "#include \"foo.h\"\n"; // this file exists
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;
    cache.insert({"foo.h", simplecpp::TokenList(files)});
    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    const simplecpp::TokenList rawtokens = makeTokenList(code,files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(".");
    runPreprocess(tokens2, rawtokens, files, cache, dui, &outputList);
    ASSERT_EQUALS("", toString(outputList));
}

static void missingHeader3()
{
    const char code[] = "#ifdef UNDEFINED\n#include \"notexist.h\"\n#endif\n"; // this file is not included
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("", toString(outputList));
}

static void missingHeader4()
{
    const char code[] = "#/**/include <>\n";
    simplecpp::OutputList outputList;
    simplecpp::DUI dui;
    dui.removeComments = true; // TODO: remove this
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,No header in #include\n", toString(outputList));
}

static void nestedInclude()
{
    const char code[] = "#include \"test.h\"\n";
    std::vector<std::string> files;
    const simplecpp::TokenList rawtokens = makeTokenList(code,files,"test.h");
    simplecpp::FileDataCache cache;
    cache.insert({"test.h", rawtokens});

    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(".");
    runPreprocess(tokens2, rawtokens, files, cache, dui, &outputList);

    ASSERT_EQUALS("file0,1,include_nested_too_deeply,#include nested too deeply\n", toString(outputList));
}

static void systemInclude()
{
    const char code[] = "#include <limits.h>\n";
    std::vector<std::string> files;
    const simplecpp::TokenList rawtokens = makeTokenList(code,files,"local/limits.h");
    simplecpp::FileDataCache cache;
    cache.insert({"include/limits.h", simplecpp::TokenList(files)});
    cache.insert({"local/limits.h", rawtokens});

    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back("include");
    runPreprocess(tokens2, rawtokens, files, cache, dui, &outputList);

    ASSERT_EQUALS("", toString(outputList));
}

static void circularInclude()
{
    std::vector<std::string> files;
    simplecpp::FileDataCache cache;

    {
        const char *const path = "test.h";
        const char code[] =
            "#ifndef TEST_H\n"
            "#define TEST_H\n"
            "#include \"a/a.h\"\n"
            "#endif\n"
        ;
        cache.insert({path, makeTokenList(code, files, path)});
    }

    {
        const char *const path = "a/a.h";
        const char code[] =
            "#ifndef A_H\n"
            "#define A_H\n"
            "#include \"../test.h\"\n"
            "#endif\n"
        ;
        cache.insert({path, makeTokenList(code, files, path)});
    }

    simplecpp::OutputList outputList;
    simplecpp::TokenList tokens2(files);
    {
        std::vector<std::string> filenames;
        const simplecpp::DUI dui;

        const char code[] = "#include \"test.h\"\n";
        const simplecpp::TokenList rawtokens = makeTokenList(code, files, "test.cpp");

        cache = simplecpp::load(rawtokens, filenames, dui, &outputList, std::move(cache));
        runPreprocess(tokens2, rawtokens, files, cache, dui, &outputList);
    }

    ASSERT_EQUALS("", toString(outputList));
}

static void multiline1()
{
    const char code[] = "#define A \\\n"
                        "1\n"
                        "A";
    ASSERT_EQUALS("\n\n1", preprocess(code));
}

static void multiline2()
{
    const char code[] = "#define A /*\\\n"
                        "*/1\n"
                        "A";
    std::vector<std::string> files;
    simplecpp::TokenList rawtokens = makeTokenList(code,files);
    ASSERT_EQUALS("# define A /**/ 1\n\nA", rawtokens.stringify());
    rawtokens.removeComments();
    simplecpp::FileDataCache cache;
    simplecpp::TokenList tokens2(files);
    runPreprocess(tokens2, rawtokens, files, cache, simplecpp::DUI());
    ASSERT_EQUALS("\n\n1", tokens2.stringify());
}

static void multiline3()   // #28 - macro with multiline comment
{
    const char code[] = "#define A /*\\\n"
                        "           */ 1\n"
                        "A";
    std::vector<std::string> files;
    simplecpp::TokenList rawtokens = makeTokenList(code,files);
    ASSERT_EQUALS("# define A /*           */ 1\n\nA", rawtokens.stringify());
    rawtokens.removeComments();
    simplecpp::FileDataCache cache;
    simplecpp::TokenList tokens2(files);
    runPreprocess(tokens2, rawtokens, files, cache, simplecpp::DUI());
    ASSERT_EQUALS("\n\n1", tokens2.stringify());
}

static void multiline4()   // #28 - macro with multiline comment
{
    const char code[] = "#define A \\\n"
                        "          /*\\\n"
                        "           */ 1\n"
                        "A";
    std::vector<std::string> files;
    simplecpp::TokenList rawtokens = makeTokenList(code,files);
    ASSERT_EQUALS("# define A /*           */ 1\n\n\nA", rawtokens.stringify());
    rawtokens.removeComments();
    simplecpp::FileDataCache cache;
    simplecpp::TokenList tokens2(files);
    runPreprocess(tokens2, rawtokens, files, cache, simplecpp::DUI());
    ASSERT_EQUALS("\n\n\n1", tokens2.stringify());
}

static void multiline5()   // column
{
    const char code[] = "#define A\\\n"
                        "(";
    std::vector<std::string> files;
    const simplecpp::TokenList rawtokens = makeTokenList(code, files);
    ASSERT_EQUALS("# define A (", rawtokens.stringify());
    ASSERT_EQUALS(11, rawtokens.cback()->location.col);
}

static void multiline6()   // multiline string in macro
{
    const char code[] = "#define string  (\"\\\n"
                        "x\")\n"
                        "string\n";
    std::vector<std::string> files;
    const simplecpp::TokenList rawtokens = makeTokenList(code, files);
    ASSERT_EQUALS("# define string ( \"x\" )\n"
                  "\n"
                  "string", rawtokens.stringify());
}

static void multiline7()   // multiline string in macro
{
    const char code[] = "#define A(X) aaa { f(\"\\\n"
                        "a\"); }\n"
                        "A(1)";
    std::vector<std::string> files;
    const simplecpp::TokenList rawtokens = makeTokenList(code, files);
    ASSERT_EQUALS("# define A ( X ) aaa { f ( \"a\" ) ; }\n"
                  "\n"
                  "A ( 1 )", rawtokens.stringify());
}

static void multiline8()   // multiline prefix string in macro
{
    const char code[] = "#define A L\"a\\\n"
                        " b\"\n"
                        "A;";
    ASSERT_EQUALS("\n\nL\"a b\" ;", preprocess(code));
}

static void multiline9()   // multiline prefix string in macro
{
    const char code[] = "#define A u8\"a\\\n"
                        " b\"\n"
                        "A;";
    ASSERT_EQUALS("\n\nu8\"a b\" ;", preprocess(code));
}

static void multiline10() // multiline string literal
{
    const char code[] = "const char *ptr = \"\\\\\n"
                        "\\n\";";
    ASSERT_EQUALS("const char * ptr = \"\\\\n\"\n;", preprocess(code));
}

static void nullDirective1()
{
    const char code[] = "#\n"
                        "#if 1\n"
                        "#define a 1\n"
                        "#endif\n"
                        "x = a;\n";

    ASSERT_EQUALS("\n\n\n\nx = 1 ;", preprocess(code));
}

static void nullDirective2()
{
    const char code[] = "# // comment\n"
                        "#if 1\n"
                        "#define a 1\n"
                        "#endif\n"
                        "x = a;\n";

    ASSERT_EQUALS("\n\n\n\nx = 1 ;", preprocess(code));
}

static void nullDirective3()
{
    const char code[] = "#if 1\n"
                        "#define a 1\n"
                        "#\n"
                        "#endif\n"
                        "x = a;\n";

    ASSERT_EQUALS("\n\n\n\nx = 1 ;", preprocess(code));
}

static void lineDirective()
{
    for (const std::string std : {"c89", "c90", "c++03"}) {
        std::string std_name;
        if (simplecpp::getCStd(std) != simplecpp::CUnknown)
            std_name = simplecpp::getCStdName(simplecpp::getCStd(std));
        else
            std_name = simplecpp::getCppStdName(simplecpp::getCppStd(std));

        simplecpp::DUI dui;
        dui.std = std;

        {
            const char code[] =
                "#line -1\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,syntax_error,Invalid character in line directive: '-'.\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 0\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 0. Line number zero is undefined behavior.\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 32767\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("", toString(outputList));
        }

        {
            const char code[] =
                "#line 32768\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 32768. Line numbers above 32767 are undefined behavior in " + std_name + ".\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 18446744073709551617\n" // 2^64 + 1
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 18446744073709551617. Line numbers above 32767 are undefined behavior in " + std_name + ".\n",
                          toString(outputList));
        }
    }

    for (const std::string std : {"c99", "c11", "c17", "c23", "c2y", "c++11", "c++14", "c++17", "c++20", "c++23"}) {
        std::string std_name;
        if (simplecpp::getCStd(std) != simplecpp::CUnknown)
            std_name = simplecpp::getCStdName(simplecpp::getCStd(std));
        else
            std_name = simplecpp::getCppStdName(simplecpp::getCppStd(std));

        simplecpp::DUI dui;
        dui.std = std;

        {
            const char code[] =
                "#line -1\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,syntax_error,Invalid character in line directive: '-'.\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 0\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 0. Line number zero is undefined behavior.\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 32767\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("", toString(outputList));
        }

        {
            const char code[] =
                "#line 32768\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("", toString(outputList));
        }

        {
            const char code[] =
                "#line 2147483647\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("", toString(outputList));
        }

        {
            const char code[] =
                "#line 2147483648\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 2147483648. Line numbers above 2147483647 are undefined behavior in " + std_name + ".\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 18446744073709551617\n" // 2^64 + 1
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 18446744073709551617. Line numbers above 2147483647 are undefined behavior in " + std_name + ".\n",
                          toString(outputList));
        }
    }

    {
        const std::string std_name = "C++26";

        simplecpp::DUI dui;
        dui.std = "c++26";

        {
            const char code[] =
                "#line -1\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,syntax_error,Invalid character in line directive: '-'.\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 0\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 0. Line number zero is undefined behavior.\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 32767\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("", toString(outputList));
        }

        {
            const char code[] =
                "#line 32768\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("", toString(outputList));
        }

        {
            const char code[] =
                "#line 2147483647\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("", toString(outputList));
        }

        {
            const char code[] =
                "#line 2147483648\n"
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 2147483648. Line numbers above 2147483647 are conditionally supported in " + std_name + ".\n",
                          toString(outputList));
        }

        {
            const char code[] =
                "#line 18446744073709551617\n" // 2^64 + 1
                ";\n";
            simplecpp::OutputList outputList;
            makeTokenList(code, dui, &outputList);
            ASSERT_EQUALS("file0,2,portability_line_directive,Line number out of range: 18446744073709551617. Line numbers above 2147483647 are conditionally supported in " + std_name + ".\n",
                          toString(outputList));
        }
    }
}

static void include1()
{
    const char code[] = "#include \"A.h\"\n";
    ASSERT_EQUALS("# include \"A.h\"", readfile(code));
}

static void include2()
{
    const char code[] = "#include <A.h>\n";
    ASSERT_EQUALS("# include <A.h>", readfile(code));
}

static void include3()   // #16 - crash when expanding macro from header
{
    const char code_c[] = "#include \"A.h\"\n"
                          "glue(1,2,3,4)\n";
    const char code_h[] = "#define glue(a,b,c,d) a##b##c##d\n";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens_c = makeTokenList(code_c, files, "A.c");
    const simplecpp::TokenList rawtokens_h = makeTokenList(code_h, files, "A.h");

    ASSERT_EQUALS(2U, files.size());
    ASSERT_EQUALS("A.c", files[0]);
    ASSERT_EQUALS("A.h", files[1]);

    simplecpp::FileDataCache cache;
    cache.insert({"A.c", rawtokens_c});
    cache.insert({"A.h", rawtokens_h});

    simplecpp::TokenList out(files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(".");
    runPreprocess(out, rawtokens_c, files, cache, dui);

    ASSERT_EQUALS("\n1234", out.stringify());
}


static void include4()   // #27 - -include
{
    const char code_c[] = "X\n";
    const char code_h[] = "#define X 123\n";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens_c = makeTokenList(code_c, files, "27.c");
    const simplecpp::TokenList rawtokens_h = makeTokenList(code_h, files, "27.h");

    ASSERT_EQUALS(2U, files.size());
    ASSERT_EQUALS("27.c", files[0]);
    ASSERT_EQUALS("27.h", files[1]);

    simplecpp::FileDataCache cache;
    cache.insert({"27.c", rawtokens_c});
    cache.insert({"27.h", rawtokens_h});

    simplecpp::TokenList out(files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(".");
    dui.includes.emplace_back("27.h");
    runPreprocess(out, rawtokens_c, files, cache, dui);

    ASSERT_EQUALS("123", out.stringify());
}

static void include5()    // #3 - handle #include MACRO
{
    const char code_c[] = "#define A \"3.h\"\n#include A\n";
    const char code_h[] = "123\n";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens_c = makeTokenList(code_c, files, "3.c");
    const simplecpp::TokenList rawtokens_h = makeTokenList(code_h, files, "3.h");

    ASSERT_EQUALS(2U, files.size());
    ASSERT_EQUALS("3.c", files[0]);
    ASSERT_EQUALS("3.h", files[1]);

    simplecpp::FileDataCache cache;
    cache.insert({"3.c", rawtokens_c});
    cache.insert({"3.h", rawtokens_h});

    simplecpp::TokenList out(files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(".");
    runPreprocess(out, rawtokens_c, files, cache, dui);

    ASSERT_EQUALS("\n#line 1 \"3.h\"\n123", out.stringify());
}

static void include6()   // #57 - incomplete macro  #include MACRO(,)
{
    const char code[] = "#define MACRO(X,Y) X##Y\n#include MACRO(,)\n";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens = makeTokenList(code, files, "57.c");

    ASSERT_EQUALS(1U, files.size());
    ASSERT_EQUALS("57.c", files[0]);

    simplecpp::FileDataCache cache;
    cache.insert({"57.c", rawtokens});

    simplecpp::TokenList out(files);
    runPreprocess(out, rawtokens, files, cache, simplecpp::DUI());
}


static void include7()    // #include MACRO
{
    const char code_c[] = "#define HDR  <3.h>\n"
                          "#include HDR\n";
    const char code_h[] = "123\n";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens_c = makeTokenList(code_c, files, "3.c");
    const simplecpp::TokenList rawtokens_h = makeTokenList(code_h, files, "3.h");

    ASSERT_EQUALS(2U, files.size());
    ASSERT_EQUALS("3.c", files[0]);
    ASSERT_EQUALS("3.h", files[1]);

    simplecpp::FileDataCache cache;
    cache.insert({"3.c", rawtokens_c});
    cache.insert({"3.h", rawtokens_h});

    simplecpp::TokenList out(files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(".");
    runPreprocess(out, rawtokens_c, files, cache, dui);

    ASSERT_EQUALS("\n#line 1 \"3.h\"\n123", out.stringify());
}

static void include8()    // #include MACRO(X)
{
    const char code[] = "#define INCLUDE_LOCATION ../somewhere\n"
                        "#define INCLUDE_FILE(F)  <INCLUDE_LOCATION/F.h>\n"
                        "#include INCLUDE_FILE(header)\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,3,missing_header,Header not found: <../somewhere/header.h>\n", toString(outputList));
}

static void include9()
{
    const char code_c[] = "#define HDR \"1.h\"\n"
                          "#include HDR\n";
    const char code_h[] = "/**/ #define X 1\n"  // <- comment before hash should be ignored
                          "x=X;";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens_c = makeTokenList(code_c, files, "1.c");
    const simplecpp::TokenList rawtokens_h = makeTokenList(code_h, files, "1.h");

    ASSERT_EQUALS(2U, files.size());
    ASSERT_EQUALS("1.c", files[0]);
    ASSERT_EQUALS("1.h", files[1]);

    simplecpp::FileDataCache cache;
    cache.insert({"1.c", rawtokens_c});
    cache.insert({"1.h", rawtokens_h});

    simplecpp::TokenList out(files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(".");
    dui.removeComments = true;
    runPreprocess(out, rawtokens_c, files, cache, dui);

    ASSERT_EQUALS("\n#line 2 \"1.h\"\nx = 1 ;", out.stringify());
}

static void include10()   // #669 - -include with load()
{
    const char code_c[] = "X\n";
    const char code_h[] = "#define X 123\n";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens_c = makeTokenList(code_c, files, "src.c");
    const simplecpp::TokenList rawtokens_h = makeTokenList(code_h, files, "inc.h");

    ASSERT_EQUALS(2U, files.size());
    ASSERT_EQUALS("src.c", files[0]);
    ASSERT_EQUALS("inc.h", files[1]);

    simplecpp::FileDataCache cache;
    cache.insert({"src.c", rawtokens_c});
    cache.insert({"inc.h", rawtokens_h});

    simplecpp::OutputList outputList;
    simplecpp::DUI dui;
    dui.includes.emplace_back("inc.h");
    dui.includes.emplace_back("missing.h");
    cache = simplecpp::load(rawtokens_c, files, dui, &outputList, std::move(cache));

    ASSERT_EQUALS(1, outputList.size());
    ASSERT_EQUALS("Can not open include file 'missing.h' that is explicitly included.", outputList.begin()->msg);
}

static void include11()   // #669 - -include with preprocess()
{
    const char code_c[] = "X\n";
    const char code_h[] = "#define X 123\n";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens_c = makeTokenList(code_c, files, "src.c");
    const simplecpp::TokenList rawtokens_h = makeTokenList(code_h, files, "inc.h");

    ASSERT_EQUALS(2U, files.size());
    ASSERT_EQUALS("src.c", files[0]);
    ASSERT_EQUALS("inc.h", files[1]);

    simplecpp::FileDataCache cache;
    cache.insert({"src.c", rawtokens_c});
    cache.insert({"inc.h", rawtokens_h});

    simplecpp::OutputList outputList;
    simplecpp::TokenList out(files);
    simplecpp::DUI dui;
    dui.includes.emplace_back("inc.h");
    dui.includes.emplace_back("missing.h");
    runPreprocess(out, rawtokens_c, files, cache, dui, &outputList);

    ASSERT_EQUALS("123", out.stringify());
    ASSERT_EQUALS(1, outputList.size());
    ASSERT_EQUALS("Can not open include file 'missing.h' that is explicitly included.", outputList.begin()->msg);
}

static void readfile_nullbyte()
{
    const char code[] = "ab\0cd\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("ab cd", readfile(code,sizeof(code)-1,{},&outputList));
    ASSERT_EQUALS(true, outputList.empty()); // should warning be written?
}

static void readfile_char()
{
    ASSERT_EQUALS("'c'", readfile("'c'"));
    ASSERT_EQUALS("L'c'", readfile("L'c'"));
    ASSERT_EQUALS("L 'c'", readfile("L 'c'"));
    ASSERT_EQUALS("A = 'c'", readfile("A = 'c'"));
    ASSERT_EQUALS("A = L'c'", readfile("A = L'c'"));
    ASSERT_EQUALS("A = u'c'", readfile("A = u'c'"));
    ASSERT_EQUALS("A = U'c'", readfile("A = U'c'"));
    ASSERT_EQUALS("A = u8'c'", readfile("A = u8'c'"));
    ASSERT_EQUALS("A = u8 'c'", readfile("A = u8 'c'"));

    // The character \'
    ASSERT_EQUALS("'\\''", readfile("'\\\''"));
    ASSERT_EQUALS("L'\\''", readfile("L'\\\''"));
    ASSERT_EQUALS("u'\\''", readfile("u'\\\''"));
    ASSERT_EQUALS("U'\\''", readfile("U'\\\''"));
    ASSERT_EQUALS("u8'\\''", readfile("u8'\\\''"));
}

static void readfile_char_error()
{
    simplecpp::OutputList outputList;

    readfile("A = L's", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,No pair for character (\'). Can't process file. File is either invalid or unicode, which is currently not supported.\n", toString(outputList));
    outputList.clear();

    readfile("A = 's\n'", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,No pair for character (\'). Can't process file. File is either invalid or unicode, which is currently not supported.\n", toString(outputList));
    outputList.clear();

    // the ' at the end of the file is not dropped after the peek for a digit separator
    readfile("A = 1'", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,No pair for character (\'). Can't process file. File is either invalid or unicode, which is currently not supported.\n", toString(outputList));
}

static void readfile_string()
{
    ASSERT_EQUALS("\"\"", readfile("\"\""));
    ASSERT_EQUALS("\"a\"", readfile("\"a\""));
    ASSERT_EQUALS("u\"a\"", readfile("u\"a\""));
    ASSERT_EQUALS("u \"a\"", readfile("u \"a\""));
    ASSERT_EQUALS("A = \"\"", readfile("A = \"\""));
    ASSERT_EQUALS("A = \"abc\'def\"", readfile("A = \"abc\'def\""));
    ASSERT_EQUALS("( \"\\\\\\\\\" )", readfile("(\"\\\\\\\\\")"));
    ASSERT_EQUALS("x = \"a  b\"\n;", readfile("x=\"a\\\n  b\";"));
    ASSERT_EQUALS("x = \"a  b\"\n;", readfile("x=\"a\\\r\n  b\";"));

    ASSERT_EQUALS("A = u8\"a\"", readfile("A = u8\"a\""));
    ASSERT_EQUALS("A = u\"a\"", readfile("A = u\"a\""));
    ASSERT_EQUALS("A = U\"a\"", readfile("A = U\"a\""));
    ASSERT_EQUALS("A = L\"a\"", readfile("A = L\"a\""));
    ASSERT_EQUALS("A = L \"a\"", readfile("A = L \"a\""));
    ASSERT_EQUALS("A = \"abc\\\\\\\\def\"", readfile("A = R\"(abc\\\\def)\""));
    ASSERT_EQUALS("A = \"abc\\\\\\\\def\"", readfile("A = R\"x(abc\\\\def)x\""));
    ASSERT_EQUALS("A = \"\"", readfile("A = R\"()\""));
    ASSERT_EQUALS("A = \"\\\\\"", readfile("A = R\"(\\)\""));
    ASSERT_EQUALS("A = \"\\\"\"", readfile("A = R\"(\")\""));
    ASSERT_EQUALS("A = \"abc\"", readfile("A = R\"\"\"(abc)\"\"\""));
    ASSERT_EQUALS("A = \"a\nb\nc\";", readfile("A = R\"foo(a\nb\nc)foo\";"));
    ASSERT_EQUALS("A = L\"abc\"", readfile("A = LR\"(abc)\""));
    ASSERT_EQUALS("A = u\"abc\"", readfile("A = uR\"(abc)\""));
    ASSERT_EQUALS("A = U\"abc\"", readfile("A = UR\"(abc)\""));
    ASSERT_EQUALS("A = u8\"abc\"", readfile("A = u8R\"(abc)\""));

    // Strings containing \"
    ASSERT_EQUALS("\"\\\"\"", readfile("\"\\\"\""));
    ASSERT_EQUALS("L\"\\\"\"", readfile("L\"\\\"\""));
    ASSERT_EQUALS("u\"\\\"\"", readfile("u\"\\\"\""));
    ASSERT_EQUALS("U\"\\\"\"", readfile("U\"\\\"\""));
    ASSERT_EQUALS("u8\"\\\"\"", readfile("u8\"\\\"\""));
    ASSERT_EQUALS("\"\\\"a\\\"\"", readfile("\"\\\"a\\\"\""));
    ASSERT_EQUALS("L\"a\\\"\\\"\"", readfile("L\"a\\\"\\\"\""));
    ASSERT_EQUALS("u\"a\\\"\\\"\"", readfile("u\"a\\\"\\\"\""));
    ASSERT_EQUALS("U\"a\\\"\\\"\"", readfile("U\"a\\\"\\\"\""));
    ASSERT_EQUALS("u8\"a\\\"\\\"\"", readfile("u8\"a\\\"\\\"\""));

    // Do not concatenate when prefix is not directly adjacent to "
    ASSERT_EQUALS("u8 \"a b\"", readfile("u8 \"a b\""));
    ASSERT_EQUALS("u8\n\"a b\"", readfile("u8\n  \"a b\""));
}

static void readfile_string_error()
{
    simplecpp::OutputList outputList;

    readfile("A = \"abs", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,No pair for character (\"). Can't process file. File is either invalid or unicode, which is currently not supported.\n", toString(outputList));
    outputList.clear();

    readfile("A = u8\"abs\n\"", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,No pair for character (\"). Can't process file. File is either invalid or unicode, which is currently not supported.\n", toString(outputList));
    outputList.clear();

    readfile("A = R\"as\n(abc)as\"", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,Invalid newline in raw string delimiter.\n", toString(outputList));
    outputList.clear();

    readfile("A = u8R\"as\n(abc)as\"", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,Invalid newline in raw string delimiter.\n", toString(outputList));
    outputList.clear();

    readfile("A = R\"as(abc)a\"", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,Raw string missing terminating delimiter.\n", toString(outputList));
    outputList.clear();

    readfile("A = LR\"as(abc)a\"", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,Raw string missing terminating delimiter.\n", toString(outputList));
    outputList.clear();

    readfile("#define A \"abs", {}, &outputList);
    ASSERT_EQUALS("file0,1,syntax_error,No pair for character (\"). Can't process file. File is either invalid or unicode, which is currently not supported.\n", toString(outputList));
    outputList.clear();

    // Don't warn for a multiline define
    readfile("#define A \"abs\\\n\"\n", {}, &outputList);
    ASSERT_EQUALS("", toString(outputList));
}

static void readfile_cpp14_number()
{
    ASSERT_EQUALS("A = 12345 ;", readfile("A = 12\'345;"));
}

static void readfile_unhandled_chars()
{
    simplecpp::OutputList outputList;
    readfile("// 你好世界\n", {}, &outputList);
    ASSERT_EQUALS("", toString(outputList));
    readfile("s=\"你好世界\"\n", {}, &outputList);
    ASSERT_EQUALS("", toString(outputList));
    readfile("int 你好世界=0;\n", {}, &outputList);
    ASSERT_EQUALS("file0,1,unhandled_char_error,The code contains unhandled character(s) (character code=228). Neither unicode nor extended ascii is supported.\n", toString(outputList));
}

static void readfile_error()
{
    ASSERT_EQUALS("# if ! A\n"
                  "# error\n"
                  "# endif\n"
                  "X",readfile("#if !A\n#error\n#endif\nX\n"));
}

static void readfile_file_not_found()
{
    simplecpp::OutputList outputList;
    std::vector<std::string> files;
    (void)simplecpp::TokenList("NotAFile", files, {}, &outputList);
    ASSERT_EQUALS("file0,0,file_not_found,File is missing: NotAFile\n", toString(outputList));
}

static void readfile_no_eof_newline()
{
    {
        const char code[] = "";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        const char code[] = "\n";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        const char code[] = "\\\n";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("file0,1,portability_no_eof_newline,No newline at end of file is undefined behavior in C.\n", toString(outputList));
    }
    {
        const char code[] = "\\\n";
        simplecpp::DUI dui;
        dui.std = "c++03";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, dui, &outputList);
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        const char code[] = "#define A";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("file0,1,portability_no_eof_newline,No newline at end of file is undefined behavior in C.\n", toString(outputList));
    }
    {
        const char code[] = "#define A\n";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        const char code[] = "#define A\\";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("file0,1,portability_no_eof_newline,No newline at end of file is undefined behavior in C.\n", toString(outputList));
    }
    {
        const char code[] = "// comment";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("file0,1,portability_no_eof_newline,No newline at end of file is undefined behavior in C.\n", toString(outputList));
    }
    {
        const char code[] = "// comment\n";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        const char code[] = "/* comment \n comment */";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("file0,2,portability_no_eof_newline,No newline at end of file is undefined behavior in C.\n", toString(outputList));
    }
    {
        const char code[] = "/* comment \n comment */\n";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        // CR is a newline
        const char code[] = "x\r";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        const char code[] = "x\r\n";
        simplecpp::OutputList outputList;
        readfile(code, sizeof(code)-1, {}, &outputList);
        ASSERT_EQUALS("", toString(outputList));
    }
}

static void readfile_cr_newlines()
{
    // CR, CR LF and LF are all newlines
    ASSERT_EQUALS("a\nb\nc", readfile("a\rb\rc\r"));
    ASSERT_EQUALS("a\nb\nc", readfile("a\r\nb\r\nc\r\n"));
    ASSERT_EQUALS("a\nb\nc\n\n\nd", readfile("a\rb\r\nc\n\r\rd\n"));
    ASSERT_EQUALS("# define A 1\nA", readfile("#define A 1\rA\r"));
}

static void readfile_filestream()
{
    // the edge cases from the other readfile tests, read from a file
    {
        const char code[] = "a\rb\rc\r";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("a\nb\nc", readfileFromFile(code, sizeof(code)-1, &outputList));
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        const char code[] = "a\r\nb\r\nc\r\n";
        ASSERT_EQUALS("a\nb\nc", readfileFromFile(code, sizeof(code)-1));
    }
    {
        const char code[] = "a\rb\r\nc\n\r\rd\n";
        ASSERT_EQUALS("a\nb\nc\n\n\nd", readfileFromFile(code, sizeof(code)-1));
    }
    {
        const char code[] = "1'";
        simplecpp::OutputList outputList;
        (void)readfileFromFile(code, sizeof(code)-1, &outputList);
        ASSERT_EQUALS("file0,1,syntax_error,No pair for character (\'). Can't process file. File is either invalid or unicode, which is currently not supported.\n", toString(outputList));
    }
    {
        // UTF-16LE with CR newlines
        const char code[] = "\xff\xfe\x78\x00\x0d\x00\x79\x00\x0d\x00";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("x\ny", readfileFromFile(code, sizeof(code)-1, &outputList));
        ASSERT_EQUALS("", toString(outputList));
    }
    {
        // UTF-16BE with CR newlines
        const char code[] = "\xfe\xff\x00\x78\x00\x0d\x00\x79\x00\x0d";
        ASSERT_EQUALS("x\ny", readfileFromFile(code, sizeof(code)-1));
    }
    {
        // UTF-16 with an odd number of bytes, the dangling last byte is dropped
        const char code[] = "\xff\xfe\x31\x00\x0a";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("1", readfileFromFile(code, sizeof(code)-1, &outputList));
        ASSERT_EQUALS("file0,1,portability_no_eof_newline,No newline at end of file is undefined behavior in C.\n", toString(outputList));
    }
}

static void stringify1()
{
    const char code_c[] = "#include \"A.h\"\n"
                          "#include \"A.h\"\n";
    const char code_h[] = "1\n2";

    std::vector<std::string> files;

    const simplecpp::TokenList rawtokens_c = makeTokenList(code_c, files, "A.c");
    const simplecpp::TokenList rawtokens_h = makeTokenList(code_h, files, "A.h");

    ASSERT_EQUALS(2U, files.size());
    ASSERT_EQUALS("A.c", files[0]);
    ASSERT_EQUALS("A.h", files[1]);

    simplecpp::FileDataCache cache;
    cache.insert({"A.c", rawtokens_c});
    cache.insert({"A.h", rawtokens_h});

    simplecpp::TokenList out(files);
    simplecpp::DUI dui;
    dui.includePaths.emplace_back(".");
    runPreprocess(out, rawtokens_c, files, cache, dui);

    ASSERT_EQUALS("\n#line 1 \"A.h\"\n1\n2\n#line 1 \"A.h\"\n1\n2", out.stringify());
}

// Token::macro, the locations of expanded tokens and the macro usage are used by cppcheck
static void tokenMacro1()
{
    const char code[] = "#define A 123\n"
                        "A";
    ASSERT_EQUALS("123@A(2:1)", expandedTokens(code));
}

static void tokenMacro2()
{
    const char code[] = "#define ADD(X,Y) X+Y\n"
                        "ADD(1,2)";
    ASSERT_EQUALS("1(2:1) +@ADD(2:1) 2(2:1)", expandedTokens(code));
}

static void tokenMacro3()
{
    const char code[] = "#define ADD(X,Y) X+Y\n"
                        "#define FRED  1\n"
                        "ADD(FRED,2)";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("1@FRED(3:1) +@ADD(3:1) 2(3:1)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("ADD 1:9 use 3:1\nFRED 2:9 use 3:1\n", toString(macroUsage));
}

static void tokenMacro4()
{
    HIDESET_EXPANSION_TODO(); // Token::macro and location rules
    const char code[] = "#define A B\n"
                        "#define B 1\n"
                        "A";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("1@A(3:1)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("A 1:9 use 3:1\nB 2:9 use 3:1\n", toString(macroUsage));
}

static void tokenMacro5()
{
    const char code[] = "#define SET_BPF(code) (code)\n"
                        "#define SET_BPF_JUMP(code) SET_BPF(D | code)\n"
                        "SET_BPF_JUMP(A | B | C);";
    ASSERT_EQUALS("(@SET_BPF(3:1) D@SET_BPF_JUMP(3:1) |@SET_BPF_JUMP(3:1) A(3:1) |(3:1) B(3:1) |(3:1) C(3:1) )@SET_BPF(3:1) ;(3:24)", expandedTokens(code));
}

static void tokenMacro6()
{
    const char code[] = "#define FILE() __FILE__\n"
                        "#define LINE() __LINE__\n"
                        "#define COUNTER() __COUNTER__\n"
                        "FILE()\n"
                        "LINE()\n"
                        "COUNTER()\n";
    ASSERT_EQUALS("\"\"@__FILE__(4:1) 5@__LINE__(5:1) 0@__COUNTER__(6:1)", expandedTokens(code));
}

static void tokenMacro7()
{
    const char code[] = "#define A 1+2\n"
                        "x A y\n";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("x(2:1) 1@A(2:3) +@A(2:3) 2@A(2:3) y(2:5)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("A 1:9 use 2:3\n", toString(macroUsage));
}

static void tokenMacro8()
{
    HIDESET_EXPANSION_TODO(); // Token::macro and location rules
    // the call g(2) is completed with tokens after the expansion of f.
    // The tokens get the location of the ( and the macro g
    const char code[] = "#define f g\n"
                        "#define g(x) x+1\n"
                        "f(2)\n";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("2(3:2) +@g(3:2) 1@g(3:2)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("f 1:9 use 3:1\ng 2:9 use 3:2\n", toString(macroUsage));
}

static void tokenMacro9()
{
    HIDESET_EXPANSION_TODO(); // Token::macro and location rules
    // the arguments of g( come after the expansion of f
    const char code[] = "#define g(x) x+1\n"
                        "#define f g(\n"
                        "f 2)\n";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("2(3:3) +@g(3:3) 1@g(3:3)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("f 2:9 use 3:1\ng 1:9 use 3:3\n", toString(macroUsage));
}

static void tokenMacro10()
{
    HIDESET_EXPANSION_TODO(); // Token::macro and location rules
    // the results of # and ## have no macro name
    const char code[] = "#define str(x) #x\n"
                        "#define cat(a,b) a##b\n"
                        "str(a b) cat(x,y)\n";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("\"a b\"(3:1) xy(3:10)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("cat 2:9 use 3:10\nstr 1:9 use 3:1\n", toString(macroUsage));
}

static void tokenMacro11()
{
    const char code[] = "#define F(...) f(__VA_ARGS__)\n"
                        "F(1,2)\n";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("f@F(2:1) (@F(2:1) 1(2:1) ,(2:1) 2(2:1) )@F(2:1)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("F 1:9 use 2:1\n", toString(macroUsage));
}

static void tokenMacro12()
{
    const char code[] = "#define f f\n"
                        "f\n";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("f@f(2:1)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("f 1:9 use 2:1\n", toString(macroUsage));
}

static void tokenMacro13()
{
    // all tokens of the expansion get the location of the macro name
    const char code[] = "#define ADD(X,Y) X+Y\n"
                        "ADD(1,\n"
                        "  2)\n"
                        "z\n";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("1(2:1) +@ADD(2:1) 2(2:1) z(4:1)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("ADD 1:9 use 2:1\n", toString(macroUsage));
}

static void tokenMacro14()
{
    // A is used in the argument and in the body, so it is listed twice
    const char code[] = "#define A 1\n"
                        "#define B(x) x A\n"
                        "B(A)\n";
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("1@A(3:1) 1@A(3:1)", expandedTokens(code, simplecpp::DUI(), &macroUsage));
    ASSERT_EQUALS("A 1:9 use 3:1\nA 1:9 use 3:1\nB 2:9 use 3:1\n", toString(macroUsage));
}

static void tokenMacro15()
{
    simplecpp::DUI dui;
    dui.defines.emplace_back("D=1+2");
    std::list<simplecpp::MacroUsage> macroUsage;
    ASSERT_EQUALS("x(1:1) 1@D(1:3) +@D(1:3) 2@D(1:3)", expandedTokens("x D\n", dui, &macroUsage));
    ASSERT_EQUALS("D 1:1 use 1:3 unknown\n", toString(macroUsage));
}

// Rescanning and hide sets. The results are the same as gcc gives.
static void hideSetExpansion_recursive()
{
    const char code[] = "#define f f\n"
                        "f\n";
    ASSERT_EQUALS("\n"
                  "f", preprocess(code));
}

static void hideSetExpansion_mutual()
{
    const char code[] = "#define a b\n"
                        "#define b a\n"
                        "a b\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "a b", preprocess(code));
}

static void hideSetExpansion_std_example()
{
    const char code[] = "#define f(a) a*g\n"
                        "#define g(a) f(a)\n"
                        "f(2)(9)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "2 * 9 * g", preprocess(code));
}

static void hideSetExpansion_trailing()
{
    const char code[] = "#define f g\n"
                        "#define g(x) x+1\n"
                        "f(2)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "2 + 1", preprocess(code));
}

static void hideSetExpansion_trailing_args()
{
    const char code[] = "#define g(x) x+1\n"
                        "#define f g(\n"
                        "f 2)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "2 + 1", preprocess(code));
}

static void hideSetExpansion_args_lines()
{
    const char code[] = "#define ADD(X,Y) X+Y\n"
                        "ADD(1,\n"
                        " 2)\n"
                        "z\n";
    ASSERT_EQUALS("\n"
                  "1 + 2\n"
                  "\n"
                  "z", preprocess(code));
}

static void hideSetExpansion_nested()
{
    const char code[] = "#define A(x) [x]\n"
                        "#define B(x) A(x) A(A(x))\n"
                        "B(1)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "[ 1 ] [ [ 1 ] ]", preprocess(code));
}

static void hideSetExpansion_arg_self()
{
    const char code[] = "#define f(x) x\n"
                        "f(f)(1)\n";
    // TODO: the old macro expansion is wrong
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n"
                                     "f ( 1 )",
                                     "\n"
                                     "1",
                                     preprocess(code));
}

static void hideSetExpansion_blue_in_arg()
{
    const char code[] = "#define f(x) g(x)\n"
                        "#define g(x) f(x)\n"
                        "f(f(1))\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "f ( f ( 1 ) )", preprocess(code));
}

static void hideSetExpansion_paren_in_arg()
{
    const char code[] = "#define f(x,y) (x)(y)\n"
                        "f((1,2),3)\n";
    ASSERT_EQUALS("\n"
                  "( ( 1 , 2 ) ) ( 3 )", preprocess(code));
}

static void hideSetExpansion_not_call()
{
    const char code[] = "#define f(x) x\n"
                        "f ; f\n"
                        "(1)\n";
    ASSERT_EQUALS("\n"
                  "f ; 1", preprocess(code));
}

static void hideSetExpansion_obj_to_fn()
{
    const char code[] = "#define LPAREN (\n"
                        "#define f(x) [x]\n"
                        "f LPAREN 1)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "f ( 1 )", preprocess(code));
}

static void hideSetExpansion_va_args()
{
    const char code[] = "#define F(...) f(__VA_ARGS__)\n"
                        "F(1,2) F()\n";
    ASSERT_EQUALS("\n"
                  "f ( 1 , 2 ) f ( )", preprocess(code));
}

static void hideSetExpansion_counter()
{
    const char code[] = "#define C __COUNTER__\n"
                        "C C __COUNTER__\n";
    ASSERT_EQUALS("\n"
                  "0 1 2", preprocess(code));
}

static void hideSetExpansion_line()
{
    const char code[] = "#define L __LINE__\n"
                        "L\n";
    ASSERT_EQUALS("\n"
                  "2", preprocess(code));
}

static void hideSetExpansion_deep()
{
    const char code[] = "#define a(x) b(x)\n"
                        "#define b(x) c(x)\n"
                        "#define c(x) x x\n"
                        "a(a(1))\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "\n"
                  "1 1 1 1", preprocess(code));
}

static void hideSetExpansion_q_example()
{
    const char code[] = "#define q(x) x\n"
                        "#define r(x,y) x y\n"
                        "q(r)(q,1)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "q 1", preprocess(code));
}

static void hideSetExpansion_empty()
{
    const char code[] = "#define E\n"
                        "#define f(x) <x>\n"
                        "f(E) f()\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "< > < >", preprocess(code));
}

// __VA_OPT__ (C23 6.10.5.1). The results are the same as gcc gives.
static void hideSetExpansion_va_opt_empty_after_expansion()
{
    const char code[] = "#define F(...) <__VA_OPT__(x)>\n"
                        "#define E\n"
                        "F() F(E) F(1)\n";
    // TODO: the old macro expansion is wrong
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n"
                                     "\n"
                                     "< > < > < x >",
                                     "\n"
                                     "\n"
                                     "< > < x > < x >",
                                     preprocess(code));
}

static void hideSetExpansion_va_opt_hash()
{
    const char code[] = "#define X 1\n"
                        "#define F(a,...) #__VA_OPT__(a b)\n"
                        "F(X,2) F(X)\n";
    // TODO: the old macro expansion is wrong
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n"
                                     "\n"
                                     "\"1 b\" \"\"",
                                     "\n"
                                     "\n"
                                     "\"X\" b \"\"",
                                     preprocess(code));
}

static void hideSetExpansion_va_opt_hashhash()
{
    const char code[] = "#define F(a,...) a ## __VA_OPT__(b c) ## d\n"
                        "F(x,1) F(x)\n";
    // TODO: the old macro expansion is wrong
    TODO_OLD_EXPANSION_ASSERT_EQUALS("\n"
                                     "xb cd xd",
                                     "\n"
                                     "xb cd x# \"d\"",
                                     preprocess(code));
}

static void hideSetExpansion_va_opt_hash_in_content()
{
    const char code[] = "#define F(a,...) __VA_OPT__(a##a) __VA_OPT__(#a)\n"
                        "F(x,1)\n";
    ASSERT_EQUALS("\n"
                  "xx \"x\"", preprocess(code));
}

static void hideSetExpansion_va_opt_rescan()
{
    const char code[] = "#define LP (\n"
                        "#define F(...) __VA_OPT__(LP)\n"
                        "F(1)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "(", preprocess(code));
}

static void hideSetExpansion_va_opt_comma()
{
    const char code[] = "#define F(a,...) f(a __VA_OPT__(,) __VA_ARGS__)\n"
                        "#define X 1\n"
                        "F(X) F(X,X)\n";
    ASSERT_EQUALS("\n"
                  "\n"
                  "f ( 1 ) f ( 1 , 1 )", preprocess(code));
}

static void hideSetExpansion_va_opt_parentheses()
{
    const char code[] = "#define F(...) __VA_OPT__((__VA_ARGS__))\n"
                        "F(a,(b)) F()\n";
    ASSERT_EQUALS("\n"
                  "( a , ( b ) )", preprocess(code));
}

static void undef()
{
    const char code[] = "#define A\n"
                        "#undef A\n"
                        "#ifdef A\n"
                        "123\n"
                        "#endif";
    ASSERT_EQUALS("", preprocess(code));
}

static void userdef()
{
    const char code[] = "#ifdef A\n123\n#endif\n";
    simplecpp::DUI dui;
    dui.defines.emplace_back("A=1");
    ASSERT_EQUALS("\n123", preprocess(code, dui));
}

static void utf8()
{
    ASSERT_EQUALS("123", readfile("\xEF\xBB\xBF 123"));
}

static void utf8_invalid()
{
    ASSERT_EQUALS("", readfile("\xEF 123"));
    ASSERT_EQUALS("", readfile("\xEF\xBB 123"));
}

static void unicode()
{
    {
        const char code[] = "\xFE\xFF\x00\x31\x00\x32";
        ASSERT_EQUALS("12", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFF\xFE\x31\x00\x32\x00";
        ASSERT_EQUALS("12", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFE\xFF\x00\x2f\x00\x2f\x00\x0a\x00\x31";
        ASSERT_EQUALS("//\n1", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFF\xFE\x2f\x00\x2f\x00\x0a\x00\x31\x00";
        ASSERT_EQUALS("//\n1", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFE\xFF\x00\x22\x00\x61\x00\x22";
        ASSERT_EQUALS("\"a\"", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFF\xFE\x22\x00\x61\x00\x22\x00";
        ASSERT_EQUALS("\"a\"", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xff\xfe\x0d\x00\x0a\x00\x2f\x00\x2f\x00\x31\x00\x0d\x00\x0a\x00";
        ASSERT_EQUALS("\n//1", readfile(code, sizeof(code)-1));
    }
    {
        // CR newlines
        const char code[] = "\xff\xfe\x78\x00\x0d\x00\x79\x00\x0d\x00";
        ASSERT_EQUALS("x\ny", readfile(code, sizeof(code)-1));
    }
    {
        // odd number of bytes, the dangling last byte is dropped
        const char code[] = "\xff\xfe\x31\x00\x0a";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("1", readfile(code, sizeof(code)-1, {}, &outputList));
        ASSERT_EQUALS("file0,1,portability_no_eof_newline,No newline at end of file is undefined behavior in C.\n", toString(outputList));
    }
}

static void unicode_invalid()
{
    {
        const char code[] = "\xFF";
        ASSERT_EQUALS("", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFE";
        ASSERT_EQUALS("", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFE\xFF\x31\x00";
        ASSERT_EQUALS("", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFF\xFE\x31\x00";
        ASSERT_EQUALS("1", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFE\xFF\x31\x32";
        ASSERT_EQUALS("", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFF\xFE\x31\x32";
        ASSERT_EQUALS("", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFE\xFF\x00\x31\x00\x32\x33\x00";
        ASSERT_EQUALS("", readfile(code, sizeof(code)-1));
    }
    {
        const char code[] = "\xFF\xFE\x31\x00\x32\x00\x33\x00";
        ASSERT_EQUALS("123", readfile(code, sizeof(code)-1));
    }
}

static void warning()
{
    const char code[] = "#warning MSG\n1\n";
    simplecpp::OutputList outputList;
    ASSERT_EQUALS("\n1", preprocess(code, &outputList));
    ASSERT_EQUALS("file0,1,#warning,#warning MSG\n", toString(outputList));
}

static void simplifyPath()
{
    ASSERT_EQUALS("1.c", simplecpp::simplifyPath("./1.c"));
    ASSERT_EQUALS("1.c", simplecpp::simplifyPath("././1.c"));
    ASSERT_EQUALS("/1.c", simplecpp::simplifyPath("/./1.c"));
    ASSERT_EQUALS("/1.c", simplecpp::simplifyPath("/././1.c"));
    ASSERT_EQUALS("trailing_dot./1.c", simplecpp::simplifyPath("trailing_dot./1.c"));

    ASSERT_EQUALS("1.c", simplecpp::simplifyPath("a/../1.c"));
    ASSERT_EQUALS("1.c", simplecpp::simplifyPath("a/b/../../1.c"));
    ASSERT_EQUALS("a/1.c", simplecpp::simplifyPath("a/b/../1.c"));
    ASSERT_EQUALS("/a/1.c", simplecpp::simplifyPath("/a/b/../1.c"));
    ASSERT_EQUALS("/a/1.c", simplecpp::simplifyPath("/a/b/c/../../1.c"));
    ASSERT_EQUALS("/a/1.c", simplecpp::simplifyPath("/a/b/c/../.././1.c"));

    ASSERT_EQUALS("../1.c", simplecpp::simplifyPath("../1.c"));
    ASSERT_EQUALS("../1.c", simplecpp::simplifyPath("../a/../1.c"));
    ASSERT_EQUALS("/../1.c", simplecpp::simplifyPath("/../1.c"));
    ASSERT_EQUALS("/../1.c", simplecpp::simplifyPath("/../a/../1.c"));

    ASSERT_EQUALS("a/..b/1.c", simplecpp::simplifyPath("a/..b/1.c"));
    ASSERT_EQUALS("../../1.c", simplecpp::simplifyPath("../../1.c"));
    ASSERT_EQUALS("../../../1.c", simplecpp::simplifyPath("../../../1.c"));
    ASSERT_EQUALS("../../../1.c", simplecpp::simplifyPath("../../../a/../1.c"));
    ASSERT_EQUALS("../../1.c", simplecpp::simplifyPath("a/../../../1.c"));
}

// tests transferred from cppcheck
// https://github.com/danmar/cppcheck/blob/d3e79b71b5ec6e641ca3e516cfced623b27988af/test/testpath.cpp#L43
static void simplifyPath_cppcheck()
{
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("./index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath(".//index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath(".///index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/index.h"));
    ASSERT_EQUALS("/path/", simplecpp::simplifyPath("/path/"));
    ASSERT_EQUALS("/", simplecpp::simplifyPath("/"));
    ASSERT_EQUALS("/", simplecpp::simplifyPath("/."));
    ASSERT_EQUALS("/", simplecpp::simplifyPath("/./"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/./index.h"));
    ASSERT_EQUALS("/", simplecpp::simplifyPath("/.//"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/.//index.h"));
    ASSERT_EQUALS("../index.h", simplecpp::simplifyPath("../index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/path/../index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("./path/../index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("path/../index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/path//../index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("./path//../index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("path//../index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/path/..//index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("./path/..//index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("path/..//index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/path//..//index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("./path//..//index.h"));
    ASSERT_EQUALS("index.h", simplecpp::simplifyPath("path//..//index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/path/../other/../index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/path/../other///././../index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/path/../other/././..///index.h"));
    ASSERT_EQUALS("/index.h", simplecpp::simplifyPath("/path/../other///././..///index.h"));
    ASSERT_EQUALS("../path/index.h", simplecpp::simplifyPath("../path/other/../index.h"));
    ASSERT_EQUALS("a/index.h", simplecpp::simplifyPath("a/../a/index.h"));
    ASSERT_EQUALS(".", simplecpp::simplifyPath("a/.."));
    ASSERT_EQUALS(".", simplecpp::simplifyPath("./a/.."));
    ASSERT_EQUALS("../../src/test.cpp", simplecpp::simplifyPath("../../src/test.cpp"));
    ASSERT_EQUALS("../../../src/test.cpp", simplecpp::simplifyPath("../../../src/test.cpp"));
    ASSERT_EQUALS("src/test.cpp", simplecpp::simplifyPath(".//src/test.cpp"));
    ASSERT_EQUALS("src/test.cpp", simplecpp::simplifyPath(".///src/test.cpp"));
    ASSERT_EQUALS("test.cpp", simplecpp::simplifyPath("./././././test.cpp"));
    ASSERT_EQUALS("src/", simplecpp::simplifyPath("src/abc/.."));
    ASSERT_EQUALS("src/", simplecpp::simplifyPath("src/abc/../"));

    // Handling of UNC paths on Windows
    ASSERT_EQUALS("//" STRINGIZE(UNCHOST) "/test.cpp", simplecpp::simplifyPath("//" STRINGIZE(UNCHOST) "/test.cpp"));
    ASSERT_EQUALS("//" STRINGIZE(UNCHOST) "/test.cpp", simplecpp::simplifyPath("///" STRINGIZE(UNCHOST) "/test.cpp"));
}

static void simplifyPath_New()
{
    ASSERT_EQUALS("", simplecpp::simplifyPath(""));
    ASSERT_EQUALS("/", simplecpp::simplifyPath("/"));
    ASSERT_EQUALS("//", simplecpp::simplifyPath("//"));
    ASSERT_EQUALS("//", simplecpp::simplifyPath("///"));
    ASSERT_EQUALS("/", simplecpp::simplifyPath("\\"));
}

static void preprocessSizeOf()
{
    simplecpp::OutputList outputList;

    ASSERT_EQUALS("", preprocess("#if 3 > sizeof\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, missing sizeof argument\n", toString(outputList));

    outputList.clear();

    ASSERT_EQUALS("", preprocess("#if 3 > sizeof A\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, missing sizeof argument\n", toString(outputList));

    outputList.clear();

    ASSERT_EQUALS("", preprocess("#if 3 > sizeof(int\n", &outputList));
    ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition, invalid sizeof expression\n", toString(outputList));
}

static void timeDefine()
{
    const char code[] = "__TIME__";
    const std::string t = preprocess(code);
    // "19:09:53"
    ASSERT_EQUALS(10, t.size());
    // TODO: split string and check proper ranges instead
    ASSERT_EQUALS('"', t[0]);
    ASSERT_EQUALS(true, isdigit(t[1]) != 0);
    ASSERT_EQUALS(true, isdigit(t[2]) != 0);
    ASSERT_EQUALS(':', t[3]);
    ASSERT_EQUALS(true, isdigit(t[4]) != 0);
    ASSERT_EQUALS(true, isdigit(t[5]) != 0);
    ASSERT_EQUALS(':', t[6]);
    ASSERT_EQUALS(true, isdigit(t[7]) != 0);
    ASSERT_EQUALS(true, isdigit(t[8]) != 0);
    ASSERT_EQUALS('"', t[9]);
}

static void dateDefine()
{
    const char code[] = "__DATE__";
    const std::string dt = preprocess(code);
    // "\"Mar 11 2022\""
    ASSERT_EQUALS(13, dt.size());
    // TODO: split string and check proper ranges instead
    ASSERT_EQUALS('"', dt[0]);
    ASSERT_EQUALS(true, dt[1] >= 'A' && dt[1] <= 'Z'); // uppercase letter
    ASSERT_EQUALS(true, dt[2] >= 'a' && dt[2] <= 'z'); // lowercase letter
    ASSERT_EQUALS(true, dt[3] >= 'a' && dt[3] <= 'z'); // lowercase letter
    ASSERT_EQUALS(' ', dt[4]);
    ASSERT_EQUALS(true, isdigit(dt[5]) != 0);
    ASSERT_EQUALS(true, isdigit(dt[6]) != 0);
    ASSERT_EQUALS(' ', dt[7]);
    ASSERT_EQUALS(true, isdigit(dt[8]) != 0);
    ASSERT_EQUALS(true, isdigit(dt[9]) != 0);
    ASSERT_EQUALS(true, isdigit(dt[10]) != 0);
    ASSERT_EQUALS(true, isdigit(dt[11]) != 0);
    ASSERT_EQUALS('"', dt[12]);
}

static void stdcVersionDefine()
{
    const char code[] = "#if defined(__STDC_VERSION__)\n"
                        "  __STDC_VERSION__\n"
                        "#endif\n";
    simplecpp::DUI dui;
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "c11";
    ASSERT_EQUALS("\n201112L", preprocess(code, dui));
}

static void cpluscplusDefine()
{
    const char code[] = "#if defined(__cplusplus)\n"
                        "  __cplusplus\n"
                        "#endif\n";
    simplecpp::DUI dui;
    ASSERT_EQUALS("", preprocess(code, dui));
    dui.std = "c++11";
    ASSERT_EQUALS("\n201103L", preprocess(code, dui));
}

static void invalidStd()
{
    const char code[] = "";
    simplecpp::DUI dui;
    simplecpp::OutputList outputList;

    dui.std = "c88";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(1, outputList.size());
    ASSERT_EQUALS(simplecpp::Output::Type::DUI_ERROR, outputList.cbegin()->type);
    ASSERT_EQUALS("unknown standard specified: 'c88'", outputList.cbegin()->msg);
    outputList.clear();

    dui.std = "gnu88";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(1, outputList.size());
    ASSERT_EQUALS(simplecpp::Output::Type::DUI_ERROR, outputList.cbegin()->type);
    ASSERT_EQUALS("unknown standard specified: 'gnu88'", outputList.cbegin()->msg);
    outputList.clear();

    dui.std = "d99";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(1, outputList.size());
    ASSERT_EQUALS(simplecpp::Output::Type::DUI_ERROR, outputList.cbegin()->type);
    ASSERT_EQUALS("unknown standard specified: 'd99'", outputList.cbegin()->msg);
    outputList.clear();

    dui.std = "c++77";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(1, outputList.size());
    ASSERT_EQUALS(simplecpp::Output::Type::DUI_ERROR, outputList.cbegin()->type);
    ASSERT_EQUALS("unknown standard specified: 'c++77'", outputList.cbegin()->msg);
    outputList.clear();

    dui.std = "gnu++33";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(1, outputList.size());
    ASSERT_EQUALS(simplecpp::Output::Type::DUI_ERROR, outputList.cbegin()->type);
    ASSERT_EQUALS("unknown standard specified: 'gnu++33'", outputList.cbegin()->msg);
    outputList.clear();
}

static void stdEnum()
{
    ASSERT_EQUALS(simplecpp::cstd_t::C89, simplecpp::getCStd("c89"));
    ASSERT_EQUALS(simplecpp::cstd_t::C89, simplecpp::getCStd("c90"));
    ASSERT_EQUALS(simplecpp::cstd_t::C11, simplecpp::getCStd("iso9899:2011"));
    ASSERT_EQUALS(simplecpp::cstd_t::C23, simplecpp::getCStd("gnu23"));
    ASSERT_EQUALS(simplecpp::cstd_t::CUnknown, simplecpp::getCStd("gnu77"));
    ASSERT_EQUALS(simplecpp::cstd_t::CUnknown, simplecpp::getCStd("c++11"));

    ASSERT_EQUALS(simplecpp::cppstd_t::CPP03, simplecpp::getCppStd("c++03"));
    ASSERT_EQUALS(simplecpp::cppstd_t::CPP03, simplecpp::getCppStd("c++98"));
    ASSERT_EQUALS(simplecpp::cppstd_t::CPP17, simplecpp::getCppStd("c++1z"));
    ASSERT_EQUALS(simplecpp::cppstd_t::CPP26, simplecpp::getCppStd("gnu++26"));
    ASSERT_EQUALS(simplecpp::cppstd_t::CPPUnknown, simplecpp::getCppStd("gnu++77"));
    ASSERT_EQUALS(simplecpp::cppstd_t::CPPUnknown, simplecpp::getCppStd("c11"));
}

static void stdValid()
{
    const char code[] = "";
    simplecpp::DUI dui;
    simplecpp::OutputList outputList;

    dui.std = "c89";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(0, outputList.size());
    outputList.clear();

    dui.std = "gnu23";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(0, outputList.size());
    outputList.clear();

    dui.std = "c++03";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(0, outputList.size());
    outputList.clear();

    dui.std = "gnu++26";
    ASSERT_EQUALS("", preprocess(code, dui, &outputList));
    ASSERT_EQUALS(0, outputList.size());
    outputList.clear();
}

static void assertToken(const std::string& s, bool name, bool number, bool comment, char op, int line)
{
    const std::vector<std::string> f;
    const simplecpp::Location l;
    const simplecpp::Token t(s, l);
    assertEquals(name, t.name, line);
    assertEquals(number, t.number, line);
    assertEquals(comment, t.comment, line);
    assertEquals(op, t.op, line);
}

#define ASSERT_TOKEN(s, na, nu, c) assertToken(s, na, nu, c, '\0', __LINE__)
#define ASSERT_TOKEN_OP(s, na, nu, c, o) assertToken(s, na, nu, c, o, __LINE__)

static void token()
{
    // name
    ASSERT_TOKEN("n", true, false, false);
    ASSERT_TOKEN("name", true, false, false);
    ASSERT_TOKEN("name_1", true, false, false);
    ASSERT_TOKEN("name2", true, false, false);
    ASSERT_TOKEN("name$", true, false, false);

    // character literal
    ASSERT_TOKEN("'n'", false, false, false);
    ASSERT_TOKEN("'\\''", false, false, false);
    ASSERT_TOKEN("'\\u0012'", false, false, false);
    ASSERT_TOKEN("'\\xff'", false, false, false);
    ASSERT_TOKEN("u8'\\u0012'", false, false, false);
    ASSERT_TOKEN("u'\\u0012'", false, false, false);
    ASSERT_TOKEN("L'\\u0012'", false, false, false);
    ASSERT_TOKEN("U'\\u0012'", false, false, false);

    // include
    ASSERT_TOKEN("<include>", false, false, false);

    // comment
    ASSERT_TOKEN("/*comment*/", false, false, true);
    ASSERT_TOKEN("// TODO", false, false, true);

    // string literal
    ASSERT_TOKEN("\"literal\"", false, false, false);

    // op
    ASSERT_TOKEN_OP("<", false, false, false, '<');
    ASSERT_TOKEN_OP(">", false, false, false, '>');
    ASSERT_TOKEN_OP("(", false, false, false, '(');
    ASSERT_TOKEN_OP(")", false, false, false, ')');

    // number
    ASSERT_TOKEN("2", false, true, false);
    ASSERT_TOKEN("22", false, true, false);
    ASSERT_TOKEN("-2", false, true, false);
    ASSERT_TOKEN("-22", false, true, false);
    ASSERT_TOKEN("+2", false, true, false);
    ASSERT_TOKEN("+22", false, true, false);
}

static void preprocess_files()
{
    {
        const char code[] = "#define A";
        std::vector<std::string> files;

        const simplecpp::TokenList tokens = makeTokenList(code, files);
        ASSERT_EQUALS(1, files.size());
        ASSERT_EQUALS("", *files.cbegin());

        simplecpp::TokenList tokens2(files);
        ASSERT_EQUALS(1, files.size());
        ASSERT_EQUALS("", *files.cbegin());

        simplecpp::FileDataCache cache;
        runPreprocess(tokens2, tokens, files, cache, simplecpp::DUI(), nullptr);
        ASSERT_EQUALS(1, files.size());
        ASSERT_EQUALS("", *files.cbegin());
    }
    {
        const char code[] = "#define A";
        std::vector<std::string> files;

        const simplecpp::TokenList tokens = makeTokenList(code, files, "test.cpp");
        ASSERT_EQUALS(1, files.size());
        ASSERT_EQUALS("test.cpp", *files.cbegin());

        simplecpp::TokenList tokens2(files);
        ASSERT_EQUALS(1, files.size());
        ASSERT_EQUALS("test.cpp", *files.cbegin());

        simplecpp::FileDataCache cache;
        runPreprocess(tokens2, tokens, files, cache, simplecpp::DUI(), nullptr);
        ASSERT_EQUALS(1, files.size());
        ASSERT_EQUALS("test.cpp", *files.cbegin());
    }
}

static void tokenlist_api()
{
    std::vector<std::string> filenames;
#  if !defined(__cpp_lib_string_view) && !defined(__cpp_lib_span)
    // sized array + size
    {
        char input[] = "code"; // NOLINT(misc-const-correctness)
        simplecpp::TokenList(input,sizeof(input)-1,filenames,"");
    }
    {
        const char input[] = "code";
        simplecpp::TokenList(input,sizeof(input)-1,filenames,"");
    }
    {
        unsigned char input[] = "code"; // NOLINT(misc-const-correctness)
        simplecpp::TokenList(input,sizeof(input)-1,filenames,"");
    }
    {
        const unsigned char input[] = "code";
        simplecpp::TokenList(input,sizeof(input)-1,filenames,"");
    }
#endif // !defined(__cpp_lib_string_view) && !defined(__cpp_lib_span)
    // pointer via View
    {
        const char * const input = "code";
        simplecpp::TokenList({input},filenames,"");
    }
    // sized array via View
    {
        char input[] = "code"; // NOLINT(misc-const-correctness)
        simplecpp::TokenList(simplecpp::View{input},filenames,"");
    }
    {
        const char input[] = "code";
        simplecpp::TokenList(simplecpp::View{input},filenames,"");
    }
    // sized array + size via View/std::span
    {
        char input[] = "code"; // NOLINT(misc-const-correctness)
        simplecpp::TokenList({input,sizeof(input)-1},filenames,"");
    }
    {
        const char input[] = "code";
        simplecpp::TokenList({input,sizeof(input)-1},filenames,"");
    }
    // sized array
    {
        char input[] = "code"; // NOLINT(misc-const-correctness)
        simplecpp::TokenList(input,filenames,"");
    }
    {
        const char input[] = "code";
        simplecpp::TokenList(input,filenames,"");
    }
    {
        unsigned char input[] = "code"; // NOLINT(misc-const-correctness)
        simplecpp::TokenList(input,filenames,"");
    }
    {
        const unsigned char input[] = "code";
        simplecpp::TokenList(input,filenames,"");
    }
    // std::string via View/std::span (implicit)
    {
        std::string input = "code"; // NOLINT(misc-const-correctness)
        simplecpp::TokenList(input,filenames,"");
    }
    {
        const std::string input = "code";
        simplecpp::TokenList(input,filenames,"");
    }
    // std::string via View/std::span (explicit)
    {
        std::string input = "code"; // NOLINT(misc-const-correctness)
        simplecpp::TokenList({input},filenames,"");
    }
    {
        const std::string input = "code";
        simplecpp::TokenList({input},filenames,"");
    }

    // this test is to make sure the safe APIs are compiling
#ifdef __cpp_lib_string_view
    {
        const char input[] = "code";
        const std::string_view sv = input;
        // std::string_view can be implicitly converted into a std::span
        simplecpp::TokenList(sv,filenames,"");
    }
#endif // __cpp_lib_string_view
#ifdef __cpp_lib_span
    {
        char input[] = "code";
        const std::span sp = input;
        simplecpp::TokenList(sp,filenames,"");
    }
    {
        const char input[] = "code";
        const std::span sp = input;
        simplecpp::TokenList(sp,filenames,"");
    }
    {
        unsigned char input[] = "code";
        const std::span sp = input;
        simplecpp::TokenList(sp,filenames,"");
    }
    {
        const unsigned char input[] = "code";
        const std::span sp = input;
        simplecpp::TokenList(sp,filenames,"");
    }
#endif // __cpp_lib_span
}

static void bad_macro_syntax() // #616
{
    simplecpp::DUI dui;
    dui.defines.emplace_back("\"");

    simplecpp::OutputList outputList;
    ASSERT_EQUALS("", preprocess("", dui, &outputList));
    ASSERT_EQUALS(1, outputList.size());
    ASSERT_EQUALS(simplecpp::Output::Type::DUI_ERROR, outputList.cbegin()->type);
    ASSERT_EQUALS("bad macro syntax. macroname=\" value=1", outputList.cbegin()->msg);
}

static void ifCond()
{
    {
        const char code[] = "int i;";
        std::list<simplecpp::IfCond> ifCond;
        ASSERT_EQUALS("int i ;", preprocess(code, &ifCond));
        ASSERT_EQUALS(0, ifCond.size());
    }
    {
        const char code[] = "#if 0\n"
                            "# elif __GNUC__ == 1\n"
                            "#  elif defined(__APPLE__)\n"
                            "#endif\n";
        std::list<simplecpp::IfCond> ifCond;
        ASSERT_EQUALS("", preprocess(code, &ifCond));
        ASSERT_EQUALS(3, ifCond.size());
        auto it = ifCond.cbegin();
        ASSERT_EQUALS(0, it->location.fileIndex);
        ASSERT_EQUALS(1, it->location.line);
        ASSERT_EQUALS(2, it->location.col);
        ASSERT_EQUALS("0", it->E);
        ASSERT_EQUALS(0, it->result);
        ++it;
        ASSERT_EQUALS(0, it->location.fileIndex);
        ASSERT_EQUALS(2, it->location.line);
        ASSERT_EQUALS(3, it->location.col);
        ASSERT_EQUALS("__GNUC__ == 1", it->E);
        ASSERT_EQUALS(0, it->result);
        ++it;
        ASSERT_EQUALS(0, it->location.fileIndex);
        ASSERT_EQUALS(3, it->location.line);
        ASSERT_EQUALS(4, it->location.col);
        ASSERT_EQUALS("0", it->E);
        ASSERT_EQUALS(0, it->result);
    }
    {
        const char code[] = "#ifdef NOTDEFINED\n"
                            "#endif\n"
                            "#ifndef NOTDEFINED\n"
                            "#endif\n";
        std::list<simplecpp::IfCond> ifCond;
        ASSERT_EQUALS("", preprocess(code, &ifCond));
        ASSERT_EQUALS(2, ifCond.size());
        auto it = ifCond.cbegin();
        ASSERT_EQUALS(0, it->location.fileIndex);
        ASSERT_EQUALS(1, it->location.line);
        ASSERT_EQUALS(2, it->location.col);
        ASSERT_EQUALS("0", it->E);
        ASSERT_EQUALS(0, it->result);
        ++it;
        ASSERT_EQUALS(0, it->location.fileIndex);
        ASSERT_EQUALS(3, it->location.line);
        ASSERT_EQUALS(2, it->location.col);
        ASSERT_EQUALS("1", it->E);
        ASSERT_EQUALS(1, it->result);
    }
}

static void macroUsage()
{
    {
        const char code[] = "int i;";
        std::list<simplecpp::MacroUsage> macroUsage;
        ASSERT_EQUALS("int i ;", preprocess(code, &macroUsage));
        ASSERT_EQUALS(0, macroUsage.size());
    }
    {
        const char code[] = "#define DEF_1\n"
                            "#ifdef DEF_1\n"
                            "#endif\n";
        std::list<simplecpp::MacroUsage> macroUsage;
        ASSERT_EQUALS("", preprocess(code, &macroUsage));
        ASSERT_EQUALS(1, macroUsage.size());
        auto it = macroUsage.cbegin();
        ASSERT_EQUALS("DEF_1", it->macroName);
        ASSERT_EQUALS(0, it->macroLocation.fileIndex);
        ASSERT_EQUALS(1, it->macroLocation.line);
        ASSERT_EQUALS(9, it->macroLocation.col);
        ASSERT_EQUALS(true, it->macroValueKnown);
        ASSERT_EQUALS(0, it->useLocation.fileIndex);
        ASSERT_EQUALS(2, it->useLocation.line);
        ASSERT_EQUALS(8, it->useLocation.col);
    }
    {
        const char code[] = "#define X 1\n"
                            "#if defined(X)\n"
                            "#endif\n";
        std::list<simplecpp::MacroUsage> macroUsage;
        ASSERT_EQUALS("", preprocess(code, &macroUsage));
        ASSERT_EQUALS(1, macroUsage.size());
        auto it = macroUsage.cbegin();
        ASSERT_EQUALS("X", it->macroName);
        ASSERT_EQUALS(2, it->useLocation.line);
        ASSERT_EQUALS(13, it->useLocation.col);
    }
    {
        const char code[] = "#define X 1\n"
                            "#if X\n"
                            "#endif\n";
        std::list<simplecpp::MacroUsage> macroUsage;
        ASSERT_EQUALS("", preprocess(code, &macroUsage));
        ASSERT_EQUALS(1, macroUsage.size());
        auto it = macroUsage.cbegin();
        ASSERT_EQUALS("X", it->macroName);
        ASSERT_EQUALS(2, it->useLocation.line);
        ASSERT_EQUALS(5, it->useLocation.col);
    }
}

static void isAbsolutePath() {
#ifdef _WIN32
    ASSERT_EQUALS(true, simplecpp::isAbsolutePath("C:\\foo\\bar"));
    ASSERT_EQUALS(true, simplecpp::isAbsolutePath("C:/foo/bar"));
    ASSERT_EQUALS(true, simplecpp::isAbsolutePath("\\\\foo\\bar"));

    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("foo\\bar"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("foo/bar"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("foo.cpp"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("C:foo.cpp"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("C:foo\\bar.cpp"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("bar.cpp"));
    //ASSERT_EQUALS(true, simplecpp::isAbsolutePath("\\")); // TODO
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("0:\\foo\\bar"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("0:/foo/bar"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("\\foo\\bar"));
    //ASSERT_EQUALS(false, simplecpp::isAbsolutePath("\\\\")); // TODO
    //ASSERT_EQUALS(false, simplecpp::isAbsolutePath("//")); // TODO
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("/foo/bar"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("/"));
#else
    ASSERT_EQUALS(true, simplecpp::isAbsolutePath("/foo/bar"));
    ASSERT_EQUALS(true, simplecpp::isAbsolutePath("/"));
    ASSERT_EQUALS(true, simplecpp::isAbsolutePath("//host/foo/bar"));

    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("foo/bar"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("foo.cpp"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("C:\\foo\\bar"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("C:/foo/bar"));
    ASSERT_EQUALS(false, simplecpp::isAbsolutePath("\\\\foo\\bar"));
#endif
}

// crashes detected by fuzzer
static void fuzz_crash()
{
    {
        const char code[] = "#define n __VA_OPT__(u\n"
                            "n\n";
        (void)preprocess(code, simplecpp::DUI()); // do not crash
    }
    { // #346
        const char code[] = "#define foo(intp)f##oo(intp\n"
                            "foo(f##oo(intp))\n";
        (void)preprocess(code, simplecpp::DUI()); // do not crash
    }
    { // #546
        const char code[] = "#if __has_include<\n";
        simplecpp::OutputList outputList;
        ASSERT_EQUALS("", preprocess(code, &outputList)); // do not crash
        ASSERT_EQUALS("file0,1,syntax_error,failed to evaluate #if condition\n", toString(outputList));
    }
}

// memory leaks detected by LSAN/valgrind
static void leak()
{
    { // #498
        const char code[] = "#define e(...)__VA_OPT__()\n"
                            "#define e\n";
        (void)preprocess(code, simplecpp::DUI());
    }
}

static void runTests(int argc, char **argv, Input input)
{
    USE_INPUT = input;

    TEST_CASE(backslash);

    TEST_CASE(builtin);

    TEST_CASE(characterLiteral);

    TEST_CASE(combineOperators_floatliteral);
    TEST_CASE(combineOperators_increment);
    TEST_CASE(combineOperators_coloncolon);
    TEST_CASE(combineOperators_andequal);
    TEST_CASE(combineOperators_ellipsis);

    TEST_CASE(comment);
    TEST_CASE(comment_multiline);
    TEST_CASE(keep_comments);

    TEST_CASE(constFold);

#ifdef __CYGWIN__
    TEST_CASE(convertCygwinPath);
#endif

    TEST_CASE(define1);
    TEST_CASE(define2);
    TEST_CASE(define3);
    TEST_CASE(define4);
    TEST_CASE(define5);
    TEST_CASE(define6);
    TEST_CASE(define7);
    TEST_CASE(define8);
    TEST_CASE(macro_c11_example3);
    TEST_CASE(macro_c11_example4);
    TEST_CASE(macro_c11_example5);
    TEST_CASE(macro_c11_example7);
    TEST_CASE(macro_c23_va_opt_examples);
    TEST_CASE(define9);
    TEST_CASE(define10);
    TEST_CASE(define11);
    TEST_CASE(define12);
    TEST_CASE(define13);
    TEST_CASE(define14); // #296
    TEST_CASE(define15); // #231
    TEST_CASE(define16); // #201
    TEST_CASE(define17); // #185
    TEST_CASE(define18); // #130
    TEST_CASE(define19); // #124
    TEST_CASE(define20); // #113
    TEST_CASE(define21); // #66
    TEST_CASE(define22); // #40
    TEST_CASE(define23); // #40
    TEST_CASE(define24);
    TEST_CASE(define25);
    TEST_CASE(define26);
    TEST_CASE(define27);
    TEST_CASE(define_invalid_1);
    TEST_CASE(define_invalid_2);
    TEST_CASE(define_invalid_3);
    TEST_CASE(define_invalid_4);
    TEST_CASE(define_wrong_number_of_arguments);
    TEST_CASE(define_define_1);
    TEST_CASE(define_define_2);
    TEST_CASE(define_define_3);
    TEST_CASE(define_define_4);
    TEST_CASE(define_define_5);
    TEST_CASE(define_define_6);
    TEST_CASE(define_define_7);
    TEST_CASE(define_define_8); // line break in nested macro call
    TEST_CASE(define_define_9); // line break in nested macro call
    TEST_CASE(define_define_10);
    TEST_CASE(define_define_11);
    TEST_CASE(define_define_11a);
    TEST_CASE(define_define_12); // expand result of ##
    TEST_CASE(define_define_13);
    TEST_CASE(define_define_14);
    TEST_CASE(define_define_15);
    TEST_CASE(define_define_16);
    TEST_CASE(define_define_17);
    TEST_CASE(define_define_18);
    TEST_CASE(define_define_19);
    TEST_CASE(define_define_20); // 384 arg contains comma
    TEST_CASE(define_define_21);
    TEST_CASE(define_define_22); // #400
    TEST_CASE(define_define_23); // #403 - crash, infinite recursion
    TEST_CASE(define_define_24); // #590
    TEST_CASE(define_va_args_1);
    TEST_CASE(define_va_args_2);
    TEST_CASE(define_va_args_3);
    TEST_CASE(define_va_args_4);
    TEST_CASE(define_va_opt_1);
    TEST_CASE(define_va_opt_2);
    TEST_CASE(define_va_opt_3);
    TEST_CASE(define_va_opt_4);
    TEST_CASE(define_va_opt_5);
    TEST_CASE(define_va_opt_6);
    TEST_CASE(define_va_opt_7);
    TEST_CASE(define_va_opt_8);
    TEST_CASE(define_va_opt_9); // #632

    TEST_CASE(pragma_backslash); // multiline pragma directive
    TEST_CASE(pragma_backslash_2); // #217

    // UB: #ifdef as macro parameter
    TEST_CASE(define_ifdef);

    TEST_CASE(if_invalid_1);
    TEST_CASE(if_invalid_2);
    TEST_CASE(if_invalid_3);

    TEST_CASE(dollar);

    TEST_CASE(error1);
    TEST_CASE(error2);
    TEST_CASE(error3);
    TEST_CASE(error4);
    TEST_CASE(error5);
    TEST_CASE(error6);
    TEST_CASE(error7);
    TEST_CASE(error8);

    TEST_CASE(garbage);
    TEST_CASE(garbage_endif);

    TEST_CASE(hash);
    TEST_CASE(hashEscape);
    TEST_CASE(hashComment);
    TEST_CASE(hashMacroTrailingWhitespace);
    TEST_CASE(hashCombinedOperator);
    TEST_CASE(hashEscapeTrailingBackslash);
    TEST_CASE(hashhash1);
    TEST_CASE(hashhash2);
    TEST_CASE(hashhash3);
    TEST_CASE(hashhash4);
    TEST_CASE(hashhash4a); // #66, #130
    TEST_CASE(hashhash5);
    TEST_CASE(hashhash6);
    TEST_CASE(hashhash7); // # ## #  (C standard; 6.10.3.3.p4)
    TEST_CASE(hashhash8);
    TEST_CASE(hashhash9);
    TEST_CASE(hashhash10); // #108 : #define x # #
    TEST_CASE(hashhash11); // #60: #define x # # #
    TEST_CASE(hashFromMacroAtLineStart);
    TEST_CASE(hashhash12);
    TEST_CASE(hashhash13);
    TEST_CASE(hashhash_string_literal);
    TEST_CASE(hashhash_string_wrapped);
    TEST_CASE(hashhash_char_literal);
    TEST_CASE(hashhash_multichar_literal);
    TEST_CASE(hashhash_char_escaped);
    TEST_CASE(hashhash_string_nothing);
    TEST_CASE(hashhash_string_char);
    TEST_CASE(hashhash_string_name);
    TEST_CASE(hashhashhash_int_literal);
    TEST_CASE(hashhash_int_literal);
    TEST_CASE(hashhash_invalid_1);
    TEST_CASE(hashhash_invalid_2);
    TEST_CASE(hashhash_invalid_string_number);
    TEST_CASE(hashhash_invalid_missing_args);
    TEST_CASE(hashhash_null_stmt);
    TEST_CASE(hashhash_empty_va_args);
    TEST_CASE(hashhash_punctuator);
    TEST_CASE(hashhash_punctuator_invalid);
    TEST_CASE(hashhash_empty_placemarker);
    TEST_CASE(hashhash_invalid_dotdot);
    TEST_CASE(hashhash_invalid_name_string);
    TEST_CASE(hashhash_invalid_number_string);
    TEST_CASE(hashhash_encoding_prefix);
    TEST_CASE(hashhash_invalid_prefixed_literal);
    TEST_CASE(hashhash_va_args_unexpected);

    // C standard, 5.1.1.2, paragraph 4:
    //    If a character sequence that matches the syntax of a universal
    //    character name is produced by token concatenation (6.10.3.3),
    //    the behavior is undefined."
    TEST_CASE(hashhash_universal_character);
    TEST_CASE(hashhash_universal_character_2);

    // c++17 __has_include
    TEST_CASE(has_include_1);
    TEST_CASE(has_include_2);
    TEST_CASE(has_include_3);
    TEST_CASE(has_include_4);
    TEST_CASE(has_include_5);
    TEST_CASE(has_include_6);

    TEST_CASE(define_has_include_invalid_1);
    TEST_CASE(define_has_include_invalid_2);
    TEST_CASE(define_has_include_invalid_3);
    TEST_CASE(define_has_include_invalid_4);

    TEST_CASE(strict_ansi_1);
    TEST_CASE(strict_ansi_2);
    TEST_CASE(strict_ansi_3);
    TEST_CASE(strict_ansi_4);

    TEST_CASE(ifdef1);
    TEST_CASE(ifdef2);
    TEST_CASE(ifndef);
    TEST_CASE(ifA);
    TEST_CASE(ifFloatingPointLiteral); // #612
    TEST_CASE(ifCharLiteral);
    TEST_CASE(ifDefined);
    TEST_CASE(ifDefinedNoPar);
    TEST_CASE(ifDefinedNested);
    TEST_CASE(ifDefinedNestedNoPar);
    TEST_CASE(ifDefinedInvalid1);
    TEST_CASE(ifDefinedInvalid2);
    TEST_CASE(ifDefinedHashHash);
    TEST_CASE(ifDefinedHashHash2);
    TEST_CASE(definedAtEndOfFile);
    TEST_CASE(ifLogical);
    TEST_CASE(ifLogicalPrecedence);
    TEST_CASE(ifSizeof);
    TEST_CASE(elif);
    TEST_CASE(elifdef);
    TEST_CASE(elifndef);
    TEST_CASE(ifif);
    TEST_CASE(ifoverflow);
    TEST_CASE(ifdiv0);
    TEST_CASE(ifShortCircuit);
    TEST_CASE(ifZeroCondition);
    TEST_CASE(ifUnaryOperators);
    TEST_CASE(ifInvalidExpression);
    TEST_CASE(ifTrueFalse);
    TEST_CASE(ifalt); // using "and", "or", etc
    TEST_CASE(ifAltUnaryOperators);
    TEST_CASE(ifexpr);
    TEST_CASE(ifUndefFuncStyleMacro);

    TEST_CASE(location1);
    TEST_CASE(location2);
    TEST_CASE(location3);
    TEST_CASE(location4);
    TEST_CASE(location5);
    TEST_CASE(location6);
    TEST_CASE(location7);
    TEST_CASE(location8);
    TEST_CASE(location9);
    TEST_CASE(location10);
    TEST_CASE(location11);
    TEST_CASE(location12);

    TEST_CASE(missingHeader1);
    TEST_CASE(missingHeader2);
    TEST_CASE(missingHeader3);
    TEST_CASE(missingHeader4);
    TEST_CASE(nestedInclude);
    TEST_CASE(systemInclude);
    TEST_CASE(circularInclude);

    TEST_CASE(nullDirective1);
    TEST_CASE(nullDirective2);
    TEST_CASE(nullDirective3);

    TEST_CASE(lineDirective);

    TEST_CASE(include1);
    TEST_CASE(include2);
    TEST_CASE(include3);
    TEST_CASE(include4); // -include
    TEST_CASE(include5); // #include MACRO
    TEST_CASE(include6); // invalid code: #include MACRO(,)
    TEST_CASE(include7); // #include MACRO
    TEST_CASE(include8); // #include MACRO(X)
    TEST_CASE(include9); // #include MACRO
    TEST_CASE(include10); // -include with load()
    TEST_CASE(include11); // -include with preprocess()

    TEST_CASE(multiline1);
    TEST_CASE(multiline2);
    TEST_CASE(multiline3);
    TEST_CASE(multiline4);
    TEST_CASE(multiline5); // column
    TEST_CASE(multiline6); // multiline string in macro
    TEST_CASE(multiline7); // multiline string in macro
    TEST_CASE(multiline8); // multiline prefix string in macro
    TEST_CASE(multiline9); // multiline prefix string in macro
    TEST_CASE(multiline10);

    TEST_CASE(readfile_nullbyte);
    TEST_CASE(readfile_char);
    TEST_CASE(readfile_char_error);
    TEST_CASE(readfile_string);
    TEST_CASE(readfile_string_error);
    TEST_CASE(readfile_cpp14_number);
    TEST_CASE(readfile_unhandled_chars);
    TEST_CASE(readfile_error);
    TEST_CASE(readfile_file_not_found);
    TEST_CASE(readfile_no_eof_newline);
    TEST_CASE(readfile_cr_newlines);
    TEST_CASE(readfile_filestream);

    TEST_CASE(stringify1);

    TEST_CASE(tokenMacro1);
    TEST_CASE(tokenMacro2);
    TEST_CASE(tokenMacro3);
    TEST_CASE(tokenMacro4);
    TEST_CASE(tokenMacro5);
    TEST_CASE(tokenMacro6);
    TEST_CASE(tokenMacro7);
    TEST_CASE(tokenMacro8);
    TEST_CASE(tokenMacro9);
    TEST_CASE(tokenMacro10);
    TEST_CASE(tokenMacro11);
    TEST_CASE(tokenMacro12);
    TEST_CASE(tokenMacro13);
    TEST_CASE(tokenMacro14);
    TEST_CASE(tokenMacro15);
    TEST_CASE(hideSetExpansion_recursive);
    TEST_CASE(hideSetExpansion_mutual);
    TEST_CASE(hideSetExpansion_std_example);
    TEST_CASE(hideSetExpansion_trailing);
    TEST_CASE(hideSetExpansion_trailing_args);
    TEST_CASE(hideSetExpansion_args_lines);
    TEST_CASE(hideSetExpansion_nested);
    TEST_CASE(hideSetExpansion_arg_self);
    TEST_CASE(hideSetExpansion_blue_in_arg);
    TEST_CASE(hideSetExpansion_paren_in_arg);
    TEST_CASE(hideSetExpansion_not_call);
    TEST_CASE(hideSetExpansion_obj_to_fn);
    TEST_CASE(hideSetExpansion_va_args);
    TEST_CASE(hideSetExpansion_counter);
    TEST_CASE(hideSetExpansion_line);
    TEST_CASE(hideSetExpansion_deep);
    TEST_CASE(hideSetExpansion_q_example);
    TEST_CASE(hideSetExpansion_empty);
    TEST_CASE(hideSetExpansion_va_opt_empty_after_expansion);
    TEST_CASE(hideSetExpansion_va_opt_hash);
    TEST_CASE(hideSetExpansion_va_opt_hashhash);
    TEST_CASE(hideSetExpansion_va_opt_hash_in_content);
    TEST_CASE(hideSetExpansion_va_opt_rescan);
    TEST_CASE(hideSetExpansion_va_opt_comma);
    TEST_CASE(hideSetExpansion_va_opt_parentheses);

    TEST_CASE(undef);

    TEST_CASE(userdef);

    // utf/unicode
    TEST_CASE(utf8);
    TEST_CASE(utf8_invalid);
    TEST_CASE(unicode);
    TEST_CASE(unicode_invalid);

    TEST_CASE(warning);

    // utility functions.
    TEST_CASE(simplifyPath);
    TEST_CASE(simplifyPath_cppcheck);
    TEST_CASE(simplifyPath_New);

    TEST_CASE(preprocessSizeOf);

    TEST_CASE(timeDefine);
    TEST_CASE(dateDefine);

    TEST_CASE(stdcVersionDefine);
    TEST_CASE(cpluscplusDefine);
    TEST_CASE(invalidStd);
    TEST_CASE(stdEnum);
    TEST_CASE(stdValid);

    TEST_CASE(token);

    TEST_CASE(preprocess_files);

    TEST_CASE(tokenlist_api);

    TEST_CASE(isAbsolutePath);

    TEST_CASE(bad_macro_syntax);

    TEST_CASE(ifCond);
    TEST_CASE(macroUsage);

    TEST_CASE(fuzz_crash);

    TEST_CASE(leak);
}

int main(int argc, char **argv)
{
    runTests(argc, argv, Input::Stringstream);
    runTests(argc, argv, Input::CharBuffer);
    USE_HIDESET_EXPANSION = true;
    runTests(argc, argv, Input::Stringstream);
    if (numberOfHideSetTodos > 0)
        std::cout << "Tests skipped with the hide-set expansion: " << numberOfHideSetTodos << std::endl;
    return numberOfFailedAssertions > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
