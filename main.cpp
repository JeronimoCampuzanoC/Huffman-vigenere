#include <iostream>
#include <vector>
#include <string>
#include "Huffman.h"
using namespace std;

int main()
{
    // Sample input data
    const std::string sample = "This is a small test string to exercise Huffman compression stub.\n"
                               "Repeat: This is a small test string to exercise Huffman compression stub.\n";
    std::vector<char> input(sample.begin(), sample.end());

    std::cout << "Original size: " << input.size() << " bytes\n";

    // Call the Huffman compressor (static method)
    std::vector<char> out = Huffman::HuffmanCompression(input);

    std::cout << "Compressed size: " << out.size() << " bytes\n";

    // Print first up to 64 bytes as hex for quick inspection
    std::cout << "First bytes (hex, up to 64): ";
    size_t to_print = std::min<size_t>(out.size(), 64);
    for (size_t i = 0; i < to_print; ++i)
    {
        unsigned char c = static_cast<unsigned char>(out[i]);
        std::cout << std::hex << (c >> 4) << (c & 0xF);
    }
    std::cout << std::dec << "\n";

    // Basic correctness check (since current stub returns input)
    if (out == input)
    {
        std::cout << "Note: compressor is currently a passthrough stub.\n";
    }
    else
    {
        std::cout << "Compressor returned transformed buffer.\n";
    }

    return 0;
}
