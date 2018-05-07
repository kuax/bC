# bC

This is a school project in which we had to develop a lossless general-purpose file compressor.

Developed by [@kuax](https://github.com/kuax) and [@ChristianPao](https://github.com/ChristianPao), back in the day Git wasn't yet in our toolbox :)

## Algorithms

We ended up creating a pipeline that first uses a custom LZ77/SS implementation as a first step in compression,
and subsequently passes 4 files to a custom Huffman implementation for the final compression.

## Results

On the files given to test our algorithm, compression ratio results were max 10% above a standard ZIP compression
of the same file (see documentation), yet time-wise our algorithm did take a bit longer...
