Basic Implementation of LZ78 Compression Alghorithm

## Installation

make

## Typical usage (compression):

./lz78 -i inputfile -o outputfile

## Typical usage (decompression):

./lz78 -i inputfile -o outputfile -d

## Typical pipelined execution

echo "Hello World" | ./lz78 | ./lz78 -d
