#!/usr/bin/env python

# Check that the input file has no external include guards.
# Returns with 0 exit code on success, 1 otherwise.

import re
import sys
import subprocess

def git(*args, **kwargs):
    return subprocess.check_output(['git'] + list(args), **kwargs)

def get_changed_paths(filter):
    output = git('diff', '--cached', '--name-only', '-z', '--diff-filter='+filter)
    return output.split('\0')[:-1] # remove trailing ''

def get_against():
    try:
        head = git('rev-parse', '--verify', 'HEAD', stderr=None)
    except subprocess.CalledProcessError:
        # Initial commit: diff against an empty tree object
        return '4b825dc642cb6eb9a060e54bf8d69288fbee4904'
    else:
        return 'HEAD'

against = get_against()

success = True

def croak(path, line, msg, *args):
    global success
    success = False
    if path is not None:
        sys.stderr.write("%s:%d: " % (path, line or 0))
    sys.stderr.write(msg % args if args else msg)
    if msg[-1] != '\n':
        sys.stderr.write('\n')

def check_filenames():
    try:
        allownonascii = git('config', '--get', '--bool', 'hooks.allownonascii')
    except subprocess.CalledProcessError:
        pass
    else:
        if allownonascii == 'true':
            return

    for path in get_changed_paths('ACR'):
        try:
            path.decode('ascii')
        except UnicodeDecodeError:
            croak(path, 0, "Non-ASCII file name")

def check_whitespace():
    try:
        git('diff-index', '--check', '--cached', against, stderr=None)
    except subprocess.CalledProcessError as e:
        sys.stderr.write(e.output)
        global success
        success = False

guard_re = re.compile('^[ \t]*#\s*ifndef\s+_.*?_H(PP)?\n'
                      '\s*#\s*include\s+(".*?")\s*\n'
                      '\s*#\s*endif.*?$',
                      re.MULTILINE)

def check_external_guards (infile):
    contents = infile.read()
    for m in guard_re.finditer(contents):
        lineno = 1 + contents[:m.start()].count('\n')
        croak(infile.name, lineno, "External include guard")
        croak(None, None, m.group(0))

def check_changed_files():
    for path in get_changed_paths('AM'):
        check_external_guards(open(path))

def main():
    check_filenames()
    check_changed_files()
    check_whitespace()
    if not success:
        exit(1)

if __name__ == '__main__':
    main()
