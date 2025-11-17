#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdio>     // std::rename
#include <unistd.h>   // para fork, exec
#include <sys/wait.h> // para wait
#include <pthread.h>  // para threads
#include <mutex>      // para sincronización
#include "Huffman.h"
#include "Vigenere.h"

namespace fs = std::filesystem;
using namespace std;

// Mutex global para sincronizar acceso a freqTable.bin
pthread_mutex_t freqTableMutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
bool isCompressibleFile(const fs::path &p);

// Estructura para pasar datos a los threads
struct ThreadData
{
    fs::path file;
    string mode;
    string key;
    int threadId;
    bool success;
};

// Función que ejecutará cada thread
void *processFileThread(void *arg)
{
    ThreadData *data = static_cast<ThreadData *>(arg);

    cout << "\n[Thread " << data->threadId << "] Procesando: " << data->file << endl;

    try
    {
        if (data->mode == "-c" || data->mode == "--compress")
        {
            if (isCompressibleFile(data->file))
            {
                // Leer archivo
                vector<char> fileData = Huffman::readUncompressedFile(data->file.string());
                if (!fileData.empty())
                {
                    // Comprimir
                    vector<char> compressed = Huffman::HuffmanCompression(fileData);

                    // Guardar comprimido
                    fs::path outHuf = data->file.string() + ".huf";
                    Huffman::writeFile(outHuf.string(), compressed);

                    // Renombrar freqTable.bin con nombre único para evitar conflictos entre threads
                    fs::path freqSrc = "freqTable.bin";
                    fs::path freqDest = data->file.string() + ".freq";

                    // Esperar un momento para asegurar que el archivo se haya creado
                    if (fs::exists(freqSrc))
                    {
                        // Usar un nombre temporal único basado en el threadId
                        fs::path tempFreq = "freqTable_" + to_string(data->threadId) + ".bin";

                        try
                        {
                            // Copiar a temporal primero para evitar conflictos
                            fs::copy(freqSrc, tempFreq, fs::copy_options::overwrite_existing);
                            // Luego renombrar el temporal al nombre final
                            fs::rename(tempFreq, freqDest);
                        }
                        catch (const fs::filesystem_error &e)
                        {
                            // Si falla, intentar copiar directamente
                            if (fs::exists(freqSrc))
                            {
                                fs::copy(freqSrc, freqDest, fs::copy_options::overwrite_existing);
                            }
                        }
                    }

                    cout << "[Thread " << data->threadId << "] Comprimido → " << outHuf << endl;
                    data->success = true;
                }
            }
        }
        else if (data->mode == "-d" || data->mode == "--decompress")
        {
            if (data->file.extension() == ".huf")
            {
                // Buscar archivo .freq
                fs::path freqFile = data->file.string() + ".freq";
                if (!fs::exists(freqFile))
                {
                    string hufStr = data->file.string();
                    if (hufStr.size() > 4 && hufStr.substr(hufStr.size() - 4) == ".huf")
                    {
                        string baseName = hufStr.substr(0, hufStr.size() - 4);
                        freqFile = baseName + ".freq";
                    }
                }

                if (fs::exists(freqFile))
                {
                    // Bloquear acceso a freqTable.bin para evitar conflictos entre threads
                    pthread_mutex_lock(&freqTableMutex);

                    try
                    {
                        // Copiar tabla de frecuencias al archivo que espera la función de descompresión
                        fs::copy(freqFile, "freqTable.bin", fs::copy_options::overwrite_existing);

                        // Leer y descomprimir
                        auto compressed = Huffman::readUncompressedFile(data->file.string());
                        if (!compressed.empty())
                        {
                            auto restored = Huffman::HuffmanDecompression(compressed);
                            fs::path output = data->file.string() + ".restored";
                            Huffman::writeFile(output.string(), restored);

                            cout << "[Thread " << data->threadId << "] Descomprimido → " << output << endl;
                            data->success = true;
                        }

                        // Limpiar freqTable.bin temporal
                        if (fs::exists("freqTable.bin"))
                            fs::remove("freqTable.bin");
                    }
                    catch (const exception &e)
                    {
                        cout << "[Thread " << data->threadId << "] ERROR en descompresión: " << e.what() << endl;
                        data->success = false;
                    }

                    // Desbloquear mutex
                    pthread_mutex_unlock(&freqTableMutex);
                }
                else
                {
                    cout << "[Thread " << data->threadId << "] ERROR: No se encontró " << freqFile << endl;
                    data->success = false;
                }
            }
        }
        else if (data->mode == "-e" || data->mode == "--encrypt")
        {
            vector<char> fileData = Huffman::readUncompressedFile(data->file.string());
            if (!fileData.empty())
            {
                vector<char> encrypted = Vigenere::VigenereEncryption(fileData, data->key);
                fs::path outEnc = data->file.string() + ".enc";
                Huffman::writeFile(outEnc.string(), encrypted);
                cout << "[Thread " << data->threadId << "] Encriptado → " << outEnc << endl;
                data->success = true;
            }
        }
        else if (data->mode == "-z" || data->mode == "--decrypt")
        {
            if (data->file.extension() == ".enc")
            {
                vector<char> encrypted = Huffman::readUncompressedFile(data->file.string());
                if (!encrypted.empty())
                {
                    vector<char> decrypted = Vigenere::VigenereDecryption(encrypted, data->key);
                    fs::path outDec = data->file.string() + ".dec";
                    Huffman::writeFile(outDec.string(), decrypted);
                    cout << "[Thread " << data->threadId << "] Desencriptado → " << outDec << endl;
                    data->success = true;
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

    // Check if the path is a folder or a file
    if (fs::exists(input))
    {
        if (fs::is_regular_file(input))
        {
            cout << "Procesando archivo: " << input << endl;
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
                    // Filtrar archivos según el modo
                    bool shouldProcess = false;

                    if (mode == "-c" || mode == "--compress")
                    {
                        shouldProcess = isCompressibleFile(entry.path());
                    }
                    else if (mode == "-d" || mode == "--decompress")
                    {
                        shouldProcess = (entry.path().extension() == ".huf");
                    }
                    else if (mode == "-e" || mode == "--encrypt")
                    {
                        shouldProcess = true; // Encriptar todos los archivos
                    }
                    else if (mode == "-z" || mode == "--decrypt")
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

    // Destruir el mutex antes de salir
    pthread_mutex_destroy(&freqTableMutex);

    return 0;
}