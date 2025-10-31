# Huffman-Vigenere Tool

This repository contains a small C++ project with a Huffman compression stub and a CLI pipeline for compressing/encrypting files.

Files:

- [cli_layout.cpp](cli_layout.cpp) — CLI, thread pool and pipeline (contains `parse_args`, `run_pipeline`, `map_output_path`, `ThreadPool`, `read_all`, `write_all`, `xor_encrypt`).
- [Huffman.cpp](Huffman.cpp) — Huffman implementation (contains [`Huffman::HuffmanCompression`](Huffman.cpp), [`Huffman::HuffmanDecompression`](Huffman.cpp), [`Huffman::readUncompressedFile`](Huffman.cpp), [`Huffman::writeFile`](Huffman.cpp), [`Huffman::generateCodes`](Huffman.cpp), [`Huffman::loadFreqAndBuildTree`](Huffman.cpp)).
- [Huffman.h](Huffman.h) — public declarations for the `Huffman` class.
- [NodeLetter.h](NodeLetter.h) — tree node type and `deleteTree`.
- [main.cpp](main.cpp) — small demo that calls the compressor/decompressor.

Requirements

- C++17 compiler (g++ or clang++)
- pthread (for multithreading)
- Filesystem support (std::filesystem)

Build

1. Build the CLI tool (recommended):

```sh
g++ -std=c++17 -O2 -pthread [cli_layout.cpp](http://_vscodecontentref_/0) [Huffman.cpp](http://_vscodecontentref_/1) -o clitool
```
