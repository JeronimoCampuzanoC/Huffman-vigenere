#include "Huffman.h"
#include "NodeLetter.h"
#include <map>
#include <algorithm>
#include <utility>
#include <iostream>
#include <fstream>
#include <cstdint>
using namespace std;


std::vector<char> Huffman::HuffmanCompression(const std::vector<char> &input)
{
    // Minimal stub for integration: compute frequencies (example) and
    // return the input as-is. Replace this with a real Huffman encoder.
    vector<pair<char, int>> frequency;

    // Calculate frequency of each character
    for (char c : input)
    {
        bool found = false;
        for (auto &pair : frequency)
        {
            if (pair.first == c)
            {
                pair.second++;
                found = true;
                break;
            }
        }
        if (!found)
        {
            frequency.push_back(make_pair(c, 1));
        }
    }
    // Sort frequency vector ascending by frequency (example analysis step)
    sort(frequency.begin(), frequency.end(), [](const pair<char, int> &a, const pair<char, int> &b)
         { return a.second < b.second; });

    // Create a node for each character (example analysis step)
    vector<NodeLetter *> nodes;
    for (const auto &pair : frequency)
    {
        nodes.push_back(new NodeLetter(pair.second, pair.first));
    }

    do
    {
        // sort nodes by frequency
        sort(nodes.begin(), nodes.end(), [](NodeLetter *a, NodeLetter *b)
             { return a->id < b->id; });
        NodeLetter *newNode = new NodeLetter(
            nodes[0]->id + nodes[1]->id, // sum frequencies
            '\0'                         // no character
        );

        newNode->izq = nodes[0];
        newNode->der = nodes[1];
        nodes.erase(nodes.begin(), nodes.begin() + 2);
        nodes.push_back(newNode);
        // print nodes  for debug
        /*
        for (const auto &node : nodes)
        {
            cout << nodes.size() << " - \n";
            cout << "Node ID: " << node->id << ", Char: " << node->letra << endl;
            // print children
            if (node->izq)
                cout << "  Left Child ID: " << node->izq->id << ", Char: " << node->izq->letra << endl;
            if (node->der)
                cout << "  Right Child ID: " << node->der->id << ", Char: " << node->der->letra << endl;
        }
        */
    } 
    //build the Huffman tree by merging the two nodes with the lowest frequency
    while (nodes.size() > 1);
    // root of the built Huffman tree
    NodeLetter *root = nodes.empty() ? nullptr : nodes[0];
    map<char, string> huffmanCodes;
    generateCodes(root, "", huffmanCodes);
    
    //compress the input
    string bitString;
    for (char c : input)
    {
        bitString += huffmanCodes[c];
    }
    vector<char> compressedInput;
    unsigned char currentByte = 0;
    int bitCount = 0;
    for (char bit : bitString)
    {
        currentByte = (currentByte << 1) | (bit - '0');
        bitCount++;
        if (bitCount == 8)
        {
            compressedInput.push_back(currentByte);
            currentByte = 0;
            bitCount = 0;
        }
    }
    //calculate padding with bits that are left
    uint8_t padding = (bitCount == 0) ? 0 : (uint8_t)(8 - bitCount);

    //if there are left bits, run to the left and write the last byte
    if (bitCount > 0){
        currentByte <<= (8 - bitCount);
        compressedInput.push_back(currentByte);
    }

    /*
    //print codes 
    for (const auto &code : huffmanCodes)
    {
        cout << code.first << " -> " << code.second << endl;
    }
    */
    // Convert string to vector<char> for return


    


    //Save frecuency tree for decompression
    ofstream freqFile("freqTable.bin", ios::binary);

    uint16_t symbolCount = static_cast<uint16_t>(frequency.size());
    freqFile.write(reinterpret_cast<const char*>(&symbolCount), sizeof(symbolCount));

    for (auto& p : frequency) {
        char sym = p.first;
        int32_t freq = p.second;
        freqFile.write(reinterpret_cast<const char*>(&sym),  sizeof(sym));
        freqFile.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
    }

    // add padding and original size for decompression
    uint8_t  pad = padding;
    uint32_t originalSize = static_cast<uint32_t>(input.size());
    freqFile.write(reinterpret_cast<const char*>(&pad),          sizeof(pad));
    freqFile.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));

    freqFile.close();

    //delete memory from the tree recursively
    deleteTree(root);

    return compressedInput;
    
}

void Huffman::generateCodes(NodeLetter *node, string code, map<char, string> &huffmanCodes)
{
    if (!node) return;

    if (node->izq == nullptr && node->der == nullptr)
    {
        huffmanCodes[node->letra] = code.empty() ? "0" : code; // if the code is empty, set it to "0"
        return;
    }
    generateCodes(node->izq, code + "0", huffmanCodes);
    generateCodes(node->der, code + "1", huffmanCodes);
}


bool Huffman::loadFreqAndBuildTree(const string &path,
                                   vector<pair<char, int>> &freq,
                                   uint8_t &pad,
                                   uint32_t &originalSize,
                                   NodeLetter *&root)
{
    ifstream f(path, ios::binary);
    if (!f)
    {
        return false;
    }

    uint16_t symbolCount = 0;
    f.read(reinterpret_cast<char *>(&symbolCount), sizeof(symbolCount));
    if (!f)
    {
        return false;
    }

    freq.clear();
    freq.reserve(symbolCount);
    for (uint16_t i = 0; i < symbolCount; ++i)
    {
        char sym;
        int32_t fr;
        f.read(reinterpret_cast<char *>(&sym), sizeof(sym));
        f.read(reinterpret_cast<char *>(&fr), sizeof(fr));
        if (!f)
        {
            return false;
        }
        freq.push_back({sym, static_cast<int>(fr)});
    }

    f.read(reinterpret_cast<char *>(&pad), sizeof(pad));
    f.read(reinterpret_cast<char *>(&originalSize), sizeof(originalSize));
    if (!f)
    {
        return false;
    }

    // Build Huffman tree using the same strategy as compression (sort by frequency ascending)
    vector<NodeLetter *> nodes;
    nodes.reserve(freq.size());
    for (auto &p : freq)
    {
        nodes.push_back(new NodeLetter(p.second, p.first));
    }

    if (nodes.empty())
    {
        root = nullptr;
        return true;
    }

    while (nodes.size() > 1)
    {
        sort(nodes.begin(), nodes.end(), [](NodeLetter *a, NodeLetter *b)
                  { return a->id < b->id; });
        NodeLetter *left = nodes[0];
        NodeLetter *right = nodes[1];
        NodeLetter *parent = new NodeLetter(left->id + right->id, '\0');
        parent->izq = left;
        parent->der = right;
        nodes.erase(nodes.begin(), nodes.begin() + 2);
        nodes.push_back(parent);
    }

    root = nodes[0];
    return true;
}

// Decompression function that uses the frequency table to decompress the compressed file
vector<char> Huffman::HuffmanDecompression(const vector<char> &compressed)
{
    vector<pair<char, int>> freq;
    uint8_t pad = 0;
    uint32_t originalSize = 0;
    NodeLetter *root = nullptr;

    if (!loadFreqAndBuildTree("freqTable.bin", freq, pad, originalSize, root))
    {
        return {};
    }

    vector<char> output;
    output.reserve(originalSize);

    if (!root)
    {
        return output;
    }

    size_t totalBits = compressed.size() * 8;
    if (pad > 0 && totalBits >= pad)
    {
        totalBits -= pad;
    }

    NodeLetter *node = root;
    size_t bitIndex = 0;
    for (size_t i = 0; i < compressed.size() && output.size() < originalSize; ++i)
    {
        unsigned char byte = static_cast<unsigned char>(compressed[i]);
        for (int b = 7; b >= 0 && bitIndex < totalBits && output.size() < originalSize; --b, ++bitIndex)
        {
            int bit = (byte >> b) & 1;
            node = bit == 0 ? node->izq : node->der;
            if (node->izq == nullptr && node->der == nullptr)
            {
                output.push_back(node->letra);
                node = root;
            }
        }
    }

    deleteTree(root);
    return output;
}

//function to read the uncompressed file
vector<char> Huffman::readUncompressedFile(const string &path)
{
    ifstream file(path, ios::binary);
    if (!file)
    {
        return {};
    }
    file.seekg(0, ios::end);
    streampos end = file.tellg();
    if (end < 0)
    {
        return {};
    }
    size_t size = static_cast<size_t>(end);
    file.seekg(0, ios::beg);
    vector<char> data(size);
    if (size > 0)
    {
        file.read(data.data(), static_cast<streamsize>(size));
    }
    return data;
}

bool Huffman::writeFile(const string &path, const vector<char> &data)
{
    ofstream out(path, ios::binary);
    if (!out)
    {
        return false;
    }
    if (!data.empty())
    {
        out.write(reinterpret_cast<const char *>(data.data()), static_cast<streamsize>(data.size()));
    }
    return static_cast<bool>(out);
}

