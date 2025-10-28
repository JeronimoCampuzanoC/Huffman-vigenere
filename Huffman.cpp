#include "Huffman.h"
#include "NodeLetter.h"
#include <map>
#include <algorithm>
#include <utility>
using namespace std;
#include <iostream>

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
    } while (nodes.size() > 1);
    // The root of the Huffman tree is the last remaining node
    NodeLetter *root = nodes[0];
    //generate a map that maps characters to their binary codes
    map<char, string> huffmanCodes;
    //generate codes
    generateCodes(root, "", huffmanCodes);
    
    //compress the input
    string compressedString;
    for (char c : input)
    {
        compressedString += huffmanCodes[c];
    }
    
    //print codes 
    for (const auto &code : huffmanCodes)
    {
        cout << code.first << " -> " << code.second << endl;
    }
    
    // Convert string to vector<char> for return
    vector<char> compressedInput(compressedString.begin(), compressedString.end());
    return compressedInput;
}

void Huffman::generateCodes(NodeLetter *node, string code, map<char, string> &huffmanCodes)
{
    if (node->izq == nullptr && node->der == nullptr)
    {
        huffmanCodes[node->letra] = code;
    }
    else
    {
        generateCodes(node->izq, code + "0", huffmanCodes);
        generateCodes(node->der, code + "1", huffmanCodes);
    }
}