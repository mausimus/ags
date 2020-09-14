#include <string>
#include <map>
#include <iostream>  
#include <fstream>
#include <streambuf>


#include "script/cc_options.h"
#include "gtest/gtest.h"
#include "script/cs_parser.h"
#include "script/cc_symboltable.h"
#include "script/cc_internallist.h"


/* This file is for Bytecode tests ONLY.
   Testing for programs that won't compile should be done elsewhere IMHO.
   
   If a test generates wrong code, often a good way to debug it is as follows:
   - Uncomment the "WriteOutput(" line and run the test; now you have the bytes
     that the _new_ compiler emits in the file. Compare it by hand to the code
     that's in the test. See what the problem is: Has the new compiler left
     opcodes out, or has it added some in? Has it changed a single value somewhere?
   - Find the line in cs_compiledscript.cpp, void ccCompiledScript::write_code,
     where a bytecode cell is appended, and set a breakpoint.
   - Configure the breakpoint to break when the last byte comes up that is still
     correct.
   - Debug the googletest:
     Trace along from the breakpoint and find the point where the logic fails.

   But bear in mind:
   - Sometimes the compiler emits some code, then rips it out and stashes it away,
     then, later on, retrieves it and emits it again.
   - Sometimes the compiler emits some code value, then patches it afterwards.
   These are the hard cases to debug. :)
*/


// NOTE! If any "WriteOutput" lines in this file are uncommented, then the 
//  #define below _must_ be changed to a local writable temp dir. 
// (If you only want to run the tests to see if any tests fail, you do NOT 
// need that dir and you do NOT need any local files whatsoever.)
#define LOCAL_PATH "C:\\TEMP\\"

extern void clear_error();
extern const char *last_seen_cc_error();
extern ccCompiledScript *newScriptFixture();

// from cs_parser1_test.cpp, provide "ready-made" code chunks to be included in tests
extern char g_Input_Bool[], g_Input_String[];

std::string Esc(const char ch)
{
    if (ch >= ' ' && ch <= 126)
    {
        return std::string(1, ch);
    }

    switch (ch)
    {
    default:
    {
        static const char *tohex = "0123456789abcdef";
        std::string ret = "\\x";
        ret.push_back(tohex[ch / 16]);
        ret.push_back(tohex[ch % 16]);
        return ret;
    }
    case '\a': return "\\a";
    case '\b': return "\\b";
    case '\f': return "\\f";
    case '\n': return "\\n";
    case '\r': return "\\r";
    case '\v': return "\\v";
    case '\'': return "\\\'";
    case '\"': return "\\\"";
    case '\\': return "\\\\";
    }
}

std::string EscapeString(const char *in)
{
    if (in == nullptr)
        return "0";

    std::string ret = "";
    size_t const in_len = strlen(in);
    for (size_t idx = 0; idx < in_len; idx++)
        ret += Esc(in[idx]);
    return "\"" + ret + "\"";
}

void WriteOutputCode(std::ofstream &of, ccCompiledScript *scrip)
{
    of << "const size_t codesize = " << scrip->codesize << ";" << std::endl;
    of << "EXPECT_EQ(codesize, scrip->codesize);" << std::endl << std::endl;

    if (scrip->codesize == 0)
        return;

    of << "intptr_t code[] = {" << std::endl;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->codesize; idx++)
    {
        of.width(4);
        of << scrip->code[idx] << ", ";
        if (idx % 8 == 3) of << "        ";
        if (idx % 8 == 7) of << "   // " << idx << std::endl;
    }
    of << " -999 " << std::endl << "};" << std::endl << std::endl;

    of << "for (size_t idx = 0; idx < codesize; idx++)" << std::endl;
    of << "{" << std::endl;
    of << "     if (static_cast<int>(idx) >= scrip->codesize) break;" << std::endl;
    of << "     std::string prefix = \"code[\";" << std::endl;
    of << "     prefix += std::to_string(idx) + \"] == \";" << std::endl;
    of << "     std::string is_val = prefix + std::to_string(code[idx]);" << std::endl;
    of << "     std::string test_val = prefix + std::to_string(scrip->code[idx]);" << std::endl;
    of << "     ASSERT_EQ(is_val, test_val);" << std::endl;
    of << "}" << std::endl << std::endl;

}

void WriteOutputFixups(std::ofstream &of, ccCompiledScript *scrip)
{
    of << "const size_t numfixups = " << scrip->numfixups << ";" << std::endl;
    of << "EXPECT_EQ(numfixups, scrip->numfixups);" << std::endl << std::endl;

    if (scrip->numfixups == 0)
        return;

    of << "intptr_t fixups[] = {" << std::endl;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numfixups; idx++)
    {
        of.width(4);
        of << scrip->fixups[idx] << ", ";
        if (idx % 8 == 3) of << "      ";
        if (idx % 8 == 7) of << "   // " << idx << std::endl;
    }
    of << " -999 " << std::endl << "};" << std::endl << std::endl;

    of << "for (size_t idx = 0; idx < numfixups; idx++)" << std::endl;
    of << "{" << std::endl;
    of << "     if (static_cast<int>(idx) >= scrip->numfixups) break;" << std::endl;
    of << "     std::string prefix = \"fixups[\";" << std::endl;
    of << "     prefix += std::to_string(idx) + \"] == \";" << std::endl;
    of << "     std::string   is_val = prefix + std::to_string(fixups[idx]);" << std::endl;
    of << "     std::string test_val = prefix + std::to_string(scrip->fixups[idx]);" << std::endl;
    of << "     ASSERT_EQ(is_val, test_val);" << std::endl;
    of << "}" << std::endl << std::endl;

    of << "char fixuptypes[] = {" << std::endl;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numfixups; idx++)
    {
        of.width(3);
        of << static_cast<int>(scrip->fixuptypes[idx]) << ", ";
        if (idx % 8 == 3) of << "   ";
        if (idx % 8 == 7) of << "   // " << idx << std::endl;
    }
    of << " '\\0' " << std::endl << "};" << std::endl << std::endl;

    of << "for (size_t idx = 0; idx < numfixups; idx++)" << std::endl;
    of << "{" << std::endl;
    of << "     if (static_cast<int>(idx) >= scrip->numfixups) break;" << std::endl;
    of << "     std::string prefix = \"fixuptypes[\";" << std::endl;
    of << "     prefix += std::to_string(idx) + \"] == \";" << std::endl;
    of << "     std::string   is_val = prefix + std::to_string(fixuptypes[idx]);" << std::endl;
    of << "     std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);" << std::endl;
    of << "     ASSERT_EQ(is_val, test_val);" << std::endl;
    of << "}" << std::endl << std::endl;
}

void WriteOutputImports(std::ofstream &of, ccCompiledScript *scrip)
{
    // Unfortunately, imports can contain empty strings that
    // mustn't be counted. So we can't just believe numimports,
    // and we can't check against scrip->numimports.
    size_t realNumImports = 0;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
        if (0 != strcmp(scrip->imports[idx], ""))
            ++realNumImports;

    of << "const int numimports = " << realNumImports << ";" << std::endl;

    of << "std::string imports[] = {" << std::endl;

    size_t linelen = 0;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;

        std::string item = EscapeString(scrip->imports[idx]);
        item += ",";
        item += std::string(15 - (item.length() % 15), ' ');
        of << item;
        linelen += item.length();
        if (linelen >= 75)
        {
            linelen = 0;
            of << "// " << idx << std::endl;
        }
    }
    of << " \"[[SENTINEL]]\" " << std::endl << "};" << std::endl << std::endl;

    of << "int idx2 = -1;" << std::endl;
    of << "for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)" << std::endl;
    of << "{" << std::endl;
    of << "     if (!strcmp(scrip->imports[idx], \"\"))" << std::endl;
    of << "         continue;" << std::endl;
    of << "     idx2++;" << std::endl;
    of << "     ASSERT_LT(idx2, numimports);" << std::endl;
    of << "     std::string prefix = \"imports[\";" << std::endl;
    // Note that the prefix has to be identical for is_val and test_val,
    // or ASSERT_EQ will always fail.
    of << "     prefix += std::to_string(idx2) + \"] == \";" << std::endl;
    of << "     std::string is_val   = prefix + scrip->imports[idx];" << std::endl;
    of << "     std::string test_val = prefix + imports[idx2];" << std::endl;
    of << "     ASSERT_EQ(is_val, test_val);" << std::endl;
    of << "}" << std::endl << std::endl;
}

void WriteOutputExports(std::ofstream &of, ccCompiledScript *scrip)
{
    of << "const size_t numexports = " << scrip->numexports << ";" << std::endl;
    of << "EXPECT_EQ(numexports, scrip->numexports);" << std::endl << std::endl;

    if (scrip->numexports == 0)
        return;

    of << "std::string exports[] = {" << std::endl;

    size_t linelen = 0;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numexports; idx++)
    {
        std::string item = EscapeString(scrip->exports[idx]);
        item += ",";
        item += std::string(6 - (item.length() % 6), ' ');
        of << item;
        linelen += item.length();
        if (linelen >= 50)
        {
            linelen = 0;
            of << "// " << idx << std::endl;
        }
    }
    of << " \"[[SENTINEL]]\" " << std::endl << "};" << std::endl << std::endl;

    of << "for (size_t idx = 0; idx < numexports; idx++)" << std::endl;
    of << "{" << std::endl;
    of << "     if (static_cast<int>(idx) >= scrip->numexports) break;" << std::endl;
    of << "     std::string prefix = \"exports[\";" << std::endl;
    of << "     prefix += std::to_string(idx) + \"] == \";" << std::endl;
    of << "     std::string is_val = prefix + exports[idx];" << std::endl;
    of << "     std::string test_val = prefix + scrip->exports[idx];" << std::endl;
    of << "     ASSERT_EQ(is_val, test_val);" << std::endl;
    of << "}" << std::endl << std::endl;


    of << "int32_t export_addr[] = {" << std::endl;

    for (size_t idx = 0; static_cast<int>(idx) < scrip->numexports; idx++)
    {
        of.setf(std::ios::hex, std::ios::basefield);
        of.setf(std::ios::showbase);
        of.width(4);
        of << scrip->export_addr[idx] << ", ";
        if (idx % 4 == 1) of << "   ";
        if (idx % 8 == 3)
        {
            of.setf(std::ios::dec, std::ios::basefield);
            of.unsetf(std::ios::showbase);
            of.width(0);
            of << "// " << idx;
            of << std::endl;
        }
    }

    of.setf(std::ios::dec, std::ios::basefield);
    of.unsetf(std::ios::showbase);
    of.width(0);

    of << " 0 " << std::endl << "};" << std::endl << std::endl;

    of << "for (size_t idx = 0; idx < numexports; idx++)" << std::endl;
    of << "{" << std::endl;
    of << "     if (static_cast<int>(idx) >= scrip->numexports) break;" << std::endl;
    of << "     std::string prefix = \"export_addr[\";" << std::endl;
    of << "     prefix += std::to_string(idx) + \"] == \";" << std::endl;
    of << "     std::string is_val   = prefix + std::to_string(export_addr[idx]);" << std::endl;
    of << "     std::string test_val = prefix + std::to_string(scrip->export_addr[idx]);" << std::endl;
    of << "     ASSERT_EQ(is_val, test_val);" << std::endl;
    of << "}" << std::endl << std::endl;
}

void WriteOutputStrings(std::ofstream &of, ccCompiledScript *scrip)
{
    of << "const size_t stringssize = " << scrip->stringssize << ";" << std::endl;
    of << "EXPECT_EQ(stringssize, scrip->stringssize);" << std::endl << std::endl;

    if (scrip->stringssize == 0)
        return;

    of << "char strings[] = {" << std::endl;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->stringssize; idx++)
    {
        std::string out = "";
        if (scrip->strings[idx] == 0)
            out = "  0";
        else
            out += '\'' + Esc(scrip->strings[idx]) + '\'';
        of << out << ",  ";
        if (idx % 8 == 3) of << "        ";
        if (idx % 8 == 7) of << "   // " << idx << std::endl;
    }
    of << "'\\0'" << std::endl << "};" << std::endl << std::endl;

    of << "for (size_t idx = 0; static_cast<int>(idx) < stringssize; idx++)" << std::endl;
    of << "{" << std::endl;
    of << "     if (static_cast<int>(idx) >= scrip->stringssize) break;" << std::endl;
    of << "     std::string prefix = \"strings[\";" << std::endl;
    of << "     prefix += std::to_string(idx) + \"] == \";" << std::endl;
    of << "     std::string is_val = prefix + std::to_string(strings[idx]);" << std::endl;
    of << "     std::string test_val = prefix + std::to_string(scrip->strings[idx]);" << std::endl;
    of << "     ASSERT_EQ(is_val, test_val);" << std::endl;
    of << "}" << std::endl;

}

void WriteOutput(char *fname, ccCompiledScript *scrip)
{
    std::string path = LOCAL_PATH;
    std::ofstream of;
    of.open(path.append(fname).append(".txt"));

    WriteOutputCode(of, scrip);
    WriteOutputFixups(of, scrip);
    WriteOutputImports(of, scrip);
    WriteOutputExports(of, scrip);
    WriteOutputStrings(of, scrip);

    of.close();
}

void WriteReducedOutput(char *fname, ccCompiledScript *scrip)
{
    std::string path = LOCAL_PATH;
    std::ofstream of;
    of.open(path.append(fname).append(".txt"));

    WriteOutputCode(of, scrip);
    WriteOutputFixups(of, scrip);

    of.close();
}

/*    PROTOTYPE

TEST(Bytecode, P_r_o_t_o_t_y_p_e) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int Foo(int a)      \n\
        {                   \n\
            return a*a;     \n\
        }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("P_r_o_t_o_t_y_p_e", scrip);
    // run the test, comment out the previous line
    // and append its output below.
    // Then run the test in earnest after changes have been made to the code

}
*/

TEST(Bytecode, SimpleVoidFunction) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        void Foo()          \n\
        {                   \n\
            return;         \n\
        }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("SimpleVoidFunction", scrip);
    const size_t codesize = 5;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   31,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, UnaryMinus1) {
    ccCompiledScript *scrip = newScriptFixture();

    // Accept a unary minus in front of parens

    char *inpl = "\
        void Foo()              \n\
        {                       \n\
            int bar = 5;        \n\
            int baz = -(-bar);  \n\
        }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("UnaryMinus1", scrip);
    const size_t codesize = 35;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   29,    3,   51,    // 7
       4,    7,    3,    6,            4,    0,   12,    4,    // 15
       3,    3,    4,    3,            6,    4,    0,   12,    // 23
       4,    3,    3,    4,            3,   29,    3,    2,    // 31
       1,    8,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, UnaryMinus2) {
    ccCompiledScript *scrip = newScriptFixture();

    // Unary minus binds more than multiply

    char *inpl = "\
        int main()                      \n\
        {                               \n\
            int five = 5;               \n\
            int seven = 7;              \n\
            return -five * -seven;      \n\
        }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("UnaryMinus2", scrip);
    const size_t codesize = 60;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   29,    3,    6,    // 7
       3,    7,   29,    3,           51,    8,    7,    3,    // 15
       6,    4,    0,   12,            4,    3,    3,    4,    // 23
       3,   29,    3,   51,            8,    7,    3,    6,    // 31
       4,    0,   12,    4,            3,    3,    4,    3,    // 39
      30,    4,    9,    4,            3,    3,    4,    3,    // 47
       2,    1,    8,   31,            6,    2,    1,    8,    // 55
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, NotNot) {
    ccCompiledScript *scrip = newScriptFixture();

    // !!a should be interpreted as !(!a)
    char *inpl = "\
        int main()                  \n\
        {                           \n\
            int five = 5;           \n\
            return !!(!five);       \n\
        }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Notnot", scrip);
    const size_t codesize = 29;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   29,    3,   51,    // 7
       4,    7,    3,   42,            3,   42,    3,   42,    // 15
       3,    2,    1,    4,           31,    6,    2,    1,    // 23
       4,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, SimpleIntFunction) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int Foo()      \n\
    {                  \n\
        return 15;     \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("SimpleIntFunction", scrip);
    const size_t codesize = 11;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           15,   31,    3,    6,    // 7
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, IntFunctionLocalV) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int Foo()       \n\
        {               \n\
            int a = 15; \n\
            return a;   \n\
        }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("IntFunctionLocalV", scrip);
    const size_t codesize = 23;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           15,   29,    3,   51,    // 7
       4,    7,    3,    2,            1,    4,   31,    6,    // 15
       2,    1,    4,    6,            3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, IntFunctionParam) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int Foo(int a) \n\
    {                  \n\
        return a;      \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("IntFunctionParam", scrip);
    const size_t codesize = 12;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   31,    3,    // 7
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, IntFunctionGlobalV) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int a = 15;    \n\
        int Foo( )     \n\
    {                  \n\
        return a;      \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("IntFunctionGlobalV", scrip);
    const size_t codesize = 13;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            0,    7,    3,   31,    // 7
       3,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, Float1) {
    ccCompiledScript *scrip = newScriptFixture();

    // Float values

    char inpl[] = "\
        float Test0 = -9.9;                 \n\
        float main()                        \n\
        {                                   \n\
            float Test1 = -7.0;             \n\
            float Test2 = 7E2;              \n\
            float Test3 = -7E-2;            \n\
            float Test4 = -7.7E-0;          \n\
            float Test5 = 7.;               \n\
            float Test6 = 7.e-7;            \n\
            float Test7 = 007.e-07;         \n\
            float Test8 = .77;              \n\
            return Test1 + Test2 + Test3 +  \n\
                Test4 + Test5 + Test6 +     \n\
                Test7 + Test8;              \n\
        }                                   \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Float1", scrip);
    const size_t codesize = 156;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,         -1059061760,   29,    3,    6,    // 7
       3, 1143930880,   29,    3,            6,    3, -1114678231,   29,    // 15
       3,    6,    3, -1057593754,           29,    3,    6,    3,    // 23
    1088421888,   29,    3,    6,            3, 893118370,   29,    3,    // 31
       6,    3, 893118370,   29,            3,    6,    3, 1061494456,    // 39
      29,    3,   51,   32,            7,    3,   29,    3,    // 47
      51,   32,    7,    3,           30,    4,   57,    4,    // 55
       3,    3,    4,    3,           29,    3,   51,   28,    // 63
       7,    3,   30,    4,           57,    4,    3,    3,    // 71
       4,    3,   29,    3,           51,   24,    7,    3,    // 79
      30,    4,   57,    4,            3,    3,    4,    3,    // 87
      29,    3,   51,   20,            7,    3,   30,    4,    // 95
      57,    4,    3,    3,            4,    3,   29,    3,    // 103
      51,   16,    7,    3,           30,    4,   57,    4,    // 111
       3,    3,    4,    3,           29,    3,   51,   12,    // 119
       7,    3,   30,    4,           57,    4,    3,    3,    // 127
       4,    3,   29,    3,           51,    8,    7,    3,    // 135
      30,    4,   57,    4,            3,    3,    4,    3,    // 143
       2,    1,   32,   31,            6,    2,    1,   32,    // 151
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Float2) {
    ccCompiledScript *scrip = newScriptFixture();

    // Positive and negative float parameter defaults

    char inpl[] = "\
        float sub (float p1 = 7.2,          \n\
                   float p2 = -2.7)         \n\
        {                                   \n\
            return -7.0 + p1 - p2;          \n\
        }                                   \n\
        float main()                        \n\
        {                                   \n\
            return sub();                   \n\
        }                                   \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Float2", scrip);
    const size_t codesize = 65;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,         -1059061760,   29,    3,   51,    // 7
      12,    7,    3,   30,            4,   57,    4,    3,    // 15
       3,    4,    3,   29,            3,   51,   16,    7,    // 23
       3,   30,    4,   58,            4,    3,    3,    4,    // 31
       3,   31,    3,    6,            3,    0,    5,   38,    // 39
      39,    6,    3, -1070805811,           29,    3,    6,    3,    // 47
    1088841318,   29,    3,    6,            3,    0,   23,    3,    // 55
       2,    1,    8,   31,            3,    6,    3,    0,    // 63
       5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      53,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, FloatExpr1) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        float a = 15.0;     \n\
        float Foo()         \n\
        {                   \n\
            float f = 3.14; \n\
            return a + f;   \n\
        }                   \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("FloatExpr1", scrip);
    const size_t codesize = 38;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,         1078523331,   29,    3,    6,    // 7
       2,    0,    7,    3,           29,    3,   51,    8,    // 15
       7,    3,   30,    4,           57,    4,    3,    3,    // 23
       4,    3,    2,    1,            4,   31,    6,    2,    // 31
       1,    4,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       9,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, FloatExpr2) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        float a = 15.0;                             \n\
        float Foo()                                 \n\
        {                                           \n\
            float b = 22.2;                         \n\
            int E1 = (3.14 < 1.34) == 1;            \n\
            short E2 = 0 == (1234.5 > 5.0) && 1;    \n\
            long E3 = a <= 44.4;                    \n\
            char E4 = 55.5 >= 44.4;                 \n\
            int E5 = (((a == b) || (a != b)));      \n\
            return a - b * (a / b);                 \n\
        }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("FloatExpr2", scrip);
    const size_t codesize = 244;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,         1102158234,   29,    3,    6,    // 7
       3, 1078523331,   29,    3,            6,    3, 1068205343,   30,    // 15
       4,   60,    4,    3,            3,    4,    3,   29,    // 23
       3,    6,    3,    1,           30,    4,   15,    4,    // 31
       3,    3,    4,    3,           29,    3,    6,    3,    // 39
       0,   29,    3,    6,            3, 1150963712,   29,    3,    // 47
       6,    3, 1084227584,   30,            4,   59,    4,    3,    // 55
       3,    4,    3,   30,            4,   15,    4,    3,    // 63
       3,    4,    3,   28,           13,   29,    3,    6,    // 71
       3,    1,   30,    4,           21,    4,    3,    3,    // 79
       4,    3,   51,    0,           27,    3,    1,    1,    // 87
       2,    6,    2,    0,            7,    3,   29,    3,    // 95
       6,    3, 1110546842,   30,            4,   62,    4,    3,    // 103
       3,    4,    3,   29,            3,    6,    3, 1113456640,    // 111
      29,    3,    6,    3,         1110546842,   30,    4,   61,    // 119
       4,    3,    3,    4,            3,   51,    0,   26,    // 127
       3,    1,    1,    1,            6,    2,    0,    7,    // 135
       3,   29,    3,   51,           19,    7,    3,   30,    // 143
       4,   15,    4,    3,            3,    4,    3,   70,    // 151
      29,   29,    3,    6,            2,    0,    7,    3,    // 159
      29,    3,   51,   23,            7,    3,   30,    4,    // 167
      16,    4,    3,    3,            4,    3,   30,    4,    // 175
      22,    4,    3,    3,            4,    3,   29,    3,    // 183
       6,    2,    0,    7,            3,   29,    3,   51,    // 191
      23,    7,    3,   29,            3,    6,    2,    0,    // 199
       7,    3,   29,    3,           51,   31,    7,    3,    // 207
      30,    4,   56,    4,            3,    3,    4,    3,    // 215
      30,    4,   55,    4,            3,    3,    4,    3,    // 223
      30,    4,   58,    4,            3,    3,    4,    3,    // 231
       2,    1,   19,   31,            6,    2,    1,   19,    // 239
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 5;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      91,  134,  157,  186,        199,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   1,      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, IfThenElse1) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    int Foo()               \n\
    {                       \n\
        int a = 15 - 4 * 2; \n\
        if (a < 5)          \n\
            a >>= 2;        \n\
        else                \n\
            a <<= 3;        \n\
        return a;           \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("IfThenElse1", scrip);
    const size_t codesize = 102;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           15,   29,    3,    6,    // 7
       3,    4,   29,    3,            6,    3,    2,   30,    // 15
       4,    9,    4,    3,            3,    4,    3,   30,    // 23
       4,   12,    4,    3,            3,    4,    3,   29,    // 31
       3,   51,    4,    7,            3,   29,    3,    6,    // 39
       3,    5,   30,    4,           18,    4,    3,    3,    // 47
       4,    3,   28,   18,            6,    3,    2,   29,    // 55
       3,   51,    8,    7,            3,   30,    4,   44,    // 63
       3,    4,    8,    3,           31,   16,    6,    3,    // 71
       3,   29,    3,   51,            8,    7,    3,   30,    // 79
       4,   43,    3,    4,            8,    3,   51,    4,    // 87
       7,    3,    2,    1,            4,   31,    6,    2,    // 95
       1,    4,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, IfThenElse2) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    int Foo()               \n\
    {                       \n\
        int a = 15 - 4 % 2; \n\
        if (a >= 5) {       \n\
            a -= 2;         \n\
        } else {            \n\
            a += 3;         \n\
        }                   \n\
        return a;           \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("IfThenElse2", scrip);
    const size_t codesize = 102;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           15,   29,    3,    6,    // 7
       3,    4,   29,    3,            6,    3,    2,   30,    // 15
       4,   40,    4,    3,            3,    4,    3,   30,    // 23
       4,   12,    4,    3,            3,    4,    3,   29,    // 31
       3,   51,    4,    7,            3,   29,    3,    6,    // 39
       3,    5,   30,    4,           19,    4,    3,    3,    // 47
       4,    3,   28,   18,            6,    3,    2,   29,    // 55
       3,   51,    8,    7,            3,   30,    4,   12,    // 63
       3,    4,    8,    3,           31,   16,    6,    3,    // 71
       3,   29,    3,   51,            8,    7,    3,   30,    // 79
       4,   11,    3,    4,            8,    3,   51,    4,    // 87
       7,    3,    2,    1,            4,   31,    6,    2,    // 95
       1,    4,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, While) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    char c = 'x';             \n\
    int Foo(int i, float f)   \n\
    {                         \n\
        int sum = 0;          \n\
        while (c >= 0)        \n\
        {                     \n\
            sum += (500 & c); \n\
            c--;              \n\
            if (c == 1) continue; \n\
        }                     \n\
        return sum;           \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("While", scrip);
    const size_t codesize = 108;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   29,    3,    6,    // 7
       2,    0,   24,    3,           29,    3,    6,    3,    // 15
       0,   30,    4,   19,            4,    3,    3,    4,    // 23
       3,   28,   65,    6,            3,  500,   29,    3,    // 31
       6,    2,    0,   24,            3,   30,    4,   13,    // 39
       4,    3,    3,    4,            3,   29,    3,   51,    // 47
       8,    7,    3,   30,            4,   11,    3,    4,    // 55
       8,    3,    6,    2,            0,   24,    3,    2,    // 63
       3,    1,   26,    3,            6,    2,    0,   24,    // 71
       3,   29,    3,    6,            3,    1,   30,    4,    // 79
      15,    4,    3,    3,            4,    3,   28,    2,    // 87
      31,  -83,   31,  -85,           51,    4,    7,    3,    // 95
       2,    1,    4,   31,            6,    2,    1,    4,    // 103
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 4;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       9,   34,   60,   70,        -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   1,     '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}



TEST(Bytecode, DoNCall) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    char c = 'x';             \n\
    int Foo(int i)            \n\
    {                         \n\
        int sum = 0;          \n\
        do                    \n\
        {                     \n\
            sum -= (500 | c); \n\
            c--;              \n\
        }                     \n\
        while (c > 0);        \n\
        return sum;           \n\
    }                         \n\
                              \n\
    int Bar(int x)            \n\
    {                         \n\
        return Foo(x^x);      \n\
    }                         \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("DoNCall", scrip);
    const size_t codesize = 120;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   29,    3,    6,    // 7
       3,  500,   29,    3,            6,    2,    0,   24,    // 15
       3,   30,    4,   14,            4,    3,    3,    4,    // 23
       3,   29,    3,   51,            8,    7,    3,   30,    // 31
       4,   12,    3,    4,            8,    3,    6,    2,    // 39
       0,   24,    3,    2,            3,    1,   26,    3,    // 47
       6,    2,    0,   24,            3,   29,    3,    6,    // 55
       3,    0,   30,    4,           17,    4,    3,    3,    // 63
       4,    3,   70,  -61,           51,    4,    7,    3,    // 71
       2,    1,    4,   31,            6,    2,    1,    4,    // 79
       6,    3,    0,    5,           38,   84,   51,    8,    // 87
       7,    3,   29,    3,           51,   12,    7,    3,    // 95
      30,    4,   41,    4,            3,    3,    4,    3,    // 103
      29,    3,    6,    3,            0,   23,    3,    2,    // 111
       1,    4,   31,    3,            6,    3,    0,    5,    // 119
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 4;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      14,   40,   50,  108,        -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   2,     '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, DoUnbracedIf) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    void noloopcheck main()   \n\
    {                         \n\
        int sum = 0;          \n\
        do                    \n\
            if (sum < 100)    \n\
                sum += 10;    \n\
            else              \n\
                break;        \n\
        while (sum >= -1);    \n\
    }                         \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("DoUnbracedIf", scrip);
    const size_t codesize = 70;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   68,    6,            3,    0,   29,    3,    // 7
      51,    4,    7,    3,           29,    3,    6,    3,    // 15
     100,   30,    4,   18,            4,    3,    3,    4,    // 23
       3,   28,   18,    6,            3,   10,   29,    3,    // 31
      51,    8,    7,    3,           30,    4,   11,    3,    // 39
       4,    8,    3,   31,            2,   31,   19,   51,    // 47
       4,    7,    3,   29,            3,    6,    3,   -1,    // 55
      30,    4,   19,    4,            3,    3,    4,    3,    // 63
      70,  -58,    2,    1,            4,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}


TEST(Bytecode, For1) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    int loop;                       \n\
    int Foo(int i, float f)         \n\
    {                               \n\
        for (loop = 0; loop < 10; loop += 3)  \n\
        {                           \n\
            int sum = loop - 4 - 7; \n\
            if (loop == 6)          \n\
                break;              \n\
        }                           \n\
        return 0;                   \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("For1", scrip);
    const size_t codesize = 119;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,    6,    2,    0,    // 7
       8,    3,    6,    2,            0,    7,    3,   29,    // 15
       3,    6,    3,   10,           30,    4,   18,    4,    // 23
       3,    3,    4,    3,           28,   80,    6,    2,    // 31
       0,    7,    3,   29,            3,    6,    3,    4,    // 39
      30,    4,   12,    4,            3,    3,    4,    3,    // 47
      29,    3,    6,    3,            7,   30,    4,   12,    // 55
       4,    3,    3,    4,            3,   29,    3,    6,    // 63
       2,    0,    7,    3,           29,    3,    6,    3,    // 71
       6,   30,    4,   15,            4,    3,    3,    4,    // 79
       3,   28,    5,    2,            1,    4,   31,   22,    // 87
       2,    1,    4,    6,            3,    3,   29,    3,    // 95
       6,    2,    0,    7,            3,   30,    4,   11,    // 103
       3,    4,    8,    3,           31, -100,    6,    3,    // 111
       0,   31,    3,    6,            3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 5;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       7,   12,   32,   65,         98,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   1,      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, For2) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    int Foo(int i, float f)         \n\
    {                               \n\
        int lp, sum;                \n\
        for (; ; lp += 1)           \n\
            sum += lp;              \n\
        for ( ;; )                  \n\
            sum -= lp;              \n\
        for (; lp < 2; lp += 3)     \n\
            sum *= lp;              \n\
        for (; lp < 4; )            \n\
            sum /= lp;              \n\
        for (lp = 5; ; lp += 6)     \n\
            sum /= lp;              \n\
        for (int loop = 7; ; )      \n\
            sum &= loop;            \n\
        for (int loop = 8; loop < 9; )  \n\
            sum |= loop;            \n\
        return 0;                   \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("For2", scrip);
    const size_t codesize = 312;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           63,    4,    1,    1,    // 7
       4,   51,    0,   63,            4,    1,    1,    4,    // 15
       6,    3,    1,   28,           35,   51,    8,    7,    // 23
       3,   29,    3,   51,            8,    7,    3,   30,    // 31
       4,   11,    3,    4,            8,    3,    6,    3,    // 39
       1,   29,    3,   51,           12,    7,    3,   30,    // 47
       4,   11,    3,    4,            8,    3,   31,  -40,    // 55
       6,    3,    1,   28,           19,   51,    8,    7,    // 63
       3,   29,    3,   51,            8,    7,    3,   30,    // 71
       4,   12,    3,    4,            8,    3,   31,  -24,    // 79
      51,    8,    7,    3,           29,    3,    6,    3,    // 87
       2,   30,    4,   18,            4,    3,    3,    4,    // 95
       3,   28,   35,   51,            8,    7,    3,   29,    // 103
       3,   51,    8,    7,            3,   30,    4,    9,    // 111
       3,    4,    8,    3,            6,    3,    3,   29,    // 119
       3,   51,   12,    7,            3,   30,    4,   11,    // 127
       3,    4,    8,    3,           31,  -54,   51,    8,    // 135
       7,    3,   29,    3,            6,    3,    4,   30,    // 143
       4,   18,    4,    3,            3,    4,    3,   28,    // 151
      19,   51,    8,    7,            3,   29,    3,   51,    // 159
       8,    7,    3,   30,            4,   10,    3,    4,    // 167
       8,    3,   31,  -38,            6,    3,    5,   51,    // 175
       8,    8,    3,    6,            3,    1,   28,   35,    // 183
      51,    8,    7,    3,           29,    3,   51,    8,    // 191
       7,    3,   30,    4,           10,    3,    4,    8,    // 199
       3,    6,    3,    6,           29,    3,   51,   12,    // 207
       7,    3,   30,    4,           11,    3,    4,    8,    // 215
       3,   31,  -40,    6,            3,    7,   29,    3,    // 223
       6,    3,    1,   28,           19,   51,    4,    7,    // 231
       3,   29,    3,   51,           12,    7,    3,   30,    // 239
       4,   13,    3,    4,            8,    3,   31,  -24,    // 247
       2,    1,    4,    6,            3,    8,   29,    3,    // 255
      51,    4,    7,    3,           29,    3,    6,    3,    // 263
       9,   30,    4,   18,            4,    3,    3,    4,    // 271
       3,   28,   19,   51,            4,    7,    3,   29,    // 279
       3,   51,   12,    7,            3,   30,    4,   14,    // 287
       3,    4,    8,    3,           31,  -38,    2,    1,    // 295
       4,    6,    3,    0,            2,    1,    8,   31,    // 303
       6,    2,    1,    8,            6,    3,    0,    5,    // 311
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, For3) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct Struct           \n\
    {                               \n\
        float Payload[1];           \n\
    };                              \n\
    Struct *S;                      \n\
                                    \n\
    int main()                      \n\
    {                               \n\
        for (Struct *loop; ;)       \n\
        {                           \n\
            return ((loop == S));   \n\
        }                           \n\
        return -7;                  \n\
    }                               \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("For3", scrip);
    const size_t codesize = 57;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           49,    1,    1,    4,    // 7
       6,    3,    1,   28,           29,   51,    4,   48,    // 15
       3,   29,    3,    6,            2,    0,   48,    3,    // 23
      30,    4,   15,    4,            3,    3,    4,    3,    // 31
      51,    4,   49,    2,            1,    4,   31,   16,    // 39
      31,  -34,   51,    4,           49,    2,    1,    4,    // 47
       6,    3,   -7,   31,            3,    6,    3,    0,    // 55
       5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      21,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, For4) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    void main()                     \n\
    {                               \n\
        for (int Loop = 0; Loop < 10; Loop++)  \n\
            if (Loop == 5)          \n\
                continue;           \n\
    }                               \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("For4", scrip);
    const size_t codesize = 71;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   29,    3,   51,    // 7
       4,    7,    3,   29,            3,    6,    3,   10,    // 15
      30,    4,   18,    4,            3,    3,    4,    3,    // 23
      28,   41,   51,    4,            7,    3,   29,    3,    // 31
       6,    3,    5,   30,            4,   15,    4,    3,    // 39
       3,    4,    3,   28,           11,   51,    4,    7,    // 47
       3,    1,    3,    1,            8,    3,   31,  -49,    // 55
      51,    4,    7,    3,            1,    3,    1,    8,    // 63
       3,   31,  -60,    2,            1,    4,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, For5) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int Start()                     \n\
        {                               \n\
            return 1;                   \n\
        }                               \n\
        int Check()                     \n\
        {                               \n\
            return 10;                  \n\
        }                               \n\
        int Cont(int x)                 \n\
        {                               \n\
            return x+1;                 \n\
        }                               \n\
                                        \n\
        void main()                     \n\
        {                               \n\
            for(int i = Start(); i < Check(); i = Cont(i))   \n\
                if (i >= 0)             \n\
                    continue;           \n\
        }                               \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("For5", scrip);
    const size_t codesize = 140;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            1,   31,    3,    6,    // 7
       3,    0,    5,   38,           11,    6,    3,   10,    // 15
      31,    3,    6,    3,            0,    5,   38,   22,    // 23
      51,    8,    7,    3,           29,    3,    6,    3,    // 31
       1,   30,    4,   11,            4,    3,    3,    4,    // 39
       3,   31,    3,    6,            3,    0,    5,   38,    // 47
      47,    6,    3,    0,           23,    3,   29,    3,    // 55
      51,    4,    7,    3,           29,    3,    6,    3,    // 63
      11,   23,    3,   30,            4,   18,    4,    3,    // 71
       3,    4,    3,   28,           59,   51,    4,    7,    // 79
       3,   29,    3,    6,            3,    0,   30,    4,    // 87
      19,    4,    3,    3,            4,    3,   28,   20,    // 95
      51,    4,    7,    3,           29,    3,    6,    3,    // 103
      22,   23,    3,    2,            1,    4,   51,    4,    // 111
       8,    3,   31,  -60,           51,    4,    7,    3,    // 119
      29,    3,    6,    3,           22,   23,    3,    2,    // 127
       1,    4,   51,    4,            8,    3,   31,  -80,    // 135
       2,    1,    4,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 4;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      51,   64,  104,  124,        -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,   2,   2,   2,     '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, For6) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        void main()                     \n\
        {                               \n\
            for(int i = Start();        \n\
                i < Check();            \n\
                i = Cont(i))            \n\
                if (i >= 0)             \n\
                    continue;           \n\
        }                               \n\
        int Start()                     \n\
        {                               \n\
            return 1;                   \n\
        }                               \n\
        int Check()                     \n\
        {                               \n\
            return 10;                  \n\
        }                               \n\
        int Cont(int x)                 \n\
        {                               \n\
            return x + 1;               \n\
        }                               \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("For6", scrip);
    const size_t codesize = 140;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           93,   23,    3,   29,    // 7
       3,   51,    4,    7,            3,   29,    3,    6,    // 15
       3,  104,   23,    3,           30,    4,   18,    4,    // 23
       3,    3,    4,    3,           28,   59,   51,    4,    // 31
       7,    3,   29,    3,            6,    3,    0,   30,    // 39
       4,   19,    4,    3,            3,    4,    3,   28,    // 47
      20,   51,    4,    7,            3,   29,    3,    6,    // 55
       3,  115,   23,    3,            2,    1,    4,   51,    // 63
       4,    8,    3,   31,          -60,   51,    4,    7,    // 71
       3,   29,    3,    6,            3,  115,   23,    3,    // 79
       2,    1,    4,   51,            4,    8,    3,   31,    // 87
     -80,    2,    1,    4,            5,   38,   93,    6,    // 95
       3,    1,   31,    3,            6,    3,    0,    5,    // 103
      38,  104,    6,    3,           10,   31,    3,    6,    // 111
       3,    0,    5,   38,          115,   51,    8,    7,    // 119
       3,   29,    3,    6,            3,    1,   30,    4,    // 127
      11,    4,    3,    3,            4,    3,   31,    3,    // 135
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 4;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   17,   57,   77,        -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,   2,   2,   2,     '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, For7) {
    ccCompiledScript *scrip = newScriptFixture();

    // Initializer and iterator of a for() need not be assignments,
    // they can be func calls.

    char *inpl = "\
        int i;                          \n\
        void main()                     \n\
        {                               \n\
            for(Start(); Check(); Cont())   \n\
                if (i >= 5)             \n\
                    i = 100 - i;        \n\
        }                               \n\
        short Start()                   \n\
        {                               \n\
            i = 1;                      \n\
            return -77;                 \n\
        }                               \n\
        int Check()                     \n\
        {                               \n\
            return i < 10;              \n\
        }                               \n\
        void Cont()                     \n\
        {                               \n\
            i++;                        \n\
        }                               \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("For7", scrip);
    const size_t codesize = 123;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           65,   23,    3,    6,    // 7
       3,   84,   23,    3,           28,   50,    6,    2,    // 15
       0,    7,    3,   29,            3,    6,    3,    5,    // 23
      30,    4,   19,    4,            3,    3,    4,    3,    // 31
      28,   23,    6,    3,          100,   29,    3,    6,    // 39
       2,    0,    7,    3,           30,    4,   12,    4,    // 47
       3,    3,    4,    3,            6,    2,    0,    8,    // 55
       3,    6,    3,  110,           23,    3,   31,  -57,    // 63
       5,   38,   65,    6,            3,    1,    6,    2,    // 71
       0,    8,    3,    6,            3,  -77,   31,    3,    // 79
       6,    3,    0,    5,           38,   84,    6,    2,    // 87
       0,    7,    3,   29,            3,    6,    3,   10,    // 95
      30,    4,   18,    4,            3,    3,    4,    3,    // 103
      31,    3,    6,    3,            0,    5,   38,  110,    // 111
       6,    2,    0,    7,            3,    1,    3,    1,    // 119
       8,    3,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 9;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,    9,   16,   41,         54,   59,   72,   88,    // 7
     114,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,   2,   1,   1,      1,   2,   1,   1,    // 7
      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, Continue1) {
    ccCompiledScript *scrip = newScriptFixture();

    // Locals only become invalid at the end of their nesting; below a "continue", they
    // remain valid so the offset to start of the local block
    // must not be reduced.

    char *inpl = "\
        int main()                      \n\
        {                               \n\
            int I;                      \n\
            for(I = -1; I < 1; I++)     \n\
            {                           \n\
                int A = 7;              \n\
                int B = 77;             \n\
                if (I >= 0)             \n\
                    continue;           \n\
                int C = A;              \n\
            }                           \n\
            return I;                   \n\
        }                               \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Continue1", scrip);
    const size_t codesize = 114;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           63,    4,    1,    1,    // 7
       4,    6,    3,   -1,           51,    4,    8,    3,    // 15
      51,    4,    7,    3,           29,    3,    6,    3,    // 23
       1,   30,    4,   18,            4,    3,    3,    4,    // 31
       3,   28,   63,    6,            3,    7,   29,    3,    // 39
       6,    3,   77,   29,            3,   51,   12,    7,    // 47
       3,   29,    3,    6,            3,    0,   30,    4,    // 55
      19,    4,    3,    3,            4,    3,   28,   14,    // 63
       2,    1,    8,   51,            4,    7,    3,    1,    // 71
       3,    1,    8,    3,           31,  -62,   51,    8,    // 79
       7,    3,   29,    3,            2,    1,   12,   51,    // 87
       4,    7,    3,    1,            3,    1,    8,    3,    // 95
      31,  -82,   51,    4,            7,    3,    2,    1,    // 103
       4,   31,    6,    2,            1,    4,    6,    3,    // 111
       0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, IfDoWhile) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    int Foo(int i, float f)                      \n\
    {                                            \n\
        int five = 5, sum, loop = -2;            \n\
        if (five < 10)                           \n\
            for (loop = 0; loop < 10; loop += 3) \n\
            {                                    \n\
                sum += loop;                     \n\
                if (loop == 6) return loop;      \n\
            }                                    \n\
        else                                     \n\
            do { loop += 1; } while (loop < 100);   \n\
        return 0;                                \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("IfDoWhile", scrip);
    const size_t codesize = 179;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   29,    3,   51,    // 7
       0,   63,    4,    1,            1,    4,    6,    3,    // 15
      -2,   29,    3,   51,           12,    7,    3,   29,    // 23
       3,    6,    3,   10,           30,    4,   18,    4,    // 31
       3,    3,    4,    3,           28,   91,    6,    3,    // 39
       0,   51,    4,    8,            3,   51,    4,    7,    // 47
       3,   29,    3,    6,            3,   10,   30,    4,    // 55
      18,    4,    3,    3,            4,    3,   28,   63,    // 63
      51,    4,    7,    3,           29,    3,   51,   12,    // 71
       7,    3,   30,    4,           11,    3,    4,    8,    // 79
       3,   51,    4,    7,            3,   29,    3,    6,    // 87
       3,    6,   30,    4,           15,    4,    3,    3,    // 95
       4,    3,   28,    9,           51,    4,    7,    3,    // 103
       2,    1,   12,   31,           69,    6,    3,    3,    // 111
      29,    3,   51,    8,            7,    3,   30,    4,    // 119
      11,    3,    4,    8,            3,   31,  -82,   31,    // 127
      35,    6,    3,    1,           29,    3,   51,    8,    // 135
       7,    3,   30,    4,           11,    3,    4,    8,    // 143
       3,   51,    4,    7,            3,   29,    3,    6,    // 151
       3,  100,   30,    4,           18,    4,    3,    3,    // 159
       4,    3,   70,  -35,            6,    3,    0,    2,    // 167
       1,   12,   31,    6,            2,    1,   12,    6,    // 175
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Switch01) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    int Foo(int i, float f)         \n\
    {                               \n\
        switch (i * i)              \n\
        {                           \n\
        case 2: return 10; break;   \n\
        default: i *= 2; return i;  \n\
        case 3:                     \n\
        case 4: i = 0;              \n\
        case 5: i += 5 - i - 4;  break; \n\
        }                           \n\
        return 0;                   \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Switch01", scrip);
    const size_t codesize = 165;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   29,    3,    // 7
      51,   12,    7,    3,           30,    4,    9,    4,    // 15
       3,    3,    4,    3,            3,    3,    4,   31,    // 23
      81,    6,    3,   10,           31,  134,   31,  124,    // 31
       6,    3,    2,   29,            3,   51,   12,    7,    // 39
       3,   30,    4,    9,            3,    4,    8,    3,    // 47
      51,    8,    7,    3,           31,  110,    6,    3,    // 55
       0,   51,    8,    8,            3,    6,    3,    5,    // 63
      29,    3,   51,   12,            7,    3,   30,    4,    // 71
      12,    4,    3,    3,            4,    3,   29,    3,    // 79
       6,    3,    4,   30,            4,   12,    4,    3,    // 87
       3,    4,    3,   29,            3,   51,   12,    7,    // 95
       3,   30,    4,   11,            3,    4,    8,    3,    // 103
      31,   50,   29,    4,            6,    3,    2,   30,    // 111
       4,   16,    3,    4,           28,  -93,   29,    4,    // 119
       6,    3,    3,   30,            4,   16,    3,    4,    // 127
      28,  -76,   29,    4,            6,    3,    4,   30,    // 135
       4,   16,    3,    4,           28,  -88,   29,    4,    // 143
       6,    3,    5,   30,            4,   16,    3,    4,    // 151
      28,  -93,   31, -124,            6,    3,    0,   31,    // 159
       3,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, FreeLocalPtr) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct S                  \n\
    {                                 \n\
        int i;                        \n\
    };                                \n\
                                      \n\
    int foo()                         \n\
    {                                 \n\
        S *sptr = new S;              \n\
                                      \n\
        for (int i = 0; i < 10; i++)  \n\
            sptr = new S;             \n\
    }                                 \n\
    ";
    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("FreeLocalPtr", scrip);
    const size_t codesize = 67;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   73,    3,            4,   51,    0,   47,    // 7
       3,    1,    1,    4,            6,    3,    0,   29,    // 15
       3,   51,    4,    7,            3,   29,    3,    6,    // 23
       3,   10,   30,    4,           18,    4,    3,    3,    // 31
       4,    3,   28,   18,           73,    3,    4,   51,    // 39
       8,   47,    3,   51,            4,    7,    3,    1,    // 47
       3,    1,    8,    3,           31,  -37,    2,    1,    // 55
       4,   51,    4,   49,            2,    1,    4,    6,    // 63
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, StringOldstyle01) {
    ccSetOption(SCOPT_OLDSTRINGS, true);
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int Sentinel1;              \n\
        string GLOBAL;              \n\
        int Sentinel2;              \n\
                                    \n\
        string MyFunction(int a)    \n\
        {                           \n\
            string x;               \n\
            char   Sentinel3;       \n\
            return GLOBAL;          \n\
        }                           \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("StringOldstyle01", scrip);
    const size_t codesize = 34;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
          38,    0,   51,    0,           63,  200,    1,    1,    // 7
         200,   51,    0,   63,            1,    1,    1,    1,    // 15
           6,    2,    4,    3,            2,    3,    2,    1,    // 23
         201,   31,    6,    2,            1,  201,    6,    3,    // 31
           0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      18,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, StringOldstyle02) {

    ccSetOption(SCOPT_OLDSTRINGS, true);
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int sub(const string s) \n\
        {                       \n\
            return;             \n\
        }                       \n\
                                \n\
        int main()              \n\
        {                       \n\
            sub(\"Foo\");       \n\
        }                       \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("StringOldstyle02", scrip);
    const size_t codesize = 30;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   31,    3,    6,    // 7
       3,    0,    5,   38,           11,    6,    3,    0,    // 15
      29,    3,    6,    3,            0,   23,    3,    2,    // 23
       1,    4,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      15,   20,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      3,   2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 4;
    EXPECT_EQ(stringssize, scrip->stringssize);

    char strings[] = {
    'F',  'o',  'o',    0,          '\0'
    };

    for (size_t idx = 0; idx < stringssize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->stringssize) break;
        std::string prefix = "strings[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(strings[idx]);
        std::string test_val = prefix + std::to_string(scrip->strings[idx]);
        ASSERT_EQ(is_val, test_val);
    }
}

TEST(Bytecode, Struct01) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    	struct Struct                       \n\
		{                                   \n\
			float Float;                    \n\
			import int[] Func(int i);       \n\
		};                                  \n\
                                            \n\
		int[] Struct::Func(int i)           \n\
		{                                   \n\
			int Ret[];                      \n\
			this.Float = 0.0;               \n\
			Ret = new int[5];               \n\
			Ret[3] = 77;                    \n\
			return Ret;                     \n\
		}                                   \n\
                                            \n\
		void main()                         \n\
		{                                   \n\
			Struct S;                       \n\
			int I[] = S.Func(-1);           \n\
			int J = I[3];                   \n\
		}                                   \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct01", scrip);
    const size_t codesize = 140;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           49,    1,    1,    4,    // 7
       6,    3,    0,    3,            6,    2,   52,    8,    // 15
       3,    6,    3,    5,           72,    3,    4,    0,    // 23
      51,    4,   47,    3,            6,    3,   77,   29,    // 31
       3,   51,    8,   48,            2,   52,    1,    2,    // 39
      12,   30,    3,    8,            3,   51,    4,   48,    // 47
       3,   29,    3,   51,            4,   47,    3,   51,    // 55
       8,   49,   51,    4,           48,    3,   69,   30,    // 63
       4,    2,    1,    4,           31,    9,   51,    4,    // 71
      49,    2,    1,    4,            6,    3,    0,    5,    // 79
      38,   80,   51,    0,           63,    4,    1,    1,    // 87
       4,   51,    4,   29,            2,    6,    3,   -1,    // 95
      29,    3,   51,    8,            7,    2,   45,    2,    // 103
       6,    3,    0,   23,            3,    2,    1,    4,    // 111
      30,    2,   51,    0,           47,    3,    1,    1,    // 119
       4,   51,    4,   48,            2,   52,    1,    2,    // 127
      12,    7,    3,   29,            3,   51,    8,   49,    // 135
       2,    1,   12,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
     106,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct02) {
    ccCompiledScript *scrip = newScriptFixture();

    // test arrays; arrays in structs;
    // whether the namespace in structs is independent of the global namespace

    char *inpl = "\
    struct Struct1                  \n\
    {                               \n\
        int Array[17], Ix;          \n\
    };                              \n\
                                    \n\
    Struct1 S;                      \n\
    int Array[5];                   \n\
                                    \n\
    void main()                     \n\
    {                               \n\
        S.Ix = 5;                   \n\
        Array[2] = 3;               \n\
        S.Array[Array[2]] = 42;     \n\
        S.Array[S.Ix] = 19;         \n\
        return;                     \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct02", scrip);
    const size_t codesize = 85;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,    6,    2,   68,    // 7
       8,    3,    6,    3,            3,   29,    3,    6,    // 15
       2,   80,   30,    3,            8,    3,    6,    3,    // 23
      42,   29,    3,    6,            2,    0,   29,    2,    // 31
       6,    2,   80,    7,            3,   30,    2,   46,    // 39
       3,   17,   32,    3,            4,   11,    2,    3,    // 47
      30,    3,    8,    3,            6,    3,   19,   29,    // 55
       3,    6,    2,    0,           29,    2,    6,    2,    // 63
      68,    7,    3,   30,            2,   46,    3,   17,    // 71
      32,    3,    4,   11,            2,    3,   30,    3,    // 79
       8,    3,   31,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 6;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       7,   17,   29,   34,         59,   64,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   1,      1,   1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct03) {
    ccCompiledScript *scrip = newScriptFixture();

    // test arrays; arrays in structs;
    // whether the namespace in structs is independent of the global namespace

    char *inpl = "\
    struct Struct1                  \n\
    {                               \n\
        int Array[17], Ix;          \n\
    } S;                            \n\
    int Array[5];                   \n\
                                    \n\
    void main()                     \n\
    {                               \n\
        S.Ix = 5;                   \n\
        Array[2] = 3;               \n\
        S.Array[Array[2]] = 42;     \n\
        S.Array[S.Ix] = 19;         \n\
        return;                     \n\
    }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct03", scrip);
    const size_t codesize = 85;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,    6,    2,   68,    // 7
       8,    3,    6,    3,            3,   29,    3,    6,    // 15
       2,   80,   30,    3,            8,    3,    6,    3,    // 23
      42,   29,    3,    6,            2,    0,   29,    2,    // 31
       6,    2,   80,    7,            3,   30,    2,   46,    // 39
       3,   17,   32,    3,            4,   11,    2,    3,    // 47
      30,    3,    8,    3,            6,    3,   19,   29,    // 55
       3,    6,    2,    0,           29,    2,    6,    2,    // 63
      68,    7,    3,   30,            2,   46,    3,   17,    // 71
      32,    3,    4,   11,            2,    3,   30,    3,    // 79
       8,    3,   31,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 6;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       7,   17,   29,   34,         59,   64,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   1,      1,   1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct04) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        managed struct StructI                               \n\
        {                                                    \n\
            int k;                                           \n\
        };                                                   \n\
                                                             \n\
        struct StructO                                       \n\
        {                                                    \n\
            StructI *SI;                                     \n\
            StructI *SJ[3];                                  \n\
        };                                                   \n\
                                                             \n\
        int main()                                           \n\
        {                                                    \n\
            StructO SO;                                      \n\
            SO.SI = new StructI;                             \n\
            SO.SI.k = 12345;                                 \n\
            StructO SOA[3];                                  \n\
            SOA[2].SI = new StructI;                         \n\
        }                                                    \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct04", scrip);
    const size_t codesize = 104;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           63,   16,    1,    1,    // 7
      16,   73,    3,    4,           51,   16,   47,    3,    // 15
       6,    3, 12345,   29,            3,   51,   20,   48,    // 23
       2,   52,   30,    3,            8,    3,   51,    0,    // 31
      63,   48,    1,    1,           48,   73,    3,    4,    // 39
      29,    3,   51,   20,           30,    3,   47,    3,    // 47
      51,   64,   49,    1,            2,    4,   49,    1,    // 55
       2,    4,   49,    1,            2,    4,   49,   51,    // 63
      48,    6,    3,    3,           29,    2,   29,    3,    // 71
      49,    1,    2,    4,           49,    1,    2,    4,    // 79
      49,    1,    2,    4,           49,   30,    3,   30,    // 87
       2,    1,    2,   16,            2,    3,    1,   70,    // 95
     -29,    2,    1,   64,            6,    3,    0,    5,    // 103
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct05) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        struct StructO                                       \n\
        {                                                    \n\
            static import int StInt(int i);                  \n\
        };                                                   \n\
        StructO        S1;                                   \n\
                                                             \n\
        int main()                                           \n\
        {                                                    \n\
             StructO        S2;                              \n\
             return S1.StInt(S2.StInt(7));                   \n\
        }                                                    \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct05", scrip);
    const size_t codesize = 40;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           63,    0,    1,    1,    // 7
       0,    6,    3,    7,           34,    3,   39,    1,    // 15
       6,    3,    0,   33,            3,   35,    1,   34,    // 23
       3,   39,    1,    6,            3,    0,   33,    3,    // 31
      35,    1,   31,    3,            6,    3,    0,    5,    // 39
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      18,   29,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "StructO::StInt^1",            "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, Struct06) {
    ccCompiledScript *scrip = newScriptFixture();

    // NOTE: S1.Array[3] is null, so S1.Array[3].Payload should dump
    // when executed in real.

    char *inpl = "\
        managed struct Struct0;                             \n\
                                                            \n\
        struct Struct1                                      \n\
        {                                                   \n\
            Struct0 *Array[];                               \n\
        };                                                  \n\
                                                            \n\
        managed struct Struct0                              \n\
        {                                                   \n\
            int Payload;                                    \n\
        };                                                  \n\
                                                            \n\
        int main()                                          \n\
        {                                                   \n\
            Struct1 S1;                                     \n\
            S1.Array = new Struct0[5];                      \n\
            S1.Array[3].Payload = 77;                       \n\
        }                                                   \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct06", scrip);
    const size_t codesize = 50;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           63,    4,    1,    1,    // 7
       4,    6,    3,    5,           72,    3,    4,    1,    // 15
      51,    4,   47,    3,            6,    3,   77,   29,    // 23
       3,   51,    8,   48,            2,   52,    1,    2,    // 31
      12,   48,    2,   52,           30,    3,    8,    3,    // 39
      51,    4,   49,    2,            1,    4,    6,    3,    // 47
       0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct07) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        struct Struct1                                       \n\
        {                                                    \n\
            int IPayload;                                    \n\
            char CPayload[3];                                \n\
        };                                                   \n\
                                                             \n\
        Struct1 S1[3];                                       \n\
                                                             \n\
        int main()                                           \n\
        {                                                    \n\
            S1[1].IPayload = 0;                              \n\
            S1[1].CPayload[0] = 'A';                         \n\
            S1[1].CPayload[1] = S1[1].CPayload[0] - 'A';     \n\
            S1[1].CPayload[0] --;                            \n\
            return 0;                                        \n\
        }                                                    \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct07", scrip);
    const size_t codesize = 72;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   29,    3,    6,    // 7
       2,    8,   30,    3,            8,    3,    6,    3,    // 15
      65,   29,    3,    6,            2,   12,   30,    3,    // 23
      26,    3,    6,    2,           12,   24,    3,   29,    // 31
       3,    6,    3,   65,           30,    4,   12,    4,    // 39
       3,    3,    4,    3,           29,    3,    6,    2,    // 47
      13,   30,    3,   26,            3,    6,    2,   12,    // 55
      24,    3,    2,    3,            1,   26,    3,    6,    // 63
       3,    0,   31,    3,            6,    3,    0,    5,    // 71
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 5;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       9,   21,   28,   48,         55,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   1,      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct08) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        struct Struct                                        \n\
        {                                                    \n\
            int k;                                           \n\
        };                                                   \n\
                                                             \n\
        struct Sub extends Struct                            \n\
        {                                                    \n\
            int l;                                           \n\
        };                                                   \n\
                                                             \n\
        int Func(this Sub *, int i, int j)                   \n\
        {                                                    \n\
            return !i || !(j) && this.k || (0 != this.l);    \n\
        }                                                    \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct08", scrip);
    const size_t codesize = 84;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   42,    3,    // 7
      70,   34,   29,    3,           51,   16,    7,    3,    // 15
      42,    3,   28,   16,           29,    3,    3,    6,    // 23
       2,   52,    7,    3,           30,    4,   21,    4,    // 31
       3,    3,    4,    3,           30,    4,   22,    4,    // 39
       3,    3,    4,    3,           70,   32,   29,    3,    // 47
       6,    3,    0,   29,            3,    3,    6,    2,    // 55
      52,    1,    2,    4,            7,    3,   30,    4,    // 63
      16,    4,    3,    3,            4,    3,   30,    4,    // 71
      22,    4,    3,    3,            4,    3,   31,    3,    // 79
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func01) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct Struct1          \n\
    {                               \n\
        float Payload1;             \n\
    };                              \n\
    managed struct Struct2          \n\
    {                               \n\
        char Payload2;              \n\
    };                              \n\
                                    \n\
    import int Func(Struct1 *S1, Struct2 *S2);  \n\
                                    \n\
    int main()                      \n\
    {                               \n\
        Struct1 *SS1;               \n\
        Struct2 *SS2;               \n\
        int Ret = Func(SS1, SS2);   \n\
        return Ret;                 \n\
    }                               \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func01", scrip);
    const size_t codesize = 65;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           49,    1,    1,    4,    // 7
      51,    0,   49,    1,            1,    4,   51,    4,    // 15
      48,    3,   34,    3,           51,    8,   48,    3,    // 23
      34,    3,   39,    2,            6,    3,    0,   33,    // 31
       3,   35,    2,   29,            3,   51,    4,    7,    // 39
       3,   51,   12,   49,           51,    8,   49,    2,    // 47
       1,   12,   31,   12,           51,   12,   49,   51,    // 55
       8,   49,    2,    1,           12,    6,    3,    0,    // 63
       5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      30,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "Func",         "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func02) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct Struct1          \n\
    {                               \n\
        float Payload1;             \n\
    };                              \n\
    managed struct Struct2          \n\
    {                               \n\
        char Payload2;              \n\
    };                              \n\
                                    \n\
    int main()                      \n\
    {                               \n\
        Struct1 *SS1;               \n\
        Struct2 *SS2;               \n\
        int Ret = Func(SS1, SS2);   \n\
        return Ret;                 \n\
    }                               \n\
                                    \n\
    import int Func(Struct1 *S1, Struct2 *S2);  \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func02", scrip);
    const size_t codesize = 65;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           49,    1,    1,    4,    // 7
      51,    0,   49,    1,            1,    4,   51,    4,    // 15
      48,    3,   34,    3,           51,    8,   48,    3,    // 23
      34,    3,   39,    2,            6,    3,    0,   33,    // 31
       3,   35,    2,   29,            3,   51,    4,    7,    // 39
       3,   51,   12,   49,           51,    8,   49,    2,    // 47
       1,   12,   31,   12,           51,   12,   49,   51,    // 55
       8,   49,    2,    1,           12,    6,    3,    0,    // 63
       5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      30,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "Func",         "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func03) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct Struct1          \n\
    {                               \n\
        float Payload1;             \n\
    };                              \n\
    managed struct Struct2          \n\
    {                               \n\
        char Payload2;              \n\
    };                              \n\
                                    \n\
    import int Func(Struct1 *S1, Struct2 *S2);  \n\
                                    \n\
    int Func(Struct1 *S1, Struct2 *S2)  \n\
    {                               \n\
        return 0;                   \n\
    }                               \n\
                                    \n\
    int main()                      \n\
    {                               \n\
        Struct1 *SS1;               \n\
        Struct2 *SS2;               \n\
        int Ret = Func(SS1, SS2);   \n\
        return Ret;                 \n\
    }                               \n\
   ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func03", scrip);
    const size_t codesize = 93;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   47,    3,    // 7
      51,   12,    7,    3,           47,    3,    6,    3,    // 15
       0,   51,    8,   49,           51,   12,   49,   31,    // 23
       3,    6,    3,    0,            5,   38,   29,   51,    // 31
       0,   49,    1,    1,            4,   51,    0,   49,    // 39
       1,    1,    4,   51,            4,   48,    3,   29,    // 47
       3,   51,   12,   48,            3,   29,    3,    6,    // 55
       3,    0,   23,    3,            2,    1,    8,   29,    // 63
       3,   51,    4,    7,            3,   51,   12,   49,    // 71
      51,    8,   49,    2,            1,   12,   31,   12,    // 79
      51,   12,   49,   51,            8,   49,    2,    1,    // 87
      12,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      57,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func04) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct Struct1          \n\
    {                               \n\
        float Payload1;             \n\
    };                              \n\
                                    \n\
    int main()                      \n\
    {                               \n\
        Struct1 *SS1 = Func(5);     \n\
        return -1;                  \n\
    }                               \n\
                                    \n\
    Struct1 *Func(int Int)          \n\
    {                               \n\
        return new Struct1;         \n\
    }                               \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func04", scrip);
    const size_t codesize = 54;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   29,    3,    6,    // 7
       3,   43,   23,    3,            2,    1,    4,   51,    // 15
       0,   47,    3,    1,            1,    4,    6,    3,    // 23
      -1,   51,    4,   49,            2,    1,    4,   31,    // 31
       9,   51,    4,   49,            2,    1,    4,    6,    // 39
       3,    0,    5,   38,           43,   73,    3,    4,    // 47
      31,    3,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       9,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func05) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        import int Func(int, int = 5); \n\
                                     \n\
        int Func(int P1, int P2)     \n\
        {                            \n\
            return P1 + P2;          \n\
        }                            \n\
                                     \n\
        void main()                  \n\
        {                            \n\
            int Int = Func(4);       \n\
        }                            \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func05", scrip);
    const size_t codesize = 52;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   29,    3,    // 7
      51,   16,    7,    3,           30,    4,   11,    4,    // 15
       3,    3,    4,    3,           31,    3,    6,    3,    // 23
       0,    5,   38,   26,            6,    3,    5,   29,    // 31
       3,    6,    3,    4,           29,    3,    6,    3,    // 39
       0,   23,    3,    2,            1,    8,   29,    3,    // 47
       2,    1,    4,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      40,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func06) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        import int Func(int, int = 5); \n\
                                     \n\
        void main()                  \n\
        {                            \n\
            int Int1 = Func(4);      \n\
            int Int2 = Func(4, 1);   \n\
        }                            \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func06", scrip);
    const size_t codesize = 48;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   34,    3,    6,    // 7
       3,    4,   34,    3,           39,    2,    6,    3,    // 15
       0,   33,    3,   35,            2,   29,    3,    6,    // 23
       3,    1,   34,    3,            6,    3,    4,   34,    // 31
       3,   39,    2,    6,            3,    0,   33,    3,    // 39
      35,    2,   29,    3,            2,    1,    8,    5,    // 47
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      16,   37,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "Func",         "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func07) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        void main()                  \n\
        {                            \n\
            int Int1 = Func(4);      \n\
            int Int2 = Func(4, 1);   \n\
        }                            \n\
                                     \n\
        import int Func(int, int = 5); \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func07", scrip);
    const size_t codesize = 48;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   34,    3,    6,    // 7
       3,    4,   34,    3,           39,    2,    6,    3,    // 15
       0,   33,    3,   35,            2,   29,    3,    6,    // 23
       3,    1,   34,    3,            6,    3,    4,   34,    // 31
       3,   39,    2,    6,            3,    0,   33,    3,    // 39
      35,    2,   29,    3,            2,    1,    8,    5,    // 47
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      16,   37,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "Func",         "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func08) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        import int Func(int f, int = 5); \n\
        import int Func(int, int = 5); \n\
                                     \n\
        void main()                  \n\
        {                            \n\
            int Int1 = Func(4);      \n\
            int Int2 = Func(4, 1);   \n\
        }                            \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func08", scrip);
    const size_t codesize = 48;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   34,    3,    6,    // 7
       3,    4,   34,    3,           39,    2,    6,    3,    // 15
       0,   33,    3,   35,            2,   29,    3,    6,    // 23
       3,    1,   34,    3,            6,    3,    4,   34,    // 31
       3,   39,    2,    6,            3,    0,   33,    3,    // 39
      35,    2,   29,    3,            2,    1,    8,    5,    // 47
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      16,   37,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "Func",         "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func09) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        import int Func(int, int = 5); \n\
                                     \n\
        int Func(int P1, int P2)     \n\
        {                            \n\
            return P1 + P2;          \n\
        }                            \n\
                                     \n\
        void main()                  \n\
        {                            \n\
            int Int = Func(4,-99);   \n\
        }                            \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func09", scrip);
    const size_t codesize = 52;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   29,    3,    // 7
      51,   16,    7,    3,           30,    4,   11,    4,    // 15
       3,    3,    4,    3,           31,    3,    6,    3,    // 23
       0,    5,   38,   26,            6,    3,  -99,   29,    // 31
       3,    6,    3,    4,           29,    3,    6,    3,    // 39
       0,   23,    3,    2,            1,    8,   29,    3,    // 47
       2,    1,    4,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      40,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func10) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    struct Struct                   \n\
    {                               \n\
        float Float;                \n\
        int Func();                 \n\
    };                              \n\
                                    \n\
    int Struct::Func()              \n\
    {                               \n\
        return 5;                   \n\
    }                               \n\
                                    \n\
    int main()                      \n\
    {                               \n\
        Struct s;                   \n\
        int Int = s.Func() % 3;     \n\
        return Int;                 \n\
    }                               \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func10", scrip);
    const size_t codesize = 60;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   31,    3,    6,    // 7
       3,    0,    5,   38,           11,   51,    0,   63,    // 15
       4,    1,    1,    4,           51,    4,   45,    2,    // 23
       6,    3,    0,   23,            3,   29,    3,    6,    // 31
       3,    3,   30,    4,           40,    4,    3,    3,    // 39
       4,    3,   29,    3,           51,    4,    7,    3,    // 47
       2,    1,    8,   31,            6,    2,    1,    8,    // 55
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      26,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Export) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    struct Struct                   \n\
    {                               \n\
        float Float;                \n\
        int Int;                    \n\
    };                              \n\
    Struct StructyStructy;          \n\
    export StructyStructy;          \n\
                                    \n\
    int Inty;                       \n\
    float Floaty;                   \n\
    export Floaty, Inty;            \n\
                                    \n\
    int main()                      \n\
    {                               \n\
        Struct s;                   \n\
        s.Int = 3;                  \n\
        s.Float = 1.1 / 2.2;        \n\
        return -2;                  \n\
    }                               \n\
    export main;                    \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Export", scrip);
    const size_t codesize = 51;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           63,    8,    1,    1,    // 7
       8,    6,    3,    3,           51,    4,    8,    3,    // 15
       6,    3, 1066192077,   29,            3,    6,    3, 1074580685,    // 23
      30,    4,   56,    4,            3,    3,    4,    3,    // 31
      51,    8,    8,    3,            6,    3,   -2,    2,    // 39
       1,    8,   31,    6,            2,    1,    8,    6,    // 47
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 4;
    EXPECT_EQ(numexports, scrip->numexports);

    std::string exports[] = {
    "StructyStructy", "Floaty",   "Inty",     "main$0",   // 3
     "[[SENTINEL]]"
    };

    for (size_t idx = 0; idx < numexports; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numexports) break;
        std::string prefix = "exports[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + exports[idx];
        std::string test_val = prefix + scrip->exports[idx];
        ASSERT_EQ(is_val, test_val);
    }

    int32_t export_addr[] = {
    0x2000000, 0x200000c,    0x2000008, 0x1000000, // 3
     0
    };

    for (size_t idx = 0; idx < numexports; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numexports) break;
        std::string prefix = "export_addr[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(export_addr[idx]);
        std::string test_val = prefix + std::to_string(scrip->export_addr[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, ArrayOfPointers1) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct Struct                \n\
    {                                    \n\
        float Float;                     \n\
        protected int Int;               \n\
    };                                   \n\
    Struct *arr[50];                     \n\
                                         \n\
    int main()                           \n\
    {                                    \n\
        for (int i = 0; i < 9; i++)      \n\
            arr[i] = new Struct;         \n\
                                         \n\
        int test = (arr[10] == null);    \n\
    }                                    \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("ArrayOfPointers1", scrip);
    const size_t codesize = 96;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   29,    3,   51,    // 7
       4,    7,    3,   29,            3,    6,    3,    9,    // 15
      30,    4,   18,    4,            3,    3,    4,    3,    // 23
      28,   40,   73,    3,            8,   29,    3,    6,    // 31
       2,    0,   29,    2,           51,   12,    7,    3,    // 39
      30,    2,   46,    3,           50,   32,    3,    4,    // 47
      11,    2,    3,   30,            3,   47,    3,   51,    // 55
       4,    7,    3,    1,            3,    1,    8,    3,    // 63
      31,  -59,    2,    1,            4,    6,    2,   40,    // 71
      48,    3,   29,    3,            6,    3,    0,   30,    // 79
       4,   15,    4,    3,            3,    4,    3,   29,    // 87
       3,    2,    1,    4,            6,    3,    0,    5,    // 95
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      33,   71,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, ArrayOfPointers2) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct Struct                \n\
    {                                    \n\
        float Float;                     \n\
        protected int Int;               \n\
    };                                   \n\
                                         \n\
    int main()                           \n\
    {                                    \n\
        Struct *arr2[50];                \n\
        for (int i = 0; i < 20; i++) {   \n\
                arr2[i] = new Struct;    \n\
        }                                \n\
        arr2[5].Float = 2.2 - 0.0 * 3.3; \n\
        arr2[4] = null;                  \n\
    }                                    \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("ArrayOfPointers2", scrip);
    const size_t codesize = 147;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           63,  200,    1,    1,    // 7
     200,    6,    3,    0,           29,    3,   51,    4,    // 15
       7,    3,   29,    3,            6,    3,   20,   30,    // 23
       4,   18,    4,    3,            3,    4,    3,   28,    // 31
      39,   73,    3,    8,           29,    3,   51,  208,    // 39
      29,    2,   51,   12,            7,    3,   30,    2,    // 47
      46,    3,   50,   32,            3,    4,   11,    2,    // 55
       3,   30,    3,   47,            3,   51,    4,    7,    // 63
       3,    1,    3,    1,            8,    3,   31,  -58,    // 71
       2,    1,    4,    6,            3, 1074580685,   29,    3,    // 79
       6,    3,    0,   29,            3,    6,    3, 1079194419,    // 87
      30,    4,   55,    4,            3,    3,    4,    3,    // 95
      30,    4,   58,    4,            3,    3,    4,    3,    // 103
      29,    3,   51,  184,           48,    2,   52,   30,    // 111
       3,    8,    3,    6,            3,    0,   29,    3,    // 119
      51,  188,   30,    3,           47,    3,   51,  200,    // 127
       6,    3,   50,   49,            1,    2,    4,    2,    // 135
       3,    1,   70,   -9,            2,    1,  200,    6,    // 143
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, ArrayInStruct1) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    managed struct Struct                \n\
    {                                    \n\
        int Int[10];                     \n\
    };                                   \n\
                                         \n\
    int main()                           \n\
    {                                    \n\
        Struct *S = new Struct;          \n\
        S.Int[4] =  1;                   \n\
    }                                    \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("ArrayInStruct1", scrip);
    const size_t codesize = 39;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   73,    3,           40,   51,    0,   47,    // 7
       3,    1,    1,    4,            6,    3,    1,   29,    // 15
       3,   51,    8,   48,            2,   52,    1,    2,    // 23
      16,   30,    3,    8,            3,   51,    4,   49,    // 31
       2,    1,    4,    6,            3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, ArrayInStruct2) {
    ccCompiledScript *scrip = newScriptFixture();

    // Static arrays can be multidimensional

    char *inpl = "\
    managed struct Struct                \n\
    {                                    \n\
        int Int1[5, 4];                  \n\
        int Int2[2][3];                  \n\
    };                                   \n\
                                         \n\
    int main()                           \n\
    {                                    \n\
        Struct S = new Struct;           \n\
        S.Int1[4, 2] = 1;                \n\
        S.Int2[1][2] = S.Int1[4, 2];     \n\
    }                                    \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("ArrayInStruct2", scrip);
    const size_t codesize = 63;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   73,    3,          104,   51,    0,   47,    // 7
       3,    1,    1,    4,            6,    3,    1,   29,    // 15
       3,   51,    8,   48,            2,   52,    1,    2,    // 23
      72,   30,    3,    8,            3,   51,    4,   48,    // 31
       2,   52,    1,    2,           72,    7,    3,   29,    // 39
       3,   51,    8,   48,            2,   52,    1,    2,    // 47
     100,   30,    3,    8,            3,   51,    4,   49,    // 55
       2,    1,    4,    6,            3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func11) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    int Func(int I, ...)                 \n\
    {                                    \n\
        return I + I / I;                \n\
    }                                    \n\
                                         \n\
    int main()                           \n\
    {                                    \n\
        return 0;                        \n\
    }                                    \n\
    ";


    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func11", scrip);
    const size_t codesize = 51;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   29,    3,    // 7
      51,   12,    7,    3,           29,    3,   51,   16,    // 15
       7,    3,   30,    4,           10,    4,    3,    3,    // 23
       4,    3,   30,    4,           11,    4,    3,    3,    // 31
       4,    3,   31,    3,            6,    3,    0,    5,    // 39
      38,   40,    6,    3,            0,   31,    3,    6,    // 47
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, Func12) {
    ccCompiledScript *scrip = newScriptFixture();

    // Function with float default, or default "0", for float parameter
    char *inpl = "\
    float Func1(float F = 7.2)          \n\
    {                                   \n\
        return F;                       \n\
    }                                   \n\
                                        \n\
    float Func2(float F = 0)            \n\
    {                                   \n\
        return F;                       \n\
    }                                   \n\
                                        \n\
    float Call()                        \n\
    {                                   \n\
        return Func1() + Func2();       \n\
    }                                   \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func12", scrip);
    const size_t codesize = 68;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   31,    3,    // 7
       6,    3,    0,    5,           38,   12,   51,    8,    // 15
       7,    3,   31,    3,            6,    3,    0,    5,    // 23
      38,   24,    6,    3,         1088841318,   29,    3,    6,    // 31
       3,    0,   23,    3,            2,    1,    4,   29,    // 39
       3,    6,    3,    0,           29,    3,    6,    3,    // 47
      12,   23,    3,    2,            1,    4,   30,    4,    // 55
      57,    4,    3,    3,            4,    3,   31,    3,    // 63
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      33,   48,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,   2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}


TEST(Bytecode, Func13) {
    ccCompiledScript *scrip = newScriptFixture();

    // Function with default null or 0 for managed parameter
    char *inpl = "\
    managed struct S                    \n\
    {                                   \n\
        float f;                        \n\
    };                                  \n\
                                        \n\
    S *Func1(S s = null)                \n\
    {                                   \n\
        return s;                       \n\
    }                                   \n\
                                        \n\
    S *Func2(S s = 0)                   \n\
    {                                   \n\
        return s;                       \n\
    }                                   \n\
                                        \n\
    int Call()                           \n\
    {                                   \n\
        return Func1() == Func2();      \n\
    }                                   \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func13", scrip);
    const size_t codesize = 112;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   47,    3,    // 7
      51,    8,   48,    3,           29,    3,   51,    4,    // 15
      47,    3,   51,   12,           49,   51,    4,   48,    // 23
       3,   69,   30,    4,           31,    3,    6,    3,    // 31
       0,    5,   38,   34,           51,    8,    7,    3,    // 39
      47,    3,   51,    8,           48,    3,   29,    3,    // 47
      51,    4,   47,    3,           51,   12,   49,   51,    // 55
       4,   48,    3,   69,           30,    4,   31,    3,    // 63
       6,    3,    0,    5,           38,   68,    6,    3,    // 71
       0,   29,    3,    6,            3,    0,   23,    3,    // 79
       2,    1,    4,   29,            3,    6,    3,    0,    // 87
      29,    3,    6,    3,           34,   23,    3,    2,    // 95
       1,    4,   30,    4,           15,    4,    3,    3,    // 103
       4,    3,   31,    3,            6,    3,    0,    5,    // 111
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      77,   92,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,   2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Func14) {
    ccCompiledScript *scrip = newScriptFixture();

    // Strange misalignment due to bad function protocol

    char *inpl = "\
        struct Struct               \n\
        {                           \n\
            int A[];                \n\
            int B[];                \n\
                                    \n\
            import void Test(int Arg);  \n\
        };                          \n\
                                    \n\
        void Struct::Test(int Arg)  \n\
        {                           \n\
            this.A = new int[1];    \n\
            this.B = new int[1];    \n\
            this.B[0] = 123;        \n\
            Display(this.A[0], this.B[0]); \n\
        }                           \n\
                                    \n\
        void Display(int X, int Y)  \n\
        {                           \n\
        }                           \n\
                                    \n\
        int main()                  \n\
        {                           \n\
            Struct TS;              \n\
            TS.Test(7);             \n\
        }                           \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Func14", scrip);
    const size_t codesize = 135;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            1,   72,    3,    4,    // 7
       0,    3,    6,    2,           52,   47,    3,    6,    // 15
       3,    1,   72,    3,            4,    0,    3,    6,    // 23
       2,   52,    1,    2,            4,   47,    3,    6,    // 31
       3,  123,   29,    3,            3,    6,    2,   52,    // 39
       1,    2,    4,   48,            2,   52,   30,    3,    // 47
       8,    3,    3,    6,            2,   52,    1,    2,    // 55
       4,   48,    2,   52,            7,    3,   29,    3,    // 63
       3,    6,    2,   52,           48,    2,   52,    7,    // 71
       3,   29,    3,    6,            3,   84,   23,    3,    // 79
       2,    1,    8,    5,           38,   84,    5,   38,    // 87
      87,   51,    0,   63,            8,    1,    1,    8,    // 95
      51,    8,   29,    2,            6,    3,    7,   29,    // 103
       3,   51,    8,    7,            2,   45,    2,    6,    // 111
       3,    0,   23,    3,            2,    1,    4,   30,    // 119
       2,   51,    8,   49,            1,    2,    4,   49,    // 127
       2,    1,    8,    6,            3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      77,  113,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,   2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Writeprotected) {
    ccCompiledScript *scrip = newScriptFixture();

    // Directly taken from the doc on writeprotected, simplified.
    char *inpl = "\
        struct Weapon {                         \n\
            short Beauty;                       \n\
            writeprotected int Damage;          \n\
            import int SetDamage(int damage);   \n\
        } wp;                                   \n\
                                                \n\
        int  Weapon::SetDamage(int damage)      \n\
        {                                       \n\
            this.Damage = damage;               \n\
            return 0;                           \n\
        }                                       \n\
                                                \n\
        int main()                              \n\
        {                                       \n\
            return wp.Damage;                   \n\
        }                                       \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Writeprotected", scrip);
    const size_t codesize = 37;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,    3,    6,    // 7
       2,   52,    1,    2,            2,    8,    3,    6,    // 15
       3,    0,   31,    3,            6,    3,    0,    5,    // 23
      38,   24,    6,    2,            2,    7,    3,   31,    // 31
       3,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      28,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Protected1) {
    ccCompiledScript *scrip = newScriptFixture();

    // Directly taken from the doc on protected, simplified.
    char *inpl = "\
        struct Weapon {                        \n\
            protected int Damage;              \n\
            import int SetDamage(int damage);  \n\
        };                                     \n\
                                               \n\
        Weapon wp;                             \n\
                                               \n\
        int  Weapon::SetDamage(int damage)     \n\
        {                                      \n\
            this.Damage = damage;              \n\
            return 0;                          \n\
        }                                      \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Protected1", scrip);
    const size_t codesize = 21;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,    3,    6,    // 7
       2,   52,    8,    3,            6,    3,    0,   31,    // 15
       3,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Static1) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        struct Weapon {                         \n\
            import static int CalcDamage(       \n\
            int Lifepoints, int Hitpoints = 5);   \n\
        };                                      \n\
                                                \n\
        static int Weapon::CalcDamage(int Lifepoints, int Hitpoints)  \n\
        {                                       \n\
            return Lifepoints - Hitpoints;      \n\
        }                                       \n\
                                                \n\
        int main()                              \n\
        {                                       \n\
            int hp = Weapon.CalcDamage(9) + Weapon.CalcDamage(9, 40);  \n\
            return hp + Weapon.CalcDamage(100);     \n\
        }                                       \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Static1", scrip);
    const size_t codesize = 120;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   29,    3,    // 7
      51,   16,    7,    3,           30,    4,   12,    4,    // 15
       3,    3,    4,    3,           31,    3,    6,    3,    // 23
       0,    5,   38,   26,            6,    3,    5,   29,    // 31
       3,    6,    3,    9,           29,    3,    6,    3,    // 39
       0,   23,    3,    2,            1,    8,   29,    3,    // 47
       6,    3,   40,   29,            3,    6,    3,    9,    // 55
      29,    3,    6,    3,            0,   23,    3,    2,    // 63
       1,    8,   30,    4,           11,    4,    3,    3,    // 71
       4,    3,   29,    3,           51,    4,    7,    3,    // 79
      29,    3,    6,    3,            5,   29,    3,    6,    // 87
       3,  100,   29,    3,            6,    3,    0,   23,    // 95
       3,    2,    1,    8,           30,    4,   11,    4,    // 103
       3,    3,    4,    3,            2,    1,    4,   31,    // 111
       6,    2,    1,    4,            6,    3,    0,    5,    // 119
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 3;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      40,   60,   94,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,   2,   2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Static2) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        struct Weapon {                        \n\
        };                                     \n\
                                               \n\
        int CalcDamage(static Weapon, int Lifepoints, int Hitpoints)  \n\
        {                                      \n\
            return Lifepoints - Hitpoints;     \n\
        }                                      \n\
                                               \n\
        int main()                             \n\
        {                                      \n\
            return Weapon.CalcDamage(9, 40);   \n\
        }                                      \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Static2", scrip);

    const size_t codesize = 52;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   29,    3,    // 7
      51,   16,    7,    3,           30,    4,   12,    4,    // 15
       3,    3,    4,    3,           31,    3,    6,    3,    // 23
       0,    5,   38,   26,            6,    3,   40,   29,    // 31
       3,    6,    3,    9,           29,    3,    6,    3,    // 39
       0,   23,    3,    2,            1,    8,   31,    3,    // 47
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      40,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);

}

TEST(Bytecode, Protected2) {
    ccCompiledScript *scrip = newScriptFixture();

    // In a struct func, a variable that can't be found otherwise
    // should be taken to be out of the current struct.
    // (Note that this will currently compile to slightly more
    // inefficient code than "this.Damage = damage")

    char *inpl = "\
        struct Weapon {                        \n\
            protected int Damage;              \n\
            import int SetDamage(int damage);  \n\
        };                                     \n\
                                               \n\
        Weapon wp;                             \n\
                                               \n\
        int  Weapon::SetDamage(int damage)     \n\
        {                                      \n\
            Damage = damage;                   \n\
            return 0;                          \n\
        }                                      \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Protected2", scrip);
    const size_t codesize = 25;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   29,    3,    // 7
       3,    6,    2,   52,           30,    3,    8,    3,    // 15
       6,    3,    0,   31,            3,    6,    3,    0,    // 23
       5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Import) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        import int Weapon;                     \n\
                                               \n\
        int Func(int damage)                   \n\
        {                                      \n\
            int Int = 0;                       \n\
            Weapon = 77;                       \n\
            if (Weapon < 0)                    \n\
                Weapon = damage - (Int - Weapon) / Int; \n\
        }                                      \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Import", scrip);
    const size_t codesize = 94;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   29,    3,    6,    // 7
       3,   77,    6,    2,            0,    8,    3,    6,    // 15
       2,    0,    7,    3,           29,    3,    6,    3,    // 23
       0,   30,    4,   18,            4,    3,    3,    4,    // 31
       3,   28,   52,   51,           12,    7,    3,   29,    // 39
       3,   51,    8,    7,            3,   29,    3,    6,    // 47
       2,    0,    7,    3,           30,    4,   12,    4,    // 55
       3,    3,    4,    3,           29,    3,   51,   12,    // 63
       7,    3,   30,    4,           10,    4,    3,    3,    // 71
       4,    3,   30,    4,           12,    4,    3,    3,    // 79
       4,    3,    6,    2,            0,    8,    3,    2,    // 87
       1,    4,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 4;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      12,   17,   49,   84,        -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,   4,   4,     '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "Weapon",       "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Switch02) {
    ccCompiledScript *scrip = newScriptFixture();

    // Last switch clause no "break"
    char *inpl = "\
        void main()                     \n\
        {                               \n\
            int i = 5;                  \n\
            switch(i)                   \n\
            {                           \n\
            default: break;             \n\
            case 5: i = 0;              \n\
            }                           \n\
            return;                     \n\
        }                               \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Switch02", scrip);
    const size_t codesize = 50;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            5,   29,    3,   51,    // 7
       4,    7,    3,    3,            3,    4,   31,   11,    // 15
      31,   23,    6,    3,            0,   51,    4,    8,    // 23
       3,   31,   14,   29,            4,    6,    3,    5,    // 31
      30,    4,   16,    3,            4,   28,  -21,   31,    // 39
     -25,    2,    1,    4,           31,    3,    2,    1,    // 47
       4,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Attributes01) {
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        enum bool { false = 0, true = 1 };              \n\
        builtin managed struct ViewFrame {              \n\
            readonly import attribute bool Flipped;     \n\
            import attribute int Graphic;               \n\
            readonly import attribute float AsFloat;    \n\
        };                                              \n\
                                                        \n\
        int main()                                      \n\
        {                                               \n\
            ViewFrame *VF;                              \n\
            if (VF.Flipped)                             \n\
            {                                           \n\
                VF.Graphic = 17;                        \n\
                float f = VF.AsFloat + VF.AsFloat;      \n\
                return VF.Graphic;                      \n\
            }                                           \n\
            return VF.Flipped;                          \n\
        }                                               \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Attributes01", scrip);
    const size_t codesize = 166;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    0,           49,    1,    1,    4,    // 7
      51,    4,   48,    2,           52,   29,    6,   45,    // 15
       2,   39,    0,    6,            3,    0,   33,    3,    // 23
      30,    6,   28,  102,            6,    3,   17,   51,    // 31
       4,   48,    2,   52,           29,    6,   34,    3,    // 39
      45,    2,   39,    1,            6,    3,    2,   33,    // 47
       3,   35,    1,   30,            6,   51,    4,   48,    // 55
       2,   52,   29,    6,           45,    2,   39,    0,    // 63
       6,    3,    3,   33,            3,   30,    6,   29,    // 71
       3,   51,    8,   48,            2,   52,   29,    6,    // 79
      45,    2,   39,    0,            6,    3,    3,   33,    // 87
       3,   30,    6,   30,            4,   57,    4,    3,    // 95
       3,    4,    3,   29,            3,   51,    8,   48,    // 103
       2,   52,   29,    6,           45,    2,   39,    0,    // 111
       6,    3,    1,   33,            3,   30,    6,   51,    // 119
       8,   49,    2,    1,            8,   31,   38,    2,    // 127
       1,    4,   51,    4,           48,    2,   52,   29,    // 135
       6,   45,    2,   39,            0,    6,    3,    0,    // 143
      33,    3,   30,    6,           51,    4,   49,    2,    // 151
       1,    4,   31,    9,           51,    4,   49,    2,    // 159
       1,    4,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 6;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      21,   46,   66,   86,        114,  143,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,   4,   4,      4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 4;
    std::string imports[] = {
    "ViewFrame::get_Flipped^0",   "ViewFrame::get_Graphic^0",   "ViewFrame::set_Graphic^1",   // 2
    "ViewFrame::get_AsFloat^0",    "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Attributes02) {
    ccCompiledScript *scrip = newScriptFixture();

    // The getter and setter functions are defined locally, so
    // they ought to be exported instead of imported.
    // Assigning to the attribute should generate the same call
    // as calling the setter; reading the same as calling the getter.
    // Armor:: functions should be allowed to access _Damage.

    char *inpl = "\
        managed struct Armor {                          \n\
            attribute int Damage;                       \n\
            writeprotected short _Aura;                 \n\
            protected int _Damage;                      \n\
        };                                              \n\
                                                        \n\
        int main()                                      \n\
        {                                               \n\
            Armor *armor = new Armor;                   \n\
            armor.Damage = 17;                          \n\
            return (armor.Damage > 10);                 \n\
        }                                               \n\
                                                        \n\
        void Armor::set_Damage(int damage)              \n\
        {                                               \n\
            if (damage >= 0)                            \n\
                _Damage = damage;                       \n\
        }                                               \n\
                                                        \n\
        int Armor::get_Damage()                         \n\
        {                                               \n\
            return _Damage;                             \n\
        }                                               \n\
        ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Attributes02", scrip);
    const size_t codesize = 139;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   73,    3,            8,   51,    0,   47,    // 7
       3,    1,    1,    4,            6,    3,   17,   51,    // 15
       4,   48,    2,   52,           29,    6,   29,    3,    // 23
      45,    2,    6,    3,           83,   23,    3,    2,    // 31
       1,    4,   30,    6,           51,    4,   48,    2,    // 39
      52,   29,    6,   45,            2,    6,    3,  122,    // 47
      23,    3,   30,    6,           29,    3,    6,    3,    // 55
      10,   30,    4,   17,            4,    3,    3,    4,    // 63
       3,   51,    4,   49,            2,    1,    4,   31,    // 71
       9,   51,    4,   49,            2,    1,    4,    6,    // 79
       3,    0,    5,   38,           83,   51,    8,    7,    // 87
       3,   29,    3,    6,            3,    0,   30,    4,    // 95
      19,    4,    3,    3,            4,    3,   28,   17,    // 103
      51,    8,    7,    3,           29,    3,    3,    6,    // 111
       2,   52,    1,    2,            2,   30,    3,    8,    // 119
       3,    5,   38,  122,            3,    6,    2,   52,    // 127
       1,    2,    2,    7,            3,   31,    3,    6,    // 135
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      28,   47,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,   2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Attributes03) {
    ccCompiledScript *scrip = newScriptFixture();

    // The getters and setters are NOT defined locally, so
    // import decls should be generated for them.
    // The getters and setters should be called as import funcs.

    char *inpl = "\
        managed struct Armor {                          \n\
            attribute int Damage;                       \n\
            writeprotected short _aura;                 \n\
            protected int _damage;                      \n\
        };                                              \n\
                                                        \n\
        int main()                                      \n\
        {                                               \n\
            Armor *armor = new Armor;                   \n\
            armor.Damage = 17;                          \n\
            return (armor.Damage > 10);                 \n\
        }";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Attributes03", scrip);
    const size_t codesize = 86;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   73,    3,            8,   51,    0,   47,    // 7
       3,    1,    1,    4,            6,    3,   17,   51,    // 15
       4,   48,    2,   52,           29,    6,   34,    3,    // 23
      45,    2,   39,    1,            6,    3,    1,   33,    // 31
       3,   35,    1,   30,            6,   51,    4,   48,    // 39
       2,   52,   29,    6,           45,    2,   39,    0,    // 47
       6,    3,    0,   33,            3,   30,    6,   29,    // 55
       3,    6,    3,   10,           30,    4,   17,    4,    // 63
       3,    3,    4,    3,           51,    4,   49,    2,    // 71
       1,    4,   31,    9,           51,    4,   49,    2,    // 79
       1,    4,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      30,   50,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 2;
    std::string imports[] = {
    "Armor::get_Damage^0",        "Armor::set_Damage^1",         "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, StringStandard01) {
    ccCompiledScript *scrip = newScriptFixture();

    char inpl[] = "\
        int main()                         \n\
        {                                  \n\
            String s = \"Hello, world!\";  \n\
            if (s != \"Bye\")              \n\
                return 1;                  \n\
            return 0;                      \n\
        }                                  \n\
        ";
    std::string input = "";
    input += g_Input_Bool;
    input += g_Input_String;
    input += inpl;

    clear_error();
    int compileResult = cc_compile(input.c_str(), scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("StringStandard01", scrip);
    const size_t codesize = 65;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   64,    3,   51,    // 7
       0,   47,    3,    1,            1,    4,   51,    4,    // 15
      48,    3,   29,    3,            6,    3,   14,   30,    // 23
       4,   66,    4,    3,            3,    4,    3,   28,    // 31
      11,    6,    3,    1,           51,    4,   49,    2,    // 39
       1,    4,   31,   20,            6,    3,    0,   51,    // 47
       4,   49,    2,    1,            4,   31,    9,   51,    // 55
       4,   49,    2,    1,            4,    6,    3,    0,    // 63
       5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   22,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      3,   3,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 18;
    EXPECT_EQ(stringssize, scrip->stringssize);

    char strings[] = {
    'H',  'e',  'l',  'l',          'o',  ',',  ' ',  'w',     // 7
    'o',  'r',  'l',  'd',          '!',    0,  'B',  'y',     // 15
    'e',    0,  '\0'
    };

    for (size_t idx = 0; static_cast<int>(idx) < stringssize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->stringssize) break;
        std::string prefix = "strings[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(strings[idx]);
        std::string test_val = prefix + std::to_string(scrip->strings[idx]);
        ASSERT_EQ(is_val, test_val);
    }
}

TEST(Bytecode, StringOldstyle03) {
    ccSetOption(SCOPT_OLDSTRINGS, true);
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int Sentinel1;                  \n\
        string Global;                  \n\
        int Sentinel2;                  \n\
                                        \n\
        void ModifyString(string Parm)  \n\
        {                               \n\
            Parm = \"Parameter\";       \n\
        }                               \n\
                                        \n\
        int main()                      \n\
        {                               \n\
            ModifyString(Global);       \n\
        }                               \n\
        ";
    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("StringOldstyle03", scrip);
    const size_t codesize = 67;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   51,    8,    3,    // 7
       3,    5,    3,    2,            4,    6,    7,  199,    // 15
       3,    4,    2,    7,            3,    3,    5,    2,    // 23
       8,    3,   28,   16,            1,    4,    1,    1,    // 31
       5,    1,    2,    7,            1,    3,    7,    3,    // 39
      28,    2,   31,  -28,            5,   38,   45,    6,    // 47
       2,    4,    3,    2,            3,   29,    3,    6,    // 55
       3,    0,   23,    3,            2,    1,    4,    6,    // 63
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 3;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   49,   57,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      3,   1,   2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 10;
    EXPECT_EQ(stringssize, scrip->stringssize);

    char strings[] = {
    'P',  'a',  'r',  'a',          'm',  'e',  't',  'e',     // 7
    'r',    0,  '\0'
    };

    for (size_t idx = 0; static_cast<int>(idx) < stringssize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->stringssize) break;
        std::string prefix = "strings[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(strings[idx]);
        std::string test_val = prefix + std::to_string(scrip->strings[idx]);
        ASSERT_EQ(is_val, test_val);
    }
}

TEST(Bytecode, StringOldstyle04) {
    ccSetOption(SCOPT_OLDSTRINGS, true);
    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
        int Sentinel;                   \n\
        string Global;                  \n\
        int main()                      \n\
        {                               \n\
            string Local = Func(Global); \n\
        }                               \n\
        string Func(string Param)       \n\
        {                               \n\
            return Param;               \n\
        }                               \n\
        ";
    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("StringOldstyle04", scrip);
    const size_t codesize = 45;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            4,    3,    2,    3,    // 7
      29,    3,    6,    3,           32,   23,    3,    2,    // 15
       1,    4,   51,    0,            8,    3,    1,    1,    // 23
     200,    2,    1,  200,            6,    3,    0,    5,    // 31
      38,   32,   51,    8,            3,    2,    3,   31,    // 39
       3,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   12,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, StringStandard02) {
    ccCompiledScript *scrip = newScriptFixture();

    char inpl[] = "\
        String S;                           \n\
        import String I;                    \n\
        String Func1()                      \n\
        {                                   \n\
            return S;                       \n\
        }                                   \n\
        String Func2(String P)              \n\
        {                                   \n\
            return P;                       \n\
        }                                   \n\
        String Func3()                      \n\
        {                                   \n\
            String L = \"Hello!\";          \n\
            return L;                       \n\
        }                                   \n\
        String Func4()                      \n\
        {                                   \n\
            return \"Hello!\";              \n\
        }                                   \n\
        ";
    std::string input = "";
    input += g_Input_Bool;
    input += g_Input_String;
    input += inpl;

    clear_error();
    int compileResult = cc_compile(input.c_str(), scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("StringStandard02", scrip);
    const size_t codesize = 109;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            0,   48,    3,   31,    // 7
       3,    6,    3,    0,            5,   38,   13,   51,    // 15
       8,    7,    3,   47,            3,   51,    8,   48,    // 23
       3,   29,    3,   51,            4,   47,    3,   51,    // 31
      12,   49,   51,    4,           48,    3,   69,   30,    // 39
       4,   31,    3,    6,            3,    0,    5,   38,    // 47
      47,    6,    3,    7,           64,    3,   51,    0,    // 55
      47,    3,    1,    1,            4,   51,    4,   48,    // 63
       3,   29,    3,   51,            4,   47,    3,   51,    // 71
       8,   49,   51,    4,           48,    3,   69,   30,    // 79
       4,    2,    1,    4,           31,    9,   51,    4,    // 87
      49,    2,    1,    4,            6,    3,    0,    5,    // 95
      38,   96,    6,    3,            7,   64,    3,   31,    // 103
       3,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 3;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   51,  100,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   3,   3,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 14;
    EXPECT_EQ(stringssize, scrip->stringssize);

    char strings[] = {
    'H',  'e',  'l',  'l',          'o',  '!',    0,  'H',     // 7
    'e',  'l',  'l',  'o',          '!',    0,  '\0'
    };

    for (size_t idx = 0; static_cast<int>(idx) < stringssize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->stringssize) break;
        std::string prefix = "strings[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(strings[idx]);
        std::string test_val = prefix + std::to_string(scrip->strings[idx]);
        ASSERT_EQ(is_val, test_val);
    }
}

TEST(Bytecode, StringStandardOldstyle) {
    ccSetOption(SCOPT_OLDSTRINGS, true);
    ccCompiledScript *scrip = newScriptFixture();

    char inpl[] = "\
        string OS;                          \n\
        String Func1()                      \n\
        {                                   \n\
            return OS;                      \n\
        }                                   \n\
        String Func2(String P)              \n\
        {                                   \n\
            return P;                       \n\
        }                                   \n\
        int Func3(const string OP)          \n\
        {                                   \n\
            Func2(\"Hello\");               \n\
        }                                   \n\
        String Func4()                      \n\
        {                                   \n\
            String S = \"Hello\";           \n\
            Func3(S);                       \n\
        }                                   \n\
        ";
    std::string input = "";
    input += g_Input_Bool;
    input += g_Input_String;
    input += inpl;

    clear_error();
    int compileResult = cc_compile(input.c_str(), scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("StringStandardOldstyle", scrip);
    const size_t codesize = 111;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            0,    3,    2,    3,    // 7
      64,    3,   31,    3,            6,    3,    0,    5,    // 15
      38,   16,   51,    8,            7,    3,   47,    3,    // 23
      51,    8,   48,    3,           29,    3,   51,    4,    // 31
      47,    3,   51,   12,           49,   51,    4,   48,    // 39
       3,   69,   30,    4,           31,    3,    6,    3,    // 47
       0,    5,   38,   50,            6,    3,    6,   64,    // 55
       3,   29,    3,    6,            3,   16,   23,    3,    // 63
       2,    1,    4,    6,            3,    0,    5,   38,    // 71
      71,    6,    3,    6,           64,    3,   51,    0,    // 79
      47,    3,    1,    1,            4,   51,    4,   48,    // 87
       3,   67,    3,   29,            3,    6,    3,   50,    // 95
      23,    3,    2,    1,            4,   51,    4,   49,    // 103
       2,    1,    4,    6,            3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 5;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   54,   61,   75,         95,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   3,   2,   3,      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 12;
    EXPECT_EQ(stringssize, scrip->stringssize);

    char strings[] = {
    'H',  'e',  'l',  'l',          'o',    0,  'H',  'e',     // 7
    'l',  'l',  'o',    0,          '\0'
    };

    for (size_t idx = 0; static_cast<int>(idx) < stringssize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->stringssize) break;
        std::string prefix = "strings[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(strings[idx]);
        std::string test_val = prefix + std::to_string(scrip->strings[idx]);
        ASSERT_EQ(is_val, test_val);
    }
}

TEST(Bytecode, AccessStructAsPointer01)
{
    ccCompiledScript *scrip = newScriptFixture();

    // Managed structs can be declared without (implicit) pointer iff:
    // - they are "import" globals
    // - the struct is "builtin" as well as "managed".
    // Such structs can be used as a parameter of a function that expects a pointered struct

    char *inpl = "\
        builtin managed struct Object {                 \n\
        };                                              \n\
        import Object oCleaningCabinetDoor;             \n\
                                                        \n\
        builtin managed struct Character                \n\
        {                                               \n\
            import int FaceObject(Object *);            \n\
        };                                              \n\
        import readonly Character *player;              \n\
                                                        \n\
        int oCleaningCabinetDoor_Interact()             \n\
        {                                               \n\
            player.FaceObject(oCleaningCabinetDoor);    \n\
        }                                               \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("AccessStructAsPointer01", scrip);
    const size_t codesize = 39;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            2,   48,    2,   52,    // 7
      29,    2,    6,    2,            0,    3,    2,    3,    // 15
      34,    3,   51,    4,            7,    2,   45,    2,    // 23
      39,    1,    6,    3,            1,   33,    3,   35,    // 31
       1,   30,    2,    6,            3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 3;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   12,   28,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 3;
    std::string imports[] = {
    "oCleaningCabinetDoor",       "Character::FaceObject^1",    "player",      // 2
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, AccessStructAsPointer02)
{
    ccCompiledScript *scrip = newScriptFixture();

    // Managed structs can be declared without (implicit) pointer in certain circumstances.
    // Such structs can be assigned to a variable that is a pointered struct

    char *inpl = "\
        builtin managed struct Object {                 \n\
        };                                              \n\
        import Object oCleaningCabinetDoor;             \n\
                                                        \n\
        builtin managed struct Character                \n\
        {                                               \n\
            import int FaceObject(Object *);            \n\
        };                                              \n\
        import readonly Character *player;              \n\
                                                        \n\
        int oCleaningCabinetDoor_Interact()             \n\
        {                                               \n\
            Object o1 = oCleaningCabinetDoor;           \n\
            player.FaceObject(o1);                      \n\
        }                                               \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("AccessStructAsPointer02", scrip);
    const size_t codesize = 56;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            0,    3,    2,    3,    // 7
      51,    0,   47,    3,            1,    1,    4,    6,    // 15
       2,    2,   48,    2,           52,   29,    2,   51,    // 23
       8,   48,    3,   34,            3,   51,    4,    7,    // 31
       2,   45,    2,   39,            1,    6,    3,    1,    // 39
      33,    3,   35,    1,           30,    2,   51,    4,    // 47
      49,    2,    1,    4,            6,    3,    0,    5,    // 55
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 3;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   17,   39,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 3;
    std::string imports[] = {
    "oCleaningCabinetDoor",       "Character::FaceObject^1",    "player",      // 2
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, AccessStructAsPointer03)
{
    ccCompiledScript *scrip = newScriptFixture();

    // Managed structs can be declared without (implicit) pointer in certain circumstances.
    // Such structs can be assigned to a variable that is a pointered struct

    char *inpl = "\
        builtin managed struct Object {                 \n\
            readonly int Reserved;                      \n\
        };                                              \n\
        import Object object[40];                       \n\
                                                        \n\
        builtin managed struct Character                \n\
        {                                               \n\
            import int FaceObject(Object *);            \n\
        };                                              \n\
        import readonly Character *player;              \n\
                                                        \n\
        int oCleaningCabinetDoor_Interact()             \n\
        {                                               \n\
            Object o2 = object[5];                      \n\
        }                                               \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("AccessStructAsPointer03", scrip);
    const size_t codesize = 28;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            0,    1,    2,   20,    // 7
       3,    2,    3,   51,            0,   47,    3,    1,    // 15
       1,    4,   51,    4,           49,    2,    1,    4,    // 23
       6,    3,    0,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 3;
    std::string imports[] = {
    "object",      "Character::FaceObject^1",    "player",       "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Attributes04) {
    ccCompiledScript *scrip = newScriptFixture();

    // Attribute func was not called properly

    char *inpl = "\
        builtin managed struct Character {      \n\
            import attribute int  x;            \n\
        };                                      \n\
        import readonly Character *player;      \n\
                                                \n\
        int LinkattusStoop(int x, int y)        \n\
        {                                       \n\
            player.x += 77;                     \n\
        }                                       \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("Attributes04", scrip);
    const size_t codesize = 58;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           77,   29,    3,    6,    // 7
       2,    2,   48,    2,           52,   29,    6,   45,    // 15
       2,   39,    0,    6,            3,    0,   33,    3,    // 23
      30,    6,   30,    4,           11,    3,    4,    6,    // 31
       2,    2,   48,    2,           52,   29,    6,   34,    // 39
       3,   45,    2,   39,            1,    6,    3,    1,    // 47
      33,    3,   35,    1,           30,    6,    6,    3,    // 55
       0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 4;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       9,   21,   33,   47,        -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,   4,   4,     '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 3;
    std::string imports[] = {
    "Character::get_x^0",         "Character::set_x^1",         "player",      // 2
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Attributes05) {
    ccCompiledScript *scrip = newScriptFixture();

    // Test static attribute

    char *inpl = "\
        enum bool                               \n\
        {                                       \n\
            false = 0,                          \n\
            true = 1                            \n\
        };                                      \n\
                                                \n\
        builtin managed struct Game             \n\
        {                                       \n\
            readonly import static attribute    \n\
                bool SkippingCutscene;          \n\
        };                                      \n\
                                                \n\
        void Hook3()                            \n\
        {                                       \n\
            if (Game.SkippingCutscene)          \n\
            {                                   \n\
                int i = 99;                     \n\
            }                                   \n\
        }                                       \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("Attributes05", scrip);
    const size_t codesize = 20;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   39,    0,            6,    3,    0,   33,    // 7
       3,   28,    8,    6,            3,   99,   29,    3,    // 15
       2,    1,    4,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       6,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "Game::get_SkippingCutscene^0",               "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Attributes06) {
    ccCompiledScript *scrip = newScriptFixture();

    // Indexed static attribute -- must return an int

    char *inpl = "\
        builtin managed struct Game             \n\
        {                                       \n\
            readonly import static attribute    \n\
                int SpriteWidth[];              \n\
        };                                      \n\
                                                \n\
        void main()                             \n\
        {                                       \n\
            int I = Game.SpriteWidth[9];        \n\
        }                                       \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("Attributes06", scrip);
    const size_t codesize = 22;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            9,   34,    3,   39,    // 7
       1,    6,    3,    0,           33,    3,   35,    1,    // 15
      29,    3,    2,    1,            4,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      11,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "Game::geti_SpriteWidth^1",    "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Attributes07) {
    ccCompiledScript *scrip = newScriptFixture();

    // Assignment to attribute -- should not generate null dereference error

    std::string inpl = g_Input_Bool;
    inpl += g_Input_String;
    inpl += "\
        builtin managed struct Label {      \n\
            attribute String Text;          \n\
        };                                  \n\
        import Label lbl;                   \n\
                                            \n\
        void main()                         \n\
        {                                   \n\
            lbl.Text = \"\";                \n\
        }                                   \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl.c_str(), scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("Attributes07", scrip);
    const size_t codesize = 26;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,    6,    2,   22,    // 7
      29,    6,   34,    3,           45,    2,   39,    1,    // 15
       6,    3,   21,   33,            3,   35,    1,   30,    // 23
       6,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 3;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,    7,   18,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      3,   4,   4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 2;
    std::string imports[] = {
    "Label::set_Text^1",          "lbl",          "[[SENTINEL]]"
    };


    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 1;
    EXPECT_EQ(stringssize, scrip->stringssize);

    char strings[] = {
      0,  '\0'
    };

    for (size_t idx = 0; static_cast<int>(idx) < stringssize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->stringssize) break;
        std::string prefix = "strings[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(strings[idx]);
        std::string test_val = prefix + std::to_string(scrip->strings[idx]);
        ASSERT_EQ(is_val, test_val);
    }
}

TEST(Bytecode, Struct09) {
    ccCompiledScript *scrip = newScriptFixture();

    // Should be able to find SetCharacter as a component of
    // VehicleBase as an extension of Vehicle Cars[5];
    // should generate call of VehicleBase::SetCharacter()

    char *inpl = "\
        enum CharacterDirection                                     \n\
        {                                                           \n\
            eDirectionUp = 3                                        \n\
        };                                                          \n\
                                                                    \n\
        builtin managed struct Character                            \n\
        {                                                           \n\
            readonly import attribute int ID;                       \n\
        };                                                          \n\
        import Character character[7];                              \n\
        import Character cAICar1;                                   \n\
                                                                    \n\
        struct VehicleBase                                          \n\
        {                                                           \n\
            import void SetCharacter(Character *c,                  \n\
                                int carSprite,                      \n\
                                CharacterDirection carSpriteDir,    \n\
                                int view = 0,                       \n\
                                int loop = 0,                       \n\
                                int frame = 0);                     \n\
        };                                                          \n\
                                                                    \n\
        struct Vehicle extends VehicleBase                          \n\
        {                                                           \n\
            float bodyMass;                                         \n\
        };                                                          \n\
        import Vehicle Cars[6];                                     \n\
                                                                    \n\
        int main()                                                  \n\
        {                                                           \n\
            int drivers[] = new int[6];                             \n\
            int i = 5;                                              \n\
            Cars[i].SetCharacter(                                   \n\
                character[cAICar1.ID + i],                          \n\
                7 + drivers[i],                                     \n\
                eDirectionUp,                                       \n\
                3 + i, 0, 0);                                       \n\
        }                                                           \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("Struct09", scrip);
    const size_t codesize = 193;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            6,   72,    3,    4,    // 7
       0,   51,    0,   47,            3,    1,    1,    4,    // 15
       6,    3,    5,   29,            3,    6,    2,    4,    // 23
      29,    2,   51,    8,            7,    3,   30,    2,    // 31
      46,    3,    6,   32,            3,    4,   11,    2,    // 39
       3,   29,    2,    6,            3,    0,   34,    3,    // 47
       6,    3,    0,   34,            3,    6,    3,    3,    // 55
      29,    3,   51,   12,            7,    3,   30,    4,    // 63
      11,    4,    3,    3,            4,    3,   34,    3,    // 71
       6,    3,    3,   34,            3,    6,    3,    7,    // 79
      29,    3,   51,   16,           48,    2,   52,   29,    // 87
       2,   51,   16,    7,            3,   30,    2,   32,    // 95
       3,    4,   71,    3,           11,    2,    3,    7,    // 103
       3,   30,    4,   11,            4,    3,    3,    4,    // 111
       3,   34,    3,    6,            2,    1,   29,    2,    // 119
       6,    2,    2,   29,            6,   45,    2,   39,    // 127
       0,    6,    3,    0,           33,    3,   30,    6,    // 135
      29,    3,   51,   16,            7,    3,   30,    4,    // 143
      11,    4,    3,    3,            4,    3,   30,    2,    // 151
      46,    3,    7,   32,            3,    0,   11,    2,    // 159
       3,    3,    2,    3,           34,    3,   51,    4,    // 167
       7,    2,   45,    2,           39,    6,    6,    3,    // 175
       3,   33,    3,   35,            6,   30,    2,   51,    // 183
       8,   49,    2,    1,            8,    6,    3,    0,    // 191
       5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 5;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      23,  117,  122,  131,        176,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,   4,   4,      4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 5;
    std::string imports[] = {
    "Character::get_ID^0",        "character",   "cAICar1",     "VehicleBase::SetCharacter^6",               // 3
    "Cars",         "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct10) {
    ccCompiledScript *scrip = newScriptFixture();

    // When accessing a component of an import variable,
    // the import variable must be read first so that the fixup can be
    // applied. Only then may the offset be added to it.

    char *inpl = "\
        import struct Struct                                 \n\
        {                                                    \n\
            int fluff;                                       \n\
            int k;                                           \n\
        } ss;                                                \n\
                                                             \n\
        int main()                                           \n\
        {                                                    \n\
            return ss.k;                                     \n\
        }                                                    \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct10", scrip);
    const size_t codesize = 16;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            0,    1,    2,    4,    // 7
       7,    3,   31,    3,            6,    3,    0,    5,    // 15
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "ss",           "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct11) {
    ccCompiledScript *scrip = newScriptFixture();

    // Structs may contain variables that are structs themselves.
    // Since Inner1 is managed, In1 will convert into an Inner1 *.

    char *inpl = "\
        managed struct Inner1                               \n\
        {                                                   \n\
            short Fluff;                                    \n\
            int Payload;                                    \n\
        };                                                  \n\
        struct Inner2                                       \n\
        {                                                   \n\
            short Fluff;                                    \n\
            int Payload;                                    \n\
        };                                                  \n\
        import int Foo;                                     \n\
        import struct Struct                                \n\
        {                                                   \n\
            Inner1 In1;                                     \n\
            Inner2 In2;                                     \n\
        } SS;                                               \n\
                                                            \n\
        int main()                                          \n\
        {                                                   \n\
            SS.In1 = new Inner1;                            \n\
            SS.In1.Payload = 77;                            \n\
            SS.In2.Payload = 777;                           \n\
            return SS.In1.Payload + SS.In2.Payload;         \n\
        }                                                   \n\
    ";
    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct11", scrip);
    const size_t codesize = 78;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   73,    3,            8,    6,    2,    1,    // 7
      47,    3,    6,    3,           77,   29,    3,    6,    // 15
       2,    1,   48,    2,           52,    1,    2,    2,    // 23
      30,    3,    8,    3,            6,    3,  777,   29,    // 31
       3,    6,    2,    1,            1,    2,    6,   30,    // 39
       3,    8,    3,    6,            2,    1,   48,    2,    // 47
      52,    1,    2,    2,            7,    3,   29,    3,    // 55
       6,    2,    1,    1,            2,    6,    7,    3,    // 63
      30,    4,   11,    4,            3,    3,    4,    3,    // 71
      31,    3,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 5;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       7,   17,   35,   45,         58,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      4,   4,   4,   4,      4,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 1;
    std::string imports[] = {
    "SS",           "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Struct12) {
    ccCompiledScript *scrip = newScriptFixture();

    // Managed structs may contain dynamic arrays.

    char *inpl = "\
        managed struct Inner                                \n\
        {                                                   \n\
            short Fluff;                                    \n\
            int Payload;                                    \n\
        };                                                  \n\
        short Fluff;                                        \n\
        managed struct Struct                               \n\
        {                                                   \n\
            Inner In[];                                     \n\
        } SS, TT[];                                         \n\
                                                            \n\
        int main()                                          \n\
        {                                                   \n\
            SS = new Struct;                                \n\
            SS.In = new Inner[7];                           \n\
            SS.In[3].Payload = 77;                          \n\
            TT = new Struct[5];                             \n\
            TT[2].In = new Inner[11];                       \n\
            TT[2].In[2].Payload = 777;                      \n\
            return SS.In[3].Payload + TT[2].In[2].Payload;  \n\
        }                                                   \n\
    ";
    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Struct12", scrip);
    const size_t codesize = 184;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   73,    3,            4,    6,    2,    2,    // 7
      47,    3,    6,    3,            7,   72,    3,    8,    // 15
       1,    6,    2,    2,           48,    2,   52,   47,    // 23
       3,    6,    3,   77,           29,    3,    6,    2,    // 31
       2,   48,    2,   52,           48,    2,   52,    1,    // 39
       2,   12,   48,    2,           52,    1,    2,    2,    // 47
      30,    3,    8,    3,            6,    3,    5,   72,    // 55
       3,    4,    1,    6,            2,    6,   47,    3,    // 63
       6,    3,   11,   72,            3,    8,    1,   29,    // 71
       3,    6,    2,    6,           48,    2,   52,    1,    // 79
       2,    8,   48,    2,           52,   30,    3,   47,    // 87
       3,    6,    3,  777,           29,    3,    6,    2,    // 95
       6,   48,    2,   52,            1,    2,    8,   48,    // 103
       2,   52,   48,    2,           52,    1,    2,    8,    // 111
      48,    2,   52,    1,            2,    2,   30,    3,    // 119
       8,    3,    6,    2,            2,   48,    2,   52,    // 127
      48,    2,   52,    1,            2,   12,   48,    2,    // 135
      52,    1,    2,    2,            7,    3,   29,    3,    // 143
       6,    2,    6,   48,            2,   52,    1,    2,    // 151
       8,   48,    2,   52,           48,    2,   52,    1,    // 159
       2,    8,   48,    2,           52,    1,    2,    2,    // 167
       7,    3,   30,    4,           11,    4,    3,    3,    // 175
       4,    3,   31,    3,            6,    3,    0,    5,    // 183
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 8;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       7,   19,   32,   61,         75,   96,  124,  146,    // 7
     -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   1,      1,   1,   1,   1,    // 7
     '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, DynArrayOfPrimitives) {
    ccCompiledScript *scrip = newScriptFixture();

    // Dynamic arrays of primitives are allowed.

    char *inpl = "\
        int main()                              \n\
        {                                       \n\
            short PrmArray[] = new short[10];   \n\
            PrmArray[7] = 0;                    \n\
            PrmArray[3] = PrmArray[7];          \n\
        }                                       \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("DynArrayOfPrimitives", scrip);
    const size_t codesize = 67;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           10,   72,    3,    2,    // 7
       0,   51,    0,   47,            3,    1,    1,    4,    // 15
       6,    3,    0,   29,            3,   51,    8,   48,    // 23
       2,   52,    1,    2,           14,   30,    3,   27,    // 31
       3,   51,    4,   48,            2,   52,    1,    2,    // 39
      14,   25,    3,   29,            3,   51,    8,   48,    // 47
       2,   52,    1,    2,            6,   30,    3,   27,    // 55
       3,   51,    4,   49,            2,    1,    4,    6,    // 63
       3,    0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, ManagedDerefZerocheck) {
    ccCompiledScript *scrip = newScriptFixture();

    // Bytecode ought to check that S isn't initialized yet
    char *inpl = "\
        managed struct Struct           \n\
        {                               \n\
            int Int[10];                \n\
        } S;                            \n\
                                        \n\
        int room_AfterFadeIn()          \n\
        {                               \n\
            S.Int[4] = 1;               \n\
        }                               \n\
        ";
    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());
    // WriteOutput("ManagedDerefZerocheck", scrip);
    const size_t codesize = 24;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            1,   29,    3,    6,    // 7
       2,    0,   48,    2,           52,    1,    2,   16,    // 15
      30,    3,    8,    3,            6,    3,    0,    5,    // 23
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       9,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, MemInitPtr1) {
    ccCompiledScript *scrip = newScriptFixture();

    // Check that pointer vars are pushed correctly in func calls

    char *inpl = "\
        managed struct Struct1          \n\
        {                               \n\
            float Payload1;             \n\
        };                              \n\
        managed struct Struct2          \n\
        {                               \n\
            char Payload2;              \n\
        };                              \n\
                                        \n\
        int main()                      \n\
        {                               \n\
            Struct1 SS1 = new Struct1;  \n\
            SS1.Payload1 = 0.7;         \n\
            Struct2 SS2 = new Struct2;  \n\
            SS2.Payload2 = 4;           \n\
            int Val = Func(SS1, SS2);   \n\
        }                               \n\
                                        \n\
        int Func(Struct1 S1, Struct2 S2) \n\
        {                               \n\
            return S2.Payload2;         \n\
        }                               \n\
        ";
    clear_error();
    int compileResult = cc_compile(inpl, scrip);

    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("MemInitPtr1", scrip);
    const size_t codesize = 110;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   73,    3,            4,   51,    0,   47,    // 7
       3,    1,    1,    4,            6,    3, 1060320051,   51,    // 15
       4,   48,    2,   52,            8,    3,   73,    3,    // 23
       4,   51,    0,   47,            3,    1,    1,    4,    // 31
       6,    3,    4,   51,            4,   48,    2,   52,    // 39
      26,    3,   51,    4,           48,    3,   29,    3,    // 47
      51,   12,   48,    3,           29,    3,    6,    3,    // 55
      77,   23,    3,    2,            1,    8,   29,    3,    // 63
      51,   12,   49,   51,            8,   49,    2,    1,    // 71
      12,    6,    3,    0,            5,   38,   77,   51,    // 79
       8,    7,    3,   47,            3,   51,   12,    7,    // 87
       3,   47,    3,   51,           12,   48,    2,   52,    // 95
      24,    3,   51,    8,           49,   51,   12,   49,    // 103
      31,    3,    6,    3,            0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 1;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
      56,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      2,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Ternary1) {

    // Accept a simple ternary expression

    ccCompiledScript *scrip = newScriptFixture();

    char *inpl = "\
    int Foo(int i)              \n\
    {                           \n\
        return i > 0 ? 1 : -1;  \n\
        return 9;               \n\
    }                           \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Ternary1", scrip);
    const size_t codesize = 40;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,   51,    8,            7,    3,   29,    3,    // 7
       6,    3,    0,   30,            4,   17,    4,    3,    // 15
       3,    4,    3,   28,            5,    6,    3,    1,    // 23
      31,    3,    6,    3,           -1,   31,    8,    6,    // 31
       3,    9,   31,    3,            6,    3,    0,    5,    // 39
     -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Ternary2) {
    ccCompiledScript *scrip = newScriptFixture();

    // Accept Elvis operator expression

    char *inpl = "\
    managed struct Struct       \n\
    {                           \n\
        int Payload;            \n\
    } S, T;                     \n\
                                \n\
    void main()                 \n\
    {                           \n\
        S = null;               \n\
        T = new Struct;         \n\
        Struct Res = S ?: T;    \n\
    }                           \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Ternary2", scrip);
    const size_t codesize = 44;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,    6,    2,    0,    // 7
      47,    3,   73,    3,            4,    6,    2,    4,    // 15
      47,    3,    6,    2,            0,   48,    3,   70,    // 23
       5,    6,    2,    4,           48,    3,   51,    0,    // 31
      47,    3,    1,    1,            4,   51,    4,   49,    // 39
       2,    1,    4,    5,          -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 4;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       7,   15,   20,   27,        -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,   1,   1,     '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Ternary3) {
    ccCompiledScript *scrip = newScriptFixture();

    // Accept nested expression

    char *inpl = "\
    int main()                  \n\
    {                           \n\
        int t1 = 15;            \n\
        int t2 = 16;            \n\
        return t1 < 0 ? (t1 > 15 ? t2 : t1) : 99;     \n\
    }                           \n\
    ";

    clear_error();
    int compileResult = cc_compile(inpl, scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Ternary3", scrip);
    const size_t codesize = 77;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,           15,   29,    3,    6,    // 7
       3,   16,   29,    3,           51,    8,    7,    3,    // 15
      29,    3,    6,    3,            0,   30,    4,   18,    // 23
       4,    3,    3,    4,            3,   28,   31,   51,    // 31
       8,    7,    3,   29,            3,    6,    3,   15,    // 39
      30,    4,   17,    4,            3,    3,    4,    3,    // 47
      28,    6,   51,    4,            7,    3,   31,    4,    // 55
      51,    8,    7,    3,           31,    3,    6,    3,    // 63
      99,    2,    1,    8,           31,    6,    2,    1,    // 71
       8,    6,    3,    0,            5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 0;
    EXPECT_EQ(numfixups, scrip->numfixups);

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 0;
    EXPECT_EQ(stringssize, scrip->stringssize);
}

TEST(Bytecode, Ternary4) {
    ccCompiledScript *scrip = newScriptFixture();

    // String / literal string and conversion.

    char inpl[] = "\
        String main()                       \n\
        {                                   \n\
            String test = \"Test\";         \n\
            return 2 < 1 ? test : \"Foo\";  \n\
        }                                   \n\
        ";
    std::string input = g_Input_Bool;
    input += g_Input_String;
    input += inpl;

    clear_error();
    int compileResult = cc_compile(input.c_str(), scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("Ternary4", scrip);
    const size_t codesize = 74;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    3,            0,   64,    3,   51,    // 7
       0,   47,    3,    1,            1,    4,    6,    3,    // 15
       2,   29,    3,    6,            3,    1,   30,    4,    // 23
      18,    4,    3,    3,            4,    3,   28,    6,    // 31
      51,    4,   48,    3,           31,    5,    6,    3,    // 39
       5,   64,    3,   29,            3,   51,    4,   47,    // 47
       3,   51,    8,   49,           51,    4,   48,    3,    // 55
      69,   30,    4,    2,            1,    4,   31,    9,    // 63
      51,    4,   49,    2,            1,    4,    6,    3,    // 71
       0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   40,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      3,   3,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 9;
    EXPECT_EQ(stringssize, scrip->stringssize);

    char strings[] = {
    'T',  'e',  's',  't',            0,  'F',  'o',  'o',     // 7
      0,  '\0'
    };

    for (size_t idx = 0; static_cast<int>(idx) < stringssize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->stringssize) break;
        std::string prefix = "strings[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(strings[idx]);
        std::string test_val = prefix + std::to_string(scrip->strings[idx]);
        ASSERT_EQ(is_val, test_val);
    }
}

TEST(Bytecode, AssignToString) {
    ccCompiledScript *scrip = newScriptFixture();

    // Definition of global string with assignment

    char inpl[] = "\
        string Payload = \"Holzschuh\";     \n\
        String main()                       \n\
        {                                   \n\
            String test = Payload;          \n\
            return (~~1 == 2) ? test : Payload;  \n\
        }                                   \n\
        ";
    std::string input = g_Input_Bool;
    input += g_Input_String;
    input += "\n\"__NEWSCRIPTSTART_MAIN\"\n";
    input += inpl;

    ccSetOption(SCOPT_OLDSTRINGS, true);

    clear_error();
    int compileResult = cc_compile(input.c_str(), scrip);
    ASSERT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    // WriteOutput("AssignToString", scrip);
    const size_t codesize = 98;
    EXPECT_EQ(codesize, scrip->codesize);

    intptr_t code[] = {
      38,    0,    6,    2,            0,    3,    2,    3,    // 7
      64,    3,   51,    0,           47,    3,    1,    1,    // 15
       4,    6,    3,    1,            6,    4,   -1,   12,    // 23
       4,    3,    3,    4,            3,    6,    4,   -1,    // 31
      12,    4,    3,    3,            4,    3,   29,    3,    // 39
       6,    3,    2,   30,            4,   15,    4,    3,    // 47
       3,    4,    3,   28,            6,   51,    4,   48,    // 55
       3,   31,    8,    6,            2,    0,    3,    2,    // 63
       3,   64,    3,   29,            3,   51,    4,   47,    // 71
       3,   51,    8,   49,           51,    4,   48,    3,    // 79
      69,   30,    4,    2,            1,    4,   31,    9,    // 87
      51,    4,   49,    2,            1,    4,    6,    3,    // 95
       0,    5,  -999
    };

    for (size_t idx = 0; idx < codesize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->codesize) break;
        std::string prefix = "code[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(code[idx]);
        std::string test_val = prefix + std::to_string(scrip->code[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numfixups = 2;
    EXPECT_EQ(numfixups, scrip->numfixups);

    intptr_t fixups[] = {
       4,   61,  -999
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixups[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixups[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixups[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    char fixuptypes[] = {
      1,   1,  '\0'
    };

    for (size_t idx = 0; idx < numfixups; idx++)
    {
        if (static_cast<int>(idx) >= scrip->numfixups) break;
        std::string prefix = "fixuptypes[";
        prefix += std::to_string(idx) + "] == ";
        std::string   is_val = prefix + std::to_string(fixuptypes[idx]);
        std::string test_val = prefix + std::to_string(scrip->fixuptypes[idx]);
        ASSERT_EQ(is_val, test_val);
    }

    const int numimports = 0;
    std::string imports[] = {
     "[[SENTINEL]]"
    };

    int idx2 = -1;
    for (size_t idx = 0; static_cast<int>(idx) < scrip->numimports; idx++)
    {
        if (!strcmp(scrip->imports[idx], ""))
            continue;
        idx2++;
        ASSERT_LT(idx2, numimports);
        std::string prefix = "imports[";
        prefix += std::to_string(idx2) + "] == ";
        std::string is_val = prefix + scrip->imports[idx];
        std::string test_val = prefix + imports[idx2];
        ASSERT_EQ(is_val, test_val);
    }

    const size_t numexports = 0;
    EXPECT_EQ(numexports, scrip->numexports);

    const size_t stringssize = 10;
    EXPECT_EQ(stringssize, scrip->stringssize);

    char strings[] = {
    'H',  'o',  'l',  'z',          's',  'c',  'h',  'u',     // 7
    'h',    0,  '\0'
    };

    for (size_t idx = 0; static_cast<int>(idx) < stringssize; idx++)
    {
        if (static_cast<int>(idx) >= scrip->stringssize) break;
        std::string prefix = "strings[";
        prefix += std::to_string(idx) + "] == ";
        std::string is_val = prefix + std::to_string(strings[idx]);
        std::string test_val = prefix + std::to_string(scrip->strings[idx]);
        ASSERT_EQ(is_val, test_val);
    }
}

TEST(Bytecode, StackMisalign) {
    ccCompiledScript *scrip = newScriptFixture();

    std::ifstream t("C:/TEMP/SetLastnFurious/Vehicle.asc");
    std::string input((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

    clear_error();
    int compileResult = cc_compile(input.c_str(), scrip);
    EXPECT_STREQ("Ok", (compileResult >= 0) ? "Ok" : last_seen_cc_error());

    WriteOutput("StackMisalign", scrip);
}
