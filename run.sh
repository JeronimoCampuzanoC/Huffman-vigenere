#!/bin/bash
# Huffman-Vigenere Compilation & Run Script

set -e  # Exit on error

echo "=========================================="
echo "Huffman-Vigenere Compilation & Run Script"
echo "=========================================="
echo ""

# Check if argument is provided
if [ $# -eq 0 ]; then
    echo "Usage: ./run.sh [cli|demo]"
    echo ""
    echo "Options:"
    echo "  cli   - Compile and run CLI tool with example operations"
    echo "  demo  - Compile and run the demo program (main.cpp)"
    exit 1
fi

MODE=$1

if [ "$MODE" == "cli" ]; then
    echo "Building CLI tool..."
    g++ -std=c++17 -O2 -pthread cli_layout.cpp Huffman.cpp -o clitool
    
    if [ $? -eq 0 ]; then
        echo "✓ Build successful!"
        echo ""
        echo "Creating test input file..."
        echo "This is a test file for Huffman compression and Vigenere encryption." > test_input.txt
        echo "We will compress and encrypt this content." >> test_input.txt
        echo "Then decompress and decrypt to verify the process works correctly." >> test_input.txt
        
        echo ""
        echo "Running example operations:"
        echo ""
        
        echo "1. Compress and encrypt with XOR (Vigenere-like):"
        echo "   Command: ./clitool -ce --comp-alg huffman --enc-alg xor -i test_input.txt -o ./test_output -k secret123"
        ./clitool -ce --comp-alg huffman --enc-alg xor -i test_input.txt -o ./test_output -k secret123
        
        # The tool adds .cmp.enc extensions automatically
        COMPRESSED_FILE="./test_output.cmp.enc"
        
        echo ""
        echo "2. Decompress and decrypt (reverse order: -ud for uncompress then decrypt):"
        echo "   Command: ./clitool -ud --comp-alg huffman --enc-alg xor -i $COMPRESSED_FILE -o ./test_restored.txt -k secret123"
        ./clitool -ud --comp-alg huffman --enc-alg xor -i "$COMPRESSED_FILE" -o ./test_restored.txt -k secret123
        
        echo ""
        echo "3. Verify content:"
        if [ -f "test_restored.txt" ]; then
            if diff -q test_input.txt test_restored.txt > /dev/null; then
                echo "   ✓ Files match! Compression and encryption working correctly."
            else
                echo "   ✗ Files differ! Something went wrong."
            fi
            
            echo ""
            echo "File sizes:"
            ls -lh test_input.txt "$COMPRESSED_FILE" test_restored.txt 2>/dev/null | awk '{print "   " $9 ": " $5}'
        else
            echo "   ✗ Restored file not found!"
        fi
    else
        echo "✗ Build failed!"
        exit 1
    fi

elif [ "$MODE" == "demo" ]; then
    echo "Building demo program..."
    g++ -std=c++17 -O2 main.cpp Huffman.cpp -o demo
    
    if [ $? -eq 0 ]; then
        echo "✓ Build successful!"
        echo ""
        echo "Running demo..."
        echo ""
        ./demo
        
        if [ -f "uncompressed.txt" ]; then
            echo ""
            echo "Generated files:"
            ls -lh freqTable.bin uncompressed.txt | awk '{print "   " $9 ": " $5}'
        fi
    else
        echo "✗ Build failed!"
        exit 1
    fi

else
    echo "Invalid option: $MODE"
    echo "Use './run.sh cli' or './run.sh demo'"
    exit 1
fi

echo ""
echo "=========================================="
echo "Done!"
echo "=========================================="
