import glob
import os
import shutil
import subprocess
import sys


def cleanup(out: str) -> str:
  parts = []
  for line in out.splitlines():
    if len(line) > 1 and line[0] == '#':
      continue
    parts.append("".join(line.split()))
  return "".join(parts)


# Check for required compilers and exit if any are missing
CLANG_EXE = shutil.which('clang')
if not CLANG_EXE:
  sys.exit('Failed to run tests: clang compiler not found')

GCC_EXE = shutil.which('gcc')
if not GCC_EXE:
  sys.exit('Failed to run tests: gcc compiler not found')

SIMPLECPP_EXE = './simplecpp'


commands = []

for f in sorted(glob.glob(os.path.expanduser('testsuite/clang-preprocessor-tests/*.c*'))):
  for line in open(f, 'rt', encoding='utf-8'):
    if line.startswith('// RUN: %clang_cc1 '):
      cmd = ''
      for arg in line[19:].split():
        if arg == '-E' or (len(arg) >= 3 and arg[:2] == '-D'):
          cmd = cmd + ' ' + arg
      if len(cmd) > 1:
        newcmd = cmd[1:] + ' ' + f
        if not newcmd in commands:
          commands.append(cmd[1:] + ' ' + f)
for f in sorted(glob.glob(os.path.expanduser('testsuite/gcc-preprocessor-tests/*.c*'))):
  commands.append('-E ' + f)

# skipping tests..
skip = ['assembler-with-cpp.c',
        'builtin_line.c',
        'c99-6_10_3_3_p4.c',
        'clang_headers.c', # missing include <limits.h>
        'comment_save.c', # _Pragma
        'has_attribute.c',
        'has_attribute.cpp',
        'header_lookup1.c', # missing include <stddef.h>
        'line-directive-output.c',
        #  'macro_paste_hashhash.c',
        'microsoft-ext.c',
        'normalize-3.c', # gcc has different output \uAC00 vs \U0000AC00 on cygwin/linux
        'pr63831-1.c', # __has_attribute => works differently on cygwin/linux
        'pr63831-2.c', # __has_attribute => works differently on cygwin/linux
        'pr65238-1.c', # __has_attribute => works differently on cygwin/linux
        '_Pragma-location.c',
        '_Pragma-dependency.c',
        '_Pragma-dependency2.c',
        '_Pragma-physloc.c',
        'pragma-pushpop-macro.c', # pragma push/pop
        'x86_target_features.c',
        'warn-disabled-macro-expansion.c',
        'ucnid-2011-1.c' # \u00A8 generates different output on cygwin/linux
       ]

todo = [
         # todo, low priority: wrong number of macro arguments, pragma, etc
         'macro_backslash.c',
         'macro_fn_comma_swallow.c',
         'macro_fn_comma_swallow2.c',
         'macro_expand.c',
         'macro_fn_disable_expand.c',
         'macro_paste_commaext.c',
         'macro_paste_hard.c',
         'macro_rescan_varargs.c',

         # todo, high priority
         'c99-6_10_3_4_p5.c',
         'c99-6_10_3_4_p6.c',
         'stdint.c',

         # GCC..
         'diagnostic-pragma-1.c',
         'pr45457.c',
         'pr57580.c',
         ]


def run(compiler_executable: str, compiler_args: list[str]) -> tuple[int, str, str, str]:
  """Execute a compiler command and capture its exit code, stdout, and stderr."""
  compiler_cmd = [compiler_executable]
  compiler_cmd.extend(compiler_args)

  with subprocess.Popen(compiler_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8") as process:
    stdout, stderr = process.communicate()
    exit_code = process.returncode

  output = cleanup(stdout)
  error = (stderr or "").strip()
  return (exit_code, output, stdout, error)


numberOfSkipped = 0
numberOfFailed = 0
numberOfFixed = 0

usedTodos = []

for cmd in commands:
  if cmd[cmd.rfind('/')+1:] in skip:
    numberOfSkipped = numberOfSkipped + 1
    continue

  _, clang_output_c, clang_output, _ = run(CLANG_EXE, cmd.split(' '))

  _, gcc_output_c, gcc_output, _ = run(GCC_EXE, cmd.split(' '))

  # -E is not supported and we bail out on unknown options
  simplecpp_ec, simplecpp_output_c, simplecpp_output, simplecpp_err = run(SIMPLECPP_EXE, cmd.replace('-E ', '', 1).split(' '))

  if simplecpp_output_c != clang_output_c and simplecpp_output_c != gcc_output_c:
    filename = cmd[cmd.rfind('/')+1:]
    if filename in todo:
      print('TODO ' + cmd)
      usedTodos.append(filename)
    else:
      print('FAILED ' + cmd)
      print('---expected (clang):')
      print(clang_output_c)
      print('---expected (gcc):')
      print(gcc_output_c)
      print('---actual:')
      print(simplecpp_output_c)
      print('---output (clang):')
      print(clang_output)
      print('---output (gcc):')
      print(gcc_output)
      print('---output (simplecpp):')
      print(simplecpp_output)
      if simplecpp_ec:
          print('simplecpp failed - ' + simplecpp_err)
      numberOfFailed = numberOfFailed + 1

for filename in todo:
    if not filename in usedTodos:
        print('FIXED ' + filename)
        numberOfFixed = numberOfFixed + 1

print('Number of tests: ' + str(len(commands)))
print('Number of skipped: ' + str(numberOfSkipped))
print('Number of todos (fixed): ' + str(len(usedTodos)) + ' (' + str(numberOfFixed) + ')')
print('Number of failed: ' + str(numberOfFailed))

if numberOfFailed or numberOfFixed:
    sys.exit(1)

sys.exit(0)
