#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdio>     // std::rename
#include <unistd.h>   // para fork, exec
#include <sys/wait.h> // para wait
#include <pthread.h>  // para threads
#include "Huffman.h"
#include "Vigenere.h"

namespace fs = std::filesystem;
using namespace std;

// Forward declarations
bool isCompressibleFile(const fs::path &p);

// Funciones auxiliares para operaciones individuales en threads
bool threadCompress(const fs::path &inputFile, fs::path &outputFile, int threadId)
{
    vector<char> fileData = Huffman::readUncompressedFile(inputFile.string());
    if (fileData.empty())
        return false;

    // Generar nombre único para la tabla de frecuencias
    fs::path freqDest = inputFile.string() + ".freq";
    
    // Comprimir usando tabla de frecuencias única
    vector<char> compressed = Huffman::HuffmanCompression(fileData, freqDest.string());

    // Guardar comprimido
    outputFile = inputFile.string() + ".huf";
    return Huffman::writeFile(outputFile.string(), compressed);
}

bool threadDecompress(const fs::path &inputFile, fs::path &outputFile, int threadId)
{
    // Buscar archivo .freq
    fs::path freqFile = inputFile.string() + ".freq";
    if (!fs::exists(freqFile))
    {
        string hufStr = inputFile.string();
        if (hufStr.size() > 4 && hufStr.substr(hufStr.size() - 4) == ".huf")
        {
            string baseName = hufStr.substr(0, hufStr.size() - 4);
            freqFile = baseName + ".freq";
        }
    }

    if (!fs::exists(freqFile))
    {
        cout << "[Thread " << threadId << "] ERROR: No se encontró " << freqFile << endl;
        return false;
    }

    // Leer y descomprimir
    auto compressed = Huffman::readUncompressedFile(inputFile.string());
    if (compressed.empty())
        return false;

    auto restored = Huffman::HuffmanDecompression(compressed, freqFile.string());
    
    // Si el archivo original era texto.txt.enc.huf (resultado de -ec), 
    // al descomprimir debería dar texto.txt.enc para luego desencriptar
    string inputStr = inputFile.string();
    if (inputStr.size() > 4 && inputStr.substr(inputStr.size() - 4) == ".huf")
    {
        // Quitar .huf para obtener el nombre base (que puede terminar en .enc)
        outputFile = inputStr.substr(0, inputStr.size() - 4);
    }
    else
    {
        outputFile = inputFile.string() + ".restored";
    }
    
    return Huffman::writeFile(outputFile.string(), restored);
}

bool threadEncrypt(const fs::path &inputFile, fs::path &outputFile, const string &key, int threadId)
{
    vector<char> fileData = Huffman::readUncompressedFile(inputFile.string());
    if (fileData.empty())
        return false;

    vector<char> encrypted = Vigenere::VigenereEncryption(fileData, key);
    outputFile = inputFile.string() + ".enc";
    return Huffman::writeFile(outputFile.string(), encrypted);
}

bool threadDecrypt(const fs::path &inputFile, fs::path &outputFile, const string &key, int threadId)
{
    vector<char> encrypted = Huffman::readUncompressedFile(inputFile.string());
    if (encrypted.empty())
        return false;

    vector<char> decrypted = Vigenere::VigenereDecryption(encrypted, key);
    
    // Si el archivo termina en .enc, quitarlo para obtener el nombre original
    string inputStr = inputFile.string();
    if (inputStr.size() > 4 && inputStr.substr(inputStr.size() - 4) == ".enc")
    {
        // Quitar .enc para obtener el nombre original (ej: texto.txt.enc -> texto.txt)
        outputFile = inputStr.substr(0, inputStr.size() - 4);
    }
    else
    {
        outputFile = inputFile.string() + ".dec";
    }
    
    return Huffman::writeFile(outputFile.string(), decrypted);
}

// Estructura para pasar datos a los threads
struct ThreadData
{
    fs::path file;
    string mode;
    string key;
    int threadId;
    bool success;
    vector<string> operations; // Para operaciones combinadas: {"c", "e"} para -ce
};

// Función que ejecutará cada thread
void *processFileThread(void *arg)
{
    ThreadData *data = static_cast<ThreadData *>(arg);

    cout << "\n[Thread " << data->threadId << "] Procesando: " << data->file << endl;

    try
    {
        // Si hay operaciones combinadas, ejecutarlas en secuencia
        if (!data->operations.empty())
        {
            fs::path currentFile = data->file;
            bool allSuccess = true;

            for (size_t i = 0; i < data->operations.size(); i++)
            {
                const string &op = data->operations[i];
                fs::path outputFile;
                bool success = false;

                if (op == "c")
                {
                    if (!isCompressibleFile(currentFile))
                    {
                        cout << "[Thread " << data->threadId << "] Archivo no comprimible: " << currentFile << endl;
                        allSuccess = false;
                        break;
                    }
                    success = threadCompress(currentFile, outputFile, data->threadId);
                    if (success)
                        cout << "[Thread " << data->threadId << "] Comprimido → " << outputFile << endl;
                }
                else if (op == "d")
                {
                    if (currentFile.extension() != ".huf")
                    {
                        cout << "[Thread " << data->threadId << "] ERROR: Se esperaba archivo .huf" << endl;
                        allSuccess = false;
                        break;
                    }
                    success = threadDecompress(currentFile, outputFile, data->threadId);
                    if (success)
                        cout << "[Thread " << data->threadId << "] Descomprimido → " << outputFile << endl;
                }
                else if (op == "e")
                {
                    success = threadEncrypt(currentFile, outputFile, data->key, data->threadId);
                    if (success)
                        cout << "[Thread " << data->threadId << "] Encriptado → " << outputFile << endl;
                }
                else if (op == "z")
                {
                    if (currentFile.extension() != ".enc")
                    {
                        cout << "[Thread " << data->threadId << "] ERROR: Se esperaba archivo .enc" << endl;
                        allSuccess = false;
                        break;
                    }
                    success = threadDecrypt(currentFile, outputFile, data->key, data->threadId);
                    if (success)
                        cout << "[Thread " << data->threadId << "] Desencriptado → " << outputFile << endl;
                }

                if (!success)
                {
                    cout << "[Thread " << data->threadId << "] ERROR en operación: " << op << endl;
                    allSuccess = false;
                    break;
                }

                // El archivo de salida se convierte en entrada para la siguiente operación
                currentFile = outputFile;
            }

            data->success = allSuccess;
        }
        else
        {
            // Operación simple (modo legacy)
            if (data->mode == "-c" || data->mode == "--compress")
            {
                if (isCompressibleFile(data->file))
                {
                    fs::path outputFile;
                    if (threadCompress(data->file, outputFile, data->threadId))
                    {
                        cout << "[Thread " << data->threadId << "] Comprimido → " << outputFile << endl;
                        data->success = true;
                    }
                }
            }
            else if (data->mode == "-d" || data->mode == "--decompress")
            {
                if (data->file.extension() == ".huf")
                {
                    fs::path outputFile;
                    if (threadDecompress(data->file, outputFile, data->threadId))
                    {
                        cout << "[Thread " << data->threadId << "] Descomprimido → " << outputFile << endl;
                        data->success = true;
                    }
                }
            }
            else if (data->mode == "-e" || data->mode == "--encrypt")
            {
                fs::path outputFile;
                if (threadEncrypt(data->file, outputFile, data->key, data->threadId))
                {
                    cout << "[Thread " << data->threadId << "] Encriptado → " << outputFile << endl;
                    data->success = true;
                }
            }
            else if (data->mode == "-z" || data->mode == "--decrypt")
            {
                if (data->file.extension() == ".enc")
                {
                    fs::path outputFile;
                    if (threadDecrypt(data->file, outputFile, data->key, data->threadId))
                    {
                        cout << "[Thread " << data->threadId << "] Desencriptado → " << outputFile << endl;
                        data->success = true;
                    }
                }
            }
        }
    }
    catch (const exception &e)
    {
        cout << "[Thread " << data->threadId << "] ERROR: " << e.what() << endl;
        data->success = false;
    }

    pthread_exit(nullptr);
}

// ============================================
// FUNCIONES PARA COMPRESIÓN
// ============================================

// Función para parsear el modo y extraer operaciones
vector<string> parseMode(const string &mode, bool &needsKey)
{
    vector<string> operations;
    needsKey = false;

    // Modos simples
    if (mode == "-c" || mode == "--compress")
    {
        operations.push_back("c");
    }
    else if (mode == "-d" || mode == "--decompress")
    {
        operations.push_back("d");
    }
    else if (mode == "-e" || mode == "--encrypt")
    {
        operations.push_back("e");
        needsKey = true;
    }
    else if (mode == "-z" || mode == "--decrypt")
    {
        operations.push_back("z");
        needsKey = true;
    }
    // Modos combinados
    else if (mode.length() >= 2 && mode[0] == '-')
    {
        // Parsear cada caracter después del '-'
        for (size_t i = 1; i < mode.length(); i++)
        {
            char op = mode[i];
            if (op == 'c' || op == 'd' || op == 'e' || op == 'z')
            {
                operations.push_back(string(1, op));
                if (op == 'e' || op == 'z')
                {
                    needsKey = true;
                }
            }
            else
            {
                // Caracter no válido
                operations.clear();
                return operations;
            }
        }
    }

    return operations;
}

// Validar que la combinación de operaciones sea válida
bool validateOperations(const vector<string> &operations, string &errorMsg)
{
    if (operations.empty())
        return false;

    // Verificar combinaciones inválidas
    for (size_t i = 0; i < operations.size() - 1; i++)
    {
        string current = operations[i];
        string next = operations[i + 1];

        // No se puede encriptar/desencriptar después de comprimir (datos binarios)
        if (current == "c" && (next == "e" || next == "z"))
        {
            errorMsg = "ERROR: No se puede encriptar/desencriptar después de comprimir.\n"
                      "       El cifrado Vigenere solo funciona con texto, no con datos binarios.\n"
                      "       Use -ec (encriptar y luego comprimir) en lugar de -ce";
            return false;
        }

        // No se puede comprimir/encriptar después de descomprimir (recuperación final)
        if (current == "d" && (next == "c" || next == "e"))
        {
            errorMsg = "ERROR: No se puede comprimir/encriptar después de descomprimir.\n"
                      "       La descompresión debe ser seguida de desencriptación (-dz) o ser final.";
            return false;
        }

        // Descomprimir y luego desencriptar es válido (orden inverso de -ec)
        if (current == "d" && next == "z")
        {
            // Válido: -dz es el inverso de -ec
        }

        // No se puede encriptar después de encriptar (duplicación innecesaria)
        if (current == "e" && next == "e")
        {
            errorMsg = "ERROR: No se puede encriptar dos veces consecutivas.";
            return false;
        }

        // No se puede comprimir después de comprimir
        if (current == "c" && next == "c")
        {
            errorMsg = "ERROR: No se puede comprimir dos veces consecutivas.";
            return false;
        }
    }

    return true;
}

bool isCompressibleFile(const fs::path &p)
{
    string e = p.extension().string();
    for (auto &c : e)
        c = tolower(c);
    return e == ".pdf" || e == ".txt" || e == ".enc";
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

    // 2. Generar nombre único para la tabla de frecuencias
    fs::path freqDest = file.string() + ".freq";

    // 3. Comprimir → esto genera la tabla de frecuencias con nombre específico
    vector<char> compressed = Huffman::HuffmanCompression(data, freqDest.string());

    // 4. Guardar el comprimido
    fs::path outHuf = file.string() + ".huf";
    Huffman::writeFile(outHuf.string(), compressed);
    cout << "   Comprimido → " << outHuf << endl;

    long compressedSize = 0;
    long freqSize = 0;

    if (fs::exists(freqDest))
    {
        freqSize = fs::file_size(freqDest);
        // Tabla de frecuencias ya creada con el nombre correcto
    }
    else
    {
        cout << "   ADVERTENCIA: no se encontró la tabla de frecuencias\n";
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

    // Leer comprimido
    auto compressed = Huffman::readUncompressedFile(hufFile.string());
    if (compressed.empty())
    {
        cout << "   ERROR: No se pudo leer archivo comprimido\n";
        return;
    }

    // Descomprimir usando la tabla de frecuencias directamente
    auto restored = Huffman::HuffmanDecompression(compressed, freqFile.string());

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
    cout << "Modos combinados:\n";
    cout << "  -ec               Encriptar y luego comprimir (requiere clave) ✓\n";
    cout << "  -dz               Descomprimir y luego desencriptar (requiere clave) ✓\n";
    cout << "\n";
    cout << "  Nota: No use -ce (comprimir y encriptar) ya que el cifrado Vigenere\n";
    cout << "        solo funciona con texto. Use -ec en su lugar.\n";
    cout << "        Para revertir -ec, use -dz (no -zd).\n\n";
    cout << "Ejemplos:\n";
    cout << "  " << program << " -c archivo.pdf\n";
    cout << "  " << program << " -c archivo.txt\n";
    cout << "  " << program << " -c carpeta/\n";
    cout << "  " << program << " -d archivo.pdf.huf\n";
    cout << "  " << program << " -e archivo.txt miClave123\n";
    cout << "  " << program << " -z archivo.txt.enc miClave123\n";
    cout << "  " << program << " -ec archivo.txt miClave123  (encripta y comprime)\n";
    cout << "  " << program << " -dz archivo.txt.huf miClave123  (descomprime y desencripta)\n";
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

    // Parsear el modo para obtener las operaciones
    bool needsKey = false;
    vector<string> operations = parseMode(mode, needsKey);

    if (operations.empty())
    {
        cout << "ERROR: Modo no reconocido: " << mode << endl;
        showUsage(argv[0]);
        return 1;
    }

    // Validar que la combinación de operaciones sea válida
    string validationError;
    if (!validateOperations(operations, validationError))
    {
        cout << validationError << endl;
        cout << "\nCombinaciones válidas:\n";
        cout << "  -ec   : Encriptar y luego comprimir\n";
        cout << "  -dz   : Descomprimir y luego desencriptar (inverso de -ec)\n";
        cout << "  -e -c : Encriptar y comprimir (dos pasos separados)\n";
        showUsage(argv[0]);
        return 1;
    }

    if (needsKey && key.empty())
    {
        cout << "ERROR: Este modo requiere una clave de encriptación/desencriptación\n";
        showUsage(argv[0]);
        return 1;
    }

    // Check if the path is a folder or a file
    if (fs::exists(input))
    {
        if (fs::is_regular_file(input))
        {
            cout << "Procesando archivo: " << input << endl;
            
            // Crear thread data
            ThreadData data;
            data.file = input;
            data.mode = mode;
            data.key = key;
            data.threadId = 1;
            data.success = false;
            data.operations = operations;

            // Procesar en el hilo principal (sin crear thread adicional para un solo archivo)
            processFileThread(&data);

            if (!data.success)
            {
                cout << "ERROR: Falló el procesamiento del archivo\n";
                return 1;
            }
        }
        else if (fs::is_directory(input))
        {
            cout << "Procesando directorio: " << input << endl;

            // Recolectar todos los archivos en el directorio
            vector<fs::path> files;
            for (auto &entry : fs::recursive_directory_iterator(input))
            {
                if (entry.is_regular_file())
                {
                    // Filtrar archivos según la primera operación
                    bool shouldProcess = false;
                    string firstOp = operations[0];

                    if (firstOp == "c")
                    {
                        shouldProcess = isCompressibleFile(entry.path());
                    }
                    else if (firstOp == "d")
                    {
                        shouldProcess = (entry.path().extension() == ".huf");
                    }
                    else if (firstOp == "e")
                    {
                        shouldProcess = true; // Encriptar todos los archivos
                    }
                    else if (firstOp == "z")
                    {
                        shouldProcess = (entry.path().extension() == ".enc");
                    }

                    if (shouldProcess)
                    {
                        files.push_back(entry.path());
                    }
                }
            }

            cout << "Total de archivos encontrados: " << files.size() << endl;

            // Crear threads para procesar archivos concurrentemente
            vector<pthread_t> threads;
            vector<ThreadData> threadDataArray;

            threads.resize(files.size());
            threadDataArray.resize(files.size());

            // Crear un thread para cada archivo
            for (size_t i = 0; i < files.size(); i++)
            {
                threadDataArray[i].file = files[i];
                threadDataArray[i].mode = mode;
                threadDataArray[i].key = key;
                threadDataArray[i].threadId = i + 1;
                threadDataArray[i].success = false;
                threadDataArray[i].operations = operations;

                int result = pthread_create(&threads[i], nullptr, processFileThread, &threadDataArray[i]);

                if (result != 0)
                {
                    cout << "ERROR: No se pudo crear thread para " << files[i] << endl;
                }
            }

            // Esperar a que todos los threads terminen
            cout << "\nEsperando a que terminen " << threads.size() << " threads..." << endl;

            for (size_t i = 0; i < threads.size(); i++)
            {
                pthread_join(threads[i], nullptr);
            }

            // Contar resultados
            int successCount = 0;
            int failCount = 0;

            for (const auto &data : threadDataArray)
            {
                if (data.success)
                    successCount++;
                else
                    failCount++;
            }

            cout << "\nResultado: " << successCount << " exitosos, " << failCount << " fallidos" << endl;
            cout << "Procesamiento de directorio finalizado.\n";
        }
    }
    else
    {
        cout << "ERROR: La ruta no existe: " << input << endl;
        return 1;
    }

    return 0;
}