
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
#include <map>
#include <cstdint>

class Huffman
{
public:
    // Compress the input buffer using Huffman coding and return the compressed bytes.
    // Input: buffer with file contents.
    // Output: buffer with compressed data. Current implementation may be a passthrough
    // until a real compressor is implemented.
    static std::vector<char> HuffmanCompression(const std::vector<char> &input);

    // Decompress a buffer produced by HuffmanCompression using freqTable.bin metadata
    static std::vector<char> HuffmanDecompression(const std::vector<char> &compressed);

    // Simple helper to read raw buffer from a file
    static std::vector<char> readUncompressedFile(const std::string &path);
    static bool writeFile(const std::string &path, const std::vector<char> &data);

    // Destructor and default constructor are fine as the defaults.
    Huffman() = default;
    ~Huffman() = default;

private:
    // Helper method to generate Huffman codes
    static void generateCodes(class NodeLetter *node, std::string code, std::map<char, std::string> &huffmanCodes);

    // Helper to read freqTable.bin and rebuild the Huffman tree
    static bool loadFreqAndBuildTree(const std::string &path,
                                     std::vector<std::pair<char, int>> &freq,
                                     uint8_t &pad,
                                     uint32_t &originalSize,
                                     class NodeLetter *&root);

    // (no duplicate declarations)
};

#endif // HUFFMAN_H
