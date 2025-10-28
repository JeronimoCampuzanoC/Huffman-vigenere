
/*
 * Huffman.h
 *
 * Minimal header declaring the Huffman class used for compression.
 * It exposes a single public method:
 *   void HuffmanCompression(const std::string &filePath);
 *
 * The method takes a file path to compress and returns nothing.
 */

#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <string>
#include <vector>

class Huffman
{
public:
    // Compress the input buffer using Huffman coding and return the compressed bytes.
    // Input: buffer with file contents.
    // Output: buffer with compressed data. Current implementation may be a passthrough
    // until a real compressor is implemented.
    static std::vector<char> HuffmanCompression(const std::vector<char> &input);

    // Destructor and default constructor are fine as the defaults.
    Huffman() = default;
    ~Huffman() = default;
};

#endif // HUFFMAN_H
