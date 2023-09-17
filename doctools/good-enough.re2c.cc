// Good Enough Syntax Recognition

// Motivation:
//
// - The Github source viewer is too slow.  We want to publish a fast version
//   of our source code to view.
//   - We need to link source code from Oils docs.
// - Aesthetics
//   - I don't like noisy keyword highlighting.  Just comments and string
//     literals looks surprisingly good.
//   - Can use this on the blog too.
// - YSH needs syntax highlighters, and this code is a GUIDE to writing one.
//   - The lexer should run on its own.  Generated parsers like TreeSitter
//     require such a lexer.  In contrast to recursive descent, grammars can't
//     specify lexer modes.
// - I realized that "sloccount" is the same problem as syntax highlighting --
//   you exclude comments, whitespace, and lines with only string literals.
//   - sloccount is a huge Perl codebase, and we can stop depending on that.
// - Because re2c is fun, and I wanted to experiment with writing it directly.
// - Ideas
//   - use this on your blog?
//   - embed in a text editor?

// Later:
// - Extract declarations, and navigate to source.  This may be another step
//   that processes the TSV file.

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>  // va_list, etc.
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>  // free
#include <string.h>

const char* RESET = "\x1b[0;0m";
const char* BOLD = "\x1b[1m";
const char* REVERSE = "\x1b[7m";  // reverse video

const char* RED = "\x1b[31m";
const char* GREEN = "\x1b[32m";
const char* YELLOW = "\x1b[33m";
const char* BLUE = "\x1b[34m";
const char* PURPLE = "\x1b[35m";

void Log(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputs("\n", stderr);
}

void die(const char* message) {
  fprintf(stderr, "good-enough: %s\n", message);
  exit(1);
}

enum class lang_e {
  Unspecified,

  Py,
  Shell,
  Ysh,  // ''' etc.

  Cpp,  // including C
  R,    // uses # comments
  JS,   // uses // comments
};

enum class Id {
  Comm,
  WS,  // TODO: indent, dedent

  Name,  // foo

  DQ,  // "" and Python r""
  SQ,  // '' and Python r''

  TripleSQ,  // '''
  TripleDQ,  // """

  // Hm I guess we also need r''' and """ ?

  Other,  // any other text
  Unknown,
};

struct Token {
  Id kind;
  int end_col;
};

enum class py_mode_e {
  Outer,    // default
  MultiSQ,  // inside '''
  MultiDQ,  // inside """
};

enum class cpp_mode_e {
  Outer,  // default
  Comm,   // inside /* */ comment
};

enum class sh_mode_e {
  Outer,  // default

  SQ,        // inside multi-line ''
  DollarSQ,  // inside multi-line $''
  DQ,        // inside multi-line ""

  HereSQ,  // inside <<'EOF'
  HereDQ,  // inside <<EOF

  // We could have a separate thing for this
  YshSQ,  // inside '''
  YshDQ,  // inside """
  YshJ,   // inside j"""
};

// Lexer and Matcher are specialized on py_mode_e, cpp_mode_e, ...

template <typename T>
class Lexer {
 public:
  Lexer(char* line) : line_(line), p_current(line), line_mode(T::Outer) {
  }

  void SetLine(char* line) {
    line_ = line;
    p_current = line;
  }

  const char* line_;
  const char* p_current;  // points into line
  T line_mode;            // current mode, starts with Outer
};

template <typename T>
class Matcher {
 public:
  // Returns whether EOL was hit.  Mutates lexer state, and fills in tok out
  // param.
  bool Match(Lexer<T>* lexer, Token* tok);
};

// Macros for semantic actions

#define TOK(k)   \
  tok->kind = k; \
  break;
#define TOK_MODE(k, m)  \
  tok->kind = k;        \
  lexer->line_mode = m; \
  break;

// Regex definitions shared between languages

/*!re2c
  re2c:yyfill:enable = 0;
  re2c:define:YYCTYPE = char;
  re2c:define:YYCURSOR = p;

  nul = [\x00];
  not_nul = [^\x00];

  identifier = [_a-zA-Z][_a-zA-Z0-9]*;

  // Shell and Python have # comments
  pound_comment        = "#" not_nul*;

  // YSH and Python have ''' """
  triple_sq = "'''";
  triple_dq = ["]["]["];
*/

// Returns whether EOL was hit
template <>
bool Matcher<py_mode_e>::Match(Lexer<py_mode_e>* lexer, Token* tok) {
  const char* p = lexer->p_current;  // mutated by re2c
  const char* YYMARKER = p;

  switch (lexer->line_mode) {
  case py_mode_e::Outer:
    while (true) {
      /*!re2c
        nul                    { return true; }

        // optional raw prefix
        [r]? triple_sq         { TOK_MODE(Id::TripleSQ, py_mode_e::MultiSQ); }
        [r]? triple_dq         { TOK_MODE(Id::TripleDQ, py_mode_e::MultiDQ); }

        identifier             { TOK(Id::Name); }

        sq_middle = ( [^\x00'\\] | "\\" not_nul )*;
        dq_middle = ( [^\x00"\\] | "\\" not_nul )*;

        [r]? ['] sq_middle ['] { TOK(Id::SQ); }
        [r]? ["] dq_middle ["] { TOK(Id::DQ); }

        pound_comment          { TOK(Id::Comm); }

        // Whitespace is needed for SLOC, to tell if a line is entirely blank
        // TODO: Also compute INDENT DEDENT tokens

        whitespace = [ \t\r\n]*;
        whitespace             { TOK(Id::WS); }

        // Not the start of a string, comment, identifier
        [^\x00"'#_a-zA-Z]+     { TOK(Id::Other); }

        // e.g. unclosed quote like "foo
        *                      { TOK(Id::Unknown); }

      */
    }
    break;

  case py_mode_e::MultiSQ:
    while (true) {
      /*!re2c
        nul       { return true; }

        triple_sq { TOK_MODE(Id::TripleSQ, py_mode_e::Outer); }

        [^\x00']* { TOK(Id::TripleSQ); }

        *         { TOK(Id::TripleSQ); }

      */
    }
    break;

  case py_mode_e::MultiDQ:
    while (true) {
      /*!re2c
        nul       { return true; }

        triple_dq { TOK_MODE(Id::TripleDQ, py_mode_e::Outer); }

        [^\x00"]* { TOK(Id::TripleDQ); }

        *         { TOK(Id::TripleDQ); }

      */
    }
    break;
  }

  tok->end_col = p - lexer->line_;
  lexer->p_current = p;
  return false;
}

// Returns whether EOL was hit
template <>
bool Matcher<cpp_mode_e>::Match(Lexer<cpp_mode_e>* lexer, Token* tok) {
  const char* p = lexer->p_current;  // mutated by re2c
  const char* YYMARKER = p;

  switch (lexer->line_mode) {
  case cpp_mode_e::Outer:
    while (true) {
      /*!re2c
        nul                    { return true; }

        // optional raw prefix
        [r]? triple_sq         { TOK_MODE(Id::TripleSQ, cpp_mode_e::Comm); }
        [r]? triple_dq         { TOK_MODE(Id::TripleDQ, cpp_mode_e::Comm); }

        identifier             { TOK(Id::Name); }

        // sq_middle = ( [^\x00'\\] | "\\" not_nul )*;
        // dq_middle = ( [^\x00"\\] | "\\" not_nul )*;

        [r]? ['] sq_middle ['] { TOK(Id::SQ); }
        [r]? ["] dq_middle ["] { TOK(Id::DQ); }

        pound_comment          { TOK(Id::Comm); }

        // Whitespace is needed for SLOC, to tell if a line is entirely blank
        // TODO: Also compute INDENT DEDENT tokens

        // whitespace = [ \t\r\n]*;
        whitespace             { TOK(Id::WS); }

        // Not the start of a string, comment, identifier
        [^\x00"'#_a-zA-Z]+     { TOK(Id::Other); }

        // e.g. unclosed quote like "foo
        *                      { TOK(Id::Unknown); }

      */
    }
    break;

  case cpp_mode_e::Comm:
    assert(0);
    break;
  }

  tok->end_col = p - lexer->line_;
  lexer->p_current = p;
  return false;
}

class Reader {
  // We don't care about internal NUL, so this interface doesn't allow it

 public:
  Reader(FILE* f) : f_(f), line_(nullptr), allocated_size_(0) {
  }

  bool NextLine() {
    // Returns true if it put a line in the Reader, or false for EOF.  Handles
    // I/O errors by printing to stderr.

    // Note: getline() frees the previous line, so we don't have to
    ssize_t len = getline(&line_, &allocated_size_, f_);
    // Log("len = %d", len);

    if (len < 0) {  // EOF is -1
      // man page says the buffer should be freed if getline() fails
      free(line_);

      line_ = nullptr;  // tell the caller not to continue

      if (errno != 0) {  // I/O error
        err_num_ = errno;
        return false;
      }
    }
    return true;
  }

  char* Current() {
    return line_;
  }

  FILE* f_;

  char* line_;  // valid for one NextLine() call, nullptr on EOF or error
  size_t allocated_size_;  // unused, but must pass address to getline()
  int err_num_;            // set on error
};

class Printer {
 public:
  virtual void Print(char* line, int line_num, int start_col, Token token) = 0;
  virtual ~Printer() {
  }
};

class AnsiPrinter : public Printer {
 public:
  AnsiPrinter(bool more_color) : Printer(), more_color_(more_color) {
  }

  virtual void Print(char* line, int line_num, int start_col, Token tok) {
    char* p_start = line + start_col;
    int num_bytes = tok.end_col - start_col;
    switch (tok.kind) {
    case Id::Comm:
      fputs(BLUE, stdout);
      fwrite(p_start, 1, num_bytes, stdout);
      fputs(RESET, stdout);
      break;

    case Id::Name:
      fwrite(p_start, 1, num_bytes, stdout);
      break;

    case Id::Other:
      if (more_color_) {
        fputs(PURPLE, stdout);
      }
      fwrite(p_start, 1, num_bytes, stdout);
      if (more_color_) {
        fputs(RESET, stdout);
      }
      break;

    case Id::DQ:
    case Id::SQ:
      fputs(RED, stdout);
      fwrite(p_start, 1, num_bytes, stdout);
      fputs(RESET, stdout);
      break;

    case Id::TripleSQ:
    case Id::TripleDQ:
      fputs(GREEN, stdout);
      fwrite(p_start, 1, num_bytes, stdout);
      fputs(RESET, stdout);
      break;

    case Id::Unknown:
      // Make errors red
      fputs(REVERSE, stdout);
      fputs(RED, stdout);
      fwrite(p_start, 1, num_bytes, stdout);
      fputs(RESET, stdout);
      break;
    default:
      fwrite(p_start, 1, num_bytes, stdout);
      break;
    }
  }
  virtual ~AnsiPrinter() {
  }

 private:
  bool more_color_;
};

const char* Id_str(Id id) {
  switch (id) {
  case Id::Comm:
    return "Comm";
  case Id::WS:
    return "WS";
  case Id::Name:
    return "Name";
  case Id::Other:
    return "Other";
  case Id::DQ:
    return "DQ";
  case Id::SQ:
    return "SQ";
  case Id::TripleSQ:
    return "TripleSQ";
  case Id::TripleDQ:
    return "TripleDQ";
  case Id::Unknown:
    return "Unknown";
  default:
    assert(0);
  }
}

class TsvPrinter : public Printer {
 public:
  virtual void Print(char* line, int line_num, int start_col, Token tok) {
    printf("%d\t%s\t%d\t%d\n", line_num, Id_str(tok.kind), start_col,
           tok.end_col);
    // printf("  -> mode %d\n", lexer.line_mode);
  }
  virtual ~TsvPrinter() {
  }
};

struct Flags {
  lang_e lang;
  bool tsv;
  bool more_color;

  int argc;
  char** argv;
};

// This templated method causes some code expansion, but not too much.  The
// binary went from 38 KB to 42 KB, after being stripped.
// We get a little type safety with py_mode_e vs cpp_mode_e.

template <typename T>
int GoodEnough(const Flags& flag) {
  Reader reader(stdin);

  Lexer<T> lexer(nullptr);
  Matcher<T> matcher;

  Printer* pr;
  if (flag.tsv) {
    pr = new TsvPrinter();
  } else {
    pr = new AnsiPrinter(flag.more_color);
  }

  int line_num = 1;
  int num_sig = 0;

  while (true) {  // read each line, handling errors
    if (!reader.NextLine()) {
      Log("getline() error: %s", strerror(reader.err_num_));
      return 1;
    }
    char* line = reader.Current();
    if (line == nullptr) {
      break;  // EOF
    }
    lexer.SetLine(line);
    // Log("line = %s", line);

    int start_col = 0;
    bool is_significant = false;
    while (true) {  // tokens on each line
      Token tok;
      bool eol = matcher.Match(&lexer, &tok);
      if (eol) {
        break;
      }
      pr->Print(line, line_num, start_col, tok);
      start_col = tok.end_col;

      switch (tok.kind) {
      // Comments, whitespace, and string literals aren't significant
      case Id::Name:
      case Id::Other:
        is_significant = true;
        break;

      // TODO: can abort on Id::Unknown?
      default:
        break;
      }
    }
    line_num += 1;
    num_sig += is_significant;
  }

  Log("%d lines, %d significant", line_num - 1, num_sig);

  delete pr;

  return 0;
}

void PrintHelp() {
  puts(R"(Usage: good-enough FLAGS*

Recognizes the syntax of the text on stdin, and prints it to stdout.

Flags:

  -l    Language: py|cpp
  -m    More color
  -t    Print tokens as TSV, instead of ANSI color

  -h    This help
)");
}

int main(int argc, char** argv) {
  // Outputs:
  // - syntax highlighting
  // - SLOC - (file, number), number of lines with significant tokens
  // - LATER: parsed definitions, for now just do line by line
  //   - maybe do a transducer on the tokens

  Flags flag = {lang_e::Unspecified};

  // http://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
  // + means to be strict about flag parsing.
  int c;
  while ((c = getopt(argc, argv, "+hl:mt")) != -1) {
    switch (c) {
    case 'h':
      PrintHelp();
      return 0;

    case 'l':
      if (strcmp(optarg, "py") == 0) {
        flag.lang = lang_e::Py;

      } else if (strcmp(optarg, "cpp") == 0) {
        flag.lang = lang_e::Cpp;

      } else {
        Log("Expected -l LANG to be py|cpp, got %s", optarg);
        return 2;
      }
      break;

    case 'm':
      flag.more_color = true;
      break;

    case 't':
      flag.tsv = true;
      break;

    case '?':  // getopt library will print error
      return 2;

    default:
      abort();  // should never happen
    }
  }

  int a = optind;  // index into argv
  flag.argv = argv + a;
  flag.argc = argc - a;

  switch (flag.lang) {
  case lang_e::Py:
    return GoodEnough<py_mode_e>(flag);

  case lang_e::Cpp:
    return GoodEnough<cpp_mode_e>(flag);

  default:
    return GoodEnough<py_mode_e>(flag);
  }
}