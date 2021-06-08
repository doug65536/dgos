#!/usr/bin/env python
import sys
import io
import os
import tempfile
import subprocess
import uuid
import struct

debugging = False

def debug(message):
    global debugging
    if debugging:
        sys.stderr.write(message + '\n')

def error(message):
    sys.stderr.write('error: ' + message + '\n')
    sys.exit(1)

def help(succeed):
    print(
        '-c inputfile [-d] [-- compiler options...]\n' +
        '-d enables debugging output')
    if succeed:
        sys.exit(0)
    else:
        sys.exit(1)

def process_input():
    global debugging
    infile = None
    outfile = None
    compiler_options = []
    gendeps = False

    i = 1
    argc = len(sys.argv)
    while i < argc:
        arg = sys.argv[i]

        debug('arg1={}'.format(arg))

        if arg == '--help':
            help(True)
            sys.exit(0)

        i = i + 1

    i = 1
    while i < argc:
        arg = sys.argv[i]

        debug('arg2={}'.format(arg))

        if arg == '-d':
            debugging = True
        elif arg == '-M':
            gendeps = True
        elif arg == '-c':
            #if infile:
            #    error('input file already specified')
            i = i + 1
            infile = sys.argv[i]
            outfile = infile + '.h'
            debug('infile={}'.format(infile))
        elif arg == '--':
            compiler_options = sys.argv[i+1:]
            debug('Compiler options:\n{}'.format('\n'.join(compiler_options)))
            break
        else:
            error('invalid option: ' + arg)
            help(False)

        i = i + 1

    if infile != '-':
        input_file_path = infile

        src_path = sys.argv[2]
        stream = io.open(input_file_path, 'r')

        input_file_parts = input_file_path.split('/')
        src_parts = src_path.split('/')
        while len(input_file_parts) > 0 and \
                len(src_parts) > 0 and \
                input_file_parts[0] == src_parts[0]:
            input_file_parts.pop(0)
            src_parts.pop(0)

        relative_path = '/'.join(input_file_parts)

        print('// from {}'.format(relative_path))
    else:
        stream = sys.stdin

    fields = None
    types = []
    code = []

    while True:
        line = stream.readline()

        if not line:
            break

        line = line.strip()

        if line == '' or line[0] == '#':
            continue

        if line.startswith('type:'):
            line = line[5:].strip()
            fields = []
            offsets = []

            pack = line.split(' ', 2)

            lenpack = len(pack)

            if lenpack == 2:
                # Explicit name
                type_name, identifier = pack
            elif lenpack == 1:
                # Implicit name
                type_name = pack[0]
                identifier = type_name.upper()
                if identifier[-2:] == '_T':
                    identifier = identifier[0:-2]

            debug('idenfifier {}'.format(identifier))

            type_info = {
                'name': type_name,
                'identifier': identifier,
                'fields': fields,
                'code': code,
                'offsets': offsets
            }

            types.append(type_info)

            continue

        if line.startswith('code:'):
            while True:
                line = stream.readline()
                if not line:
                    break
                code.append(line)
            continue

        if not type:
            continue

        # If it made it to here, we are in a type, read the field declaration

        field = line.split(' ')

        if len(field) == 1:
            # Implicit name
            field.append(field[0].upper())

        fields.append(field)


    for type_info in types:
        with tempfile.NamedTemporaryFile() as tmpsrc:
            for line in type_info['code']:
                tmpsrc.file.write(line + '\n')

            tmpsrc.write('#include <stdint.h>\n')
            tmpsrc.write('asm(".global _start\\n");\n')
            tmpsrc.write('asm("_start: .byte 0xcc\\n");\n')
            tmpsrc.write('#include <stdint.h>\n')
            tmpsrc.write('static size_t const offsets[]' +
                ' __attribute__((__section__(".offsets"), __used__)) = {\n')

            fields = type_info['fields']
            field_count = len(fields)
            for index, field in enumerate(fields):
                tmpsrc.write('    offsetof({}, {}){}\n'.format(
                    type_info['name'], field[0],
                    ',' if index + 1 < field_count
                    else ''))
            tmpsrc.write('};\n')

            tmpsrc.file.flush()

            cxx = os.getenv('CXX', 'x86_64-dgos-g++')
            cwd = os.getcwd()

            output_pathname = cwd + '/genoffsets_temp' + uuid.uuid4().hex

            if not gendeps:
                compiler_args = [
                    cxx,
                    '-nostdlib',
                    '-x', 'c++',
                    '-D__DGOS_OFFSET_GENERATOR__=1',
                    tmpsrc.name,
                    '-o', output_pathname
                ]
            else:
                compiler_args = [
                    cxx,
                    '-x', 'c++',
                    '-MMD',
                    '-E',
                    tmpsrc.name,
                    '-o', output_pathname
                ]

            for option in compiler_options:
                compiler_args.append(option)

            if debugging:
                print(' '.join(compiler_args))
                subprocess.call(['cat', tmpsrc.name])

            compiler_exitcode = subprocess.call(compiler_args)

            if compiler_exitcode != 0:
                error('error: compilation failed')

            objcopy = os.getenv('OBJCOPY', 'x86_64-dgos-objcopy')

            extract_pathname = output_pathname + '.offsets'

            # Extract the .offsets section
            objcopy_args = [
                objcopy,
                '-Obinary',
                '--only-section=.offsets',
                output_pathname,
                extract_pathname
            ]

            subprocess.call(objcopy_args)

            os.unlink(output_pathname)

            with io.open(extract_pathname, "rb") as extracted_offsets:
                while True:
                    bin = bytes(extracted_offsets.read())
                    byte_count = len(bin)
                    b64_count = byte_count / 8
                    if not bin:
                        break
                    unpack_format = '<{}Q'.format(b64_count)
                    extracted_b64 = struct.unpack(unpack_format, bin)
                    for offset in extracted_b64:
                        type_info['offsets'].append(offset)

            os.unlink(extract_pathname)

            print('Generating {}'.format(outfile))

            with io.open(outfile, "w+b") as header:
                header.write('// THIS FILE IS AUTOMATICALLY GENERATED\n')
                if relative_path:
                    header.write('// from {}\n'.format(relative_path))

                sorted_offsets = []
                for index, field in enumerate(type_info['fields']):
                    offset = type_info['offsets'][index]
                    sorted_offsets.append({
                        'index': index,
                        'offset': offset
                    })

                sorted_offsets = sorted(sorted_offsets,
                    key=lambda field: field['offset'])

                prev_line = 0
                for info in sorted_offsets:
                    index = info['index']
                    field = type_info['fields'][index]
                    offset = type_info['offsets'][index]

                    cache_line_index = offset / 64

                    if cache_line_index != prev_line:
                        header.write('\n')

                    prev_line = cache_line_index

                    header.write('#define {}_{}_OFS {}\n'.format(
                        type_info['identifier'], field[1], offset))

process_input()
