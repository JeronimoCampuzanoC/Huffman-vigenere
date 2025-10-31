#include <iostream>
#include <vector>
#include <string>
#include "Huffman.h"
using namespace std;

int main()
{
    // Sample input data
    const string sample = "This is a small test string to exercise Huffman compression stub.\n"
                               "Repeat: This is a small test string to exercise Huffman compression stub.\n";
    vector<char> input(sample.begin(), sample.end());

    cout << "Original size: " << input.size() << " bytes\n";

    // Call the Huffman compressor (static method)
    vector<char> out = Huffman::HuffmanCompression(input);

    cout << "Compressed size: " << out.size() << " bytes\n";

    // Print first up to 64 bytes as hex for quick inspection
    cout << "First bytes (hex, up to 64): ";
    size_t to_print = min<size_t>(out.size(), 64);
    for (size_t i = 0; i < to_print; ++i)
    {
        unsigned char c = static_cast<unsigned char>(out[i]);
        cout << hex << (c >> 4) << (c & 0xF);
    }
    cout << dec << "\n";


    // Decompress using metadata written to freqTable.bin and verify
    vector<char> restored = Huffman::HuffmanDecompression(out);
    cout << "Decompressed size: " << restored.size() << " bytes\n";

    bool ok = (restored == input);
    cout << "Round-trip OK? " << boolalpha << ok << "\n";

    // write decompressed to disk, then read it back and verify
    Huffman::writeFile("uncompressed.txt", restored);
    
    //read the uncompressed file and verify
    vector<char> uncompressed = Huffman::readUncompressedFile("uncompressed.txt");

    return 0;
}
