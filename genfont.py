#!/usr/bin/env python
import sys
import io

if len(sys.argv) > 1 and sys.argv[1] != '-':
	stream = io.open(sys.argv[1], "r")
	print('// from {}'.format(sys.argv[1]))
else:
	stream = sys.stdin

codepoint_list = []

while True:
	line = stream.readline()

	if not line:
		break

	if line.startswith('ENCODING '):
		codepoint = int(line[9:])
		binary = chr(codepoint & 0xFF) + chr(codepoint >> 8)
		codepoint_list.append(binary)
		#sys.stdout.seek(codepoint * 16)
		continue

	if line.startswith('BITMAP'):
		for y in range(0,16):
			line = stream.readline()
			# parse hex byte
			data = int(line, 16)
			sys.stdout.write(chr(data))

for binary in codepoint_list:
	sys.stdout.write(binary)
