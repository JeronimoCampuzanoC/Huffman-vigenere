#include "Huffman.h"
#include "NodeLetter.h"
#include <map>
#include <algorithm>
#include <utility>
#include <iostream>
#include <cstdint>
// System calls POSIX para gestión de archivos
#include <fcntl.h>    // open()
#include <unistd.h>   // read(), write(), close()
#include <sys/stat.h> // fstat()
using namespace std;

std::vector<char> Huffman::HuffmanCompression(const std::vector<char> &input, const std::string &freqTablePath)
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
    // build the Huffman tree by merging the two nodes with the lowest frequency
    while (nodes.size() > 1);
    // root of the built Huffman tree
    NodeLetter *root = nodes.empty() ? nullptr : nodes[0];
    map<char, string> huffmanCodes;
    generateCodes(root, "", huffmanCodes);

    // compress the input
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
    // calculate padding with bits that are left
    uint8_t padding = (bitCount == 0) ? 0 : (uint8_t)(8 - bitCount);

    // if there are left bits, run to the left and write the last byte
    if (bitCount > 0)
    {
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

    // Save frecuency tree for decompression usando system calls POSIX
    int fd = open(freqTablePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        // Error al abrir archivo
        deleteTree(root);
        return compressedInput;
    }

    uint16_t symbolCount = static_cast<uint16_t>(frequency.size());
    write(fd, &symbolCount, sizeof(symbolCount));

    for (auto &p : frequency)
    {
        char sym = p.first;
        int32_t freq = p.second;
        write(fd, &sym, sizeof(sym));
        write(fd, &freq, sizeof(freq));
    }

    // add padding and original size for decompression
    uint8_t pad = padding;
    uint32_t originalSize = static_cast<uint32_t>(input.size());
    write(fd, &pad, sizeof(pad));
    write(fd, &originalSize, sizeof(originalSize));

    close(fd);

    // delete memory from the tree recursively
    deleteTree(root);

    return compressedInput;
}

void Huffman::generateCodes(NodeLetter *node, string code, map<char, string> &huffmanCodes)
{
    if (!node)
        return;

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
    // Usar system call open() en lugar de ifstream
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        return false;
    }

    uint16_t symbolCount = 0;
    if (read(fd, &symbolCount, sizeof(symbolCount)) != sizeof(symbolCount))
    {
        close(fd);
        return false;
    }

    freq.clear();
    freq.reserve(symbolCount);
    for (uint16_t i = 0; i < symbolCount; ++i)
    {
        char sym;
        int32_t fr;
        if (read(fd, &sym, sizeof(sym)) != sizeof(sym) ||
            read(fd, &fr, sizeof(fr)) != sizeof(fr))
        {
            close(fd);
            return false;
        }
        freq.push_back({sym, static_cast<int>(fr)});
    }

    if (read(fd, &pad, sizeof(pad)) != sizeof(pad) ||
        read(fd, &originalSize, sizeof(originalSize)) != sizeof(originalSize))
    {
        close(fd);
        return false;
    }

    close(fd);

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
vector<char> Huffman::HuffmanDecompression(const vector<char> &compressed, const string &freqTablePath)
{
    vector<pair<char, int>> freq;
    uint8_t pad = 0;
    uint32_t originalSize = 0;
    NodeLetter *root = nullptr;

    if (!loadFreqAndBuildTree(freqTablePath, freq, pad, originalSize, root))
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

// function to read the uncompressed file usando system calls POSIX
vector<char> Huffman::readUncompressedFile(const string &path)
{
    // Usar open() en lugar de ifstream
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        return {};
    }

    // Obtener tamaño del archivo con fstat()
    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        return {};
    }

    size_t size = static_cast<size_t>(st.st_size);
    vector<char> data(size);

    // Leer el archivo con read()
    if (size > 0)
    {
        ssize_t bytesRead = read(fd, data.data(), size);
        if (bytesRead < 0)
        {
            close(fd);
            return {};
        }
    }

    close(fd);
    return data;
}

bool Huffman::writeFile(const string &path, const vector<char> &data)
{
    // Usar open() con flags de escritura en lugar de ofstream
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        return false;
    }

    // Escribir datos con write()
    if (!data.empty())
    {
        ssize_t bytesWritten = write(fd, data.data(), data.size());
        if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) != data.size())
        {
            close(fd);
            return false;
        }
    }

    close(fd);
    return true;
}
