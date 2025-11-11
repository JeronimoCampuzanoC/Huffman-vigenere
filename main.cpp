#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdio> // std::rename
#include "Huffman.h"

namespace fs = std::filesystem;
using namespace std;

// ============================================
// FUNCIONES PARA COMPRESIÓN
// ============================================

bool isCompressibleFile(const fs::path &p)
{
    string e = p.extension().string();
    for (auto &c : e)
        c = tolower(c);
    return e == ".pdf" || e == ".txt";
}

void processFile(const fs::path &file)
{
    cout << "\n[+] Procesando: " << file << endl;

    // 1. Leer archivo como binario
    vector<char> data = Huffman::readUncompressedFile(file.string());
    if (data.empty())
    {
        cout << "   (No se pudo leer)\n";
        return;
    }

    long originalSize = data.size();

    // 2. Comprimir → esto genera freqTable.bin
    vector<char> compressed = Huffman::HuffmanCompression(data);

    // 3. Guardar el comprimido
    fs::path outHuf = file.string() + ".huf";
    Huffman::writeFile(outHuf.string(), compressed);
    cout << "   Comprimido → " << outHuf << endl;

    // 4. Renombrar freqTable.bin para guardarla con nombre único
    fs::path freqSrc = "freqTable.bin";
    fs::path freqDest = file.string() + ".freq";

    long compressedSize = 0;
    long freqSize = 0;

    if (fs::exists(freqSrc))
    {
        freqSize = fs::file_size(freqSrc);
        fs::rename(freqSrc, freqDest);
        // Ocultar archivo .freq (sin mostrar en salida)
    }
    else
    {
        cout << "   ADVERTENCIA: no se encontró freqTable.bin\n";
    }

    if (fs::exists(outHuf))
    {
        compressedSize = fs::file_size(outHuf);
    }

    // Mostrar estadísticas
    long totalCompressed = compressedSize + freqSize;
    double ratio = (originalSize > 0) ? (100.0 * compressedSize / originalSize) : 0;
    double ratioTotal = (originalSize > 0) ? (100.0 * totalCompressed / originalSize) : 0;

    cout << "      Original:    " << originalSize << " bytes (" << (originalSize / 1024.0) << " KB)\n";
    cout << "      Comprimido:  " << compressedSize << " bytes (" << (compressedSize / 1024.0) << " KB) - " << ratio << "%\n";
    if (freqSize > 0)
        cout << "      +Tabla freq: " << freqSize << " bytes (" << (freqSize / 1024.0) << " KB)\n";
    cout << "      Total:       " << totalCompressed << " bytes (" << (totalCompressed / 1024.0) << " KB) - " << ratioTotal << "%\n";
}

void compressMode(const fs::path &input)
{
    cout << "\n=== MODO COMPRESIÓN ===\n";

    if (!fs::exists(input))
    {
        cout << "Ruta no existe.\n";
        return;
    }

    if (fs::is_regular_file(input))
    {
        if (isCompressibleFile(input))
            processFile(input);
    }
    else if (fs::is_directory(input))
    {
        for (auto &entry : fs::recursive_directory_iterator(input))
        {
            if (entry.is_regular_file() && isCompressibleFile(entry.path()))
                processFile(entry.path());
        }
    }

    cout << "\nCompresión finalizada.\n";
}

// ============================================
// FUNCIONES PARA DESCOMPRESIÓN
// ============================================

void decompressFile(const fs::path &hufFile)
{
    cout << "\n[+] Descomprimiendo: " << hufFile << endl;

    // Intentar encontrar archivo .freq
    // Primero: hufFile + ".freq" (ejemplo.pdf.huf.freq)
    // Segundo: archivo_original + ".freq" (ejemplo.pdf.freq)
    fs::path freqFile = hufFile.string() + ".freq";

    if (!fs::exists(freqFile))
    {
        // Intentar sin el .huf (por si fue comprimido como ejemplo.pdf y después pasó a .huf)
        string hufStr = hufFile.string();
        if (hufStr.size() > 4 && hufStr.substr(hufStr.size() - 4) == ".huf")
        {
            string baseName = hufStr.substr(0, hufStr.size() - 4); // Remove .huf
            freqFile = baseName + ".freq";
        }
    }

    // Validar que exista el archivo .freq
    if (!fs::exists(freqFile))
    {
        cout << "   ERROR: No se encontró tabla de frecuencias.\n";
        cout << "   Buscadas: " << hufFile.string() + ".freq" << endl;
        return;
    }

    // Restaurar freqTable.bin (lo requiere la función)
    fs::copy(freqFile, "freqTable.bin", fs::copy_options::overwrite_existing);

    // Leer comprimido
    auto compressed = Huffman::readUncompressedFile(hufFile.string());
    if (compressed.empty())
    {
        cout << "   ERROR: No se pudo leer archivo comprimido\n";
        return;
    }

    // Descomprimir
    auto restored = Huffman::HuffmanDecompression(compressed);

    // Guardar resultado
    fs::path output = hufFile.string() + ".restored";
    Huffman::writeFile(output.string(), restored);
    cout << "   Descomprimido → " << output << endl;

    // Mostrar estadísticas
    long compressedTotal = 0;
    if (fs::exists(hufFile))
        compressedTotal += fs::file_size(hufFile);
    if (fs::exists(freqFile))
        compressedTotal += fs::file_size(freqFile);

    long restoredSize = restored.size();
    double ratio = (compressedTotal > 0) ? (100.0 * restoredSize / compressedTotal) : 0;

    cout << "      Restaurado:  " << restoredSize << " bytes (" << (restoredSize / 1024.0) << " KB)\n";
    cout << "      Ratio:       " << ratio << "% (expansión)\n";

    // Limpiar freqTable.bin temporal
    if (fs::exists("freqTable.bin"))
    {
        fs::remove("freqTable.bin");
    }
}

void decompressMode(const fs::path &input)
{
    cout << "\n=== MODO DESCOMPRESIÓN ===\n";

    if (!fs::exists(input))
    {
        cout << "Ruta no existe.\n";
        return;
    }

    if (fs::is_regular_file(input))
    {
        if (input.extension() == ".huf")
        {
            decompressFile(input);
        }
        else
        {
            cout << "El archivo debe tener extensión .huf\n";
        }
    }
    else if (fs::is_directory(input))
    {
        for (auto &entry : fs::recursive_directory_iterator(input))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".huf")
                decompressFile(entry.path());
        }
    }

    cout << "\nDescompresión finalizada.\n";
}

// ============================================
// MAIN
// ============================================

void showUsage(const char *program)
{
    cout << "Uso: " << program << " <modo> <ruta>\n\n";
    cout << "Modos:\n";
    cout << "  -c, --compress    Comprimir archivos (PDF, TXT)\n";
    cout << "  -d, --decompress  Descomprimir archivos .huf\n\n";
    cout << "Ejemplos:\n";
    cout << "  " << program << " -c archivo.pdf\n";
    cout << "  " << program << " -c archivo.txt\n";
    cout << "  " << program << " -c carpeta/\n";
    cout << "  " << program << " -d archivo.pdf.huf\n";
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        showUsage(argv[0]);
        return 0;
    }

    string mode = argv[1];
    fs::path input = argv[2];

    if (mode == "-c" || mode == "--compress")
    {
        compressMode(input);
    }
    else if (mode == "-d" || mode == "--decompress")
    {
        decompressMode(input);
    }
    else
    {
        cout << "Modo no reconocido: " << mode << endl;
        showUsage(argv[0]);
        return 1;
    }

    return 0;
}
