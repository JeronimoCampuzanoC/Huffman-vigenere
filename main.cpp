#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdio>     // std::rename
#include <unistd.h>   // para fork, exec
#include <sys/wait.h> // para wait
#include "Huffman.h"
#include "Vigenere.h"

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
// FUNCIONES PARA ENCRIPTACIÓN (CON SYSTEM CALLS)
// ============================================

void encryptFile(const fs::path &file, const string &key)
{
    cout << "\n[+] Encriptando: " << file << endl;

    // Leer archivo
    vector<char> data = Huffman::readUncompressedFile(file.string());
    if (data.empty())
    {
        cout << "   ERROR: No se pudo leer el archivo\n";
        return;
    }

    // Crear proceso hijo usando fork
    pid_t pid = fork();

    if (pid < 0)
    {
        cout << "   ERROR: No se pudo crear proceso hijo\n";
        return;
    }
    else if (pid == 0)
    {
        // Proceso HIJO: realizar encriptación
        vector<char> encrypted = Vigenere::VigenereEncryption(data, key);

        // Guardar archivo encriptado
        fs::path outEnc = file.string() + ".enc";
        Huffman::writeFile(outEnc.string(), encrypted);

        // Terminar proceso hijo
        exit(0);
    }
    else
    {
        // Proceso PADRE: esperar a que termine el hijo
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            fs::path outEnc = file.string() + ".enc";
            cout << "   Encriptado → " << outEnc << endl;

            // Mostrar estadísticas
            long originalSize = data.size();
            long encryptedSize = 0;
            if (fs::exists(outEnc))
            {
                encryptedSize = fs::file_size(outEnc);
            }

            cout << "      Original:    " << originalSize << " bytes (" << (originalSize / 1024.0) << " KB)\n";
            cout << "      Encriptado:  " << encryptedSize << " bytes (" << (encryptedSize / 1024.0) << " KB)\n";
        }
        else
        {
            cout << "   ERROR: Fallo en la encriptación\n";
        }
    }
}

void encryptMode(const fs::path &input, const string &key)
{
    cout << "\n=== MODO ENCRIPTACIÓN ===\n";

    if (key.empty())
    {
        cout << "ERROR: Debe proporcionar una clave de encriptación\n";
        return;
    }

    if (!fs::exists(input))
    {
        cout << "Ruta no existe.\n";
        return;
    }

    if (fs::is_regular_file(input))
    {
        encryptFile(input, key);
    }
    else if (fs::is_directory(input))
    {
        for (auto &entry : fs::recursive_directory_iterator(input))
        {
            if (entry.is_regular_file())
                encryptFile(entry.path(), key);
        }
    }

    cout << "\nEncriptación finalizada.\n";
}

// ============================================
// FUNCIONES PARA DESENCRIPTACIÓN (CON SYSTEM CALLS)
// ============================================

void decryptFile(const fs::path &file, const string &key)
{
    cout << "\n[+] Desencriptando: " << file << endl;

    // Leer archivo encriptado
    vector<char> encrypted = Huffman::readUncompressedFile(file.string());
    if (encrypted.empty())
    {
        cout << "   ERROR: No se pudo leer el archivo\n";
        return;
    }

    // Crear proceso hijo usando fork
    pid_t pid = fork();

    if (pid < 0)
    {
        cout << "   ERROR: No se pudo crear proceso hijo\n";
        return;
    }
    else if (pid == 0)
    {
        // Proceso HIJO: realizar desencriptación
        vector<char> decrypted = Vigenere::VigenereDecryption(encrypted, key);

        // Guardar archivo desencriptado
        fs::path outDec = file.string() + ".dec";
        Huffman::writeFile(outDec.string(), decrypted);

        // Terminar proceso hijo
        exit(0);
    }
    else
    {
        // Proceso PADRE: esperar a que termine el hijo
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            fs::path outDec = file.string() + ".dec";
            cout << "   Desencriptado → " << outDec << endl;

            // Mostrar estadísticas
            long encryptedSize = encrypted.size();
            long decryptedSize = 0;
            if (fs::exists(outDec))
            {
                decryptedSize = fs::file_size(outDec);
            }

            cout << "      Encriptado:    " << encryptedSize << " bytes (" << (encryptedSize / 1024.0) << " KB)\n";
            cout << "      Desencriptado: " << decryptedSize << " bytes (" << (decryptedSize / 1024.0) << " KB)\n";
        }
        else
        {
            cout << "   ERROR: Fallo en la desencriptación\n";
        }
    }
}

void decryptMode(const fs::path &input, const string &key)
{
    cout << "\n=== MODO DESENCRIPTACIÓN ===\n";

    if (key.empty())
    {
        cout << "ERROR: Debe proporcionar una clave de desencriptación\n";
        return;
    }

    if (!fs::exists(input))
    {
        cout << "Ruta no existe.\n";
        return;
    }

    if (fs::is_regular_file(input))
    {
        if (input.extension() == ".enc")
        {
            decryptFile(input, key);
        }
        else
        {
            cout << "El archivo debe tener extensión .enc\n";
        }
    }
    else if (fs::is_directory(input))
    {
        for (auto &entry : fs::recursive_directory_iterator(input))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".enc")
                decryptFile(entry.path(), key);
        }
    }

    cout << "\nDesencriptación finalizada.\n";
}

// ============================================
// MAIN
// ============================================

void showUsage(const char *program)
{
    cout << "Uso: " << program << " <modo> <ruta> [clave]\n\n";
    cout << "Modos:\n";
    cout << "  -c, --compress    Comprimir archivos (PDF, TXT)\n";
    cout << "  -d, --decompress  Descomprimir archivos .huf\n";
    cout << "  -e, --encrypt     Encriptar archivos (requiere clave)\n";
    cout << "  -z, --decrypt     Desencriptar archivos .enc (requiere clave)\n\n";
    cout << "Ejemplos:\n";
    cout << "  " << program << " -c archivo.pdf\n";
    cout << "  " << program << " -c archivo.txt\n";
    cout << "  " << program << " -c carpeta/\n";
    cout << "  " << program << " -d archivo.pdf.huf\n";
    cout << "  " << program << " -e archivo.txt miClave123\n";
    cout << "  " << program << " -z archivo.txt.enc miClave123\n";
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
    string key = (argc >= 4) ? argv[3] : "";

    if (mode == "-c" || mode == "--compress")
    {
        compressMode(input);
    }
    else if (mode == "-d" || mode == "--decompress")
    {
        decompressMode(input);
    }
    else if (mode == "-e" || mode == "--encrypt")
    {
        if (key.empty())
        {
            cout << "ERROR: Debe proporcionar una clave para encriptar\n";
            showUsage(argv[0]);
            return 1;
        }
        encryptMode(input, key);
    }
    else if (mode == "-z" || mode == "--decrypt")
    {
        if (key.empty())
        {
            cout << "ERROR: Debe proporcionar una clave para desencriptar\n";
            showUsage(argv[0]);
            return 1;
        }
        decryptMode(input, key);
    }
    else
    {
        cout << "Modo no reconocido: " << mode << endl;
        showUsage(argv[0]);
        return 1;
    }

    return 0;
}