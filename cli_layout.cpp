// cli_layout.cpp
// C++17. Estructura de CLI concurrente para comprimir/descomprimir y encriptar/desencriptar.
// Compilar: g++ -std=c++17 -O2 -pthread cli_layout.cpp -o clitool
// Uso rápido: ./clitool -ce --comp-alg huffman --enc-alg xor -i in_dir -o out_dir -k secret

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ====== Tu API de compresión ======
// La daremos por existente según tu requerimiento:
// Use the Huffman implementation in Huffman.cpp
#include "Huffman.h"

// Opcional: si tienes descompresión
static std::vector<char> HuffmanDecompress(const std::vector<char> &data)
{
    // Use the real Huffman decompression from Huffman.cpp
    return Huffman::HuffmanDecompression(data);
}

// ====== Utilidades de E/S binaria ======
static std::vector<char> read_all(const fs::path &p)
{
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("No se puede abrir: " + p.string());
    ifs.seekg(0, std::ios::end);
    std::streamsize sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<char> buf(static_cast<size_t>(sz));
    if (sz > 0 && !ifs.read(buf.data(), sz))
    {
        throw std::runtime_error("Error leyendo: " + p.string());
    }
    return buf;
}

static void write_all(const fs::path &p, const std::vector<char> &data)
{
    fs::create_directories(p.parent_path());
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs)
        throw std::runtime_error("No se puede crear: " + p.string());
    if (!data.empty())
        ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
}

// ====== Encriptación placeholder ======
static std::vector<char> xor_encrypt(const std::vector<char> &data, const std::string &key)
{
    if (key.empty())
        throw std::runtime_error("Clave vacía");
    std::vector<char> out(data);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] ^= key[i % key.size()];
    return out;
}
static std::vector<char> xor_decrypt(const std::vector<char> &data, const std::string &key)
{
    return xor_encrypt(data, key); // XOR simétrica
}

// ====== Operaciones encadenables ======
enum class OpKind
{
    Compress,
    Decompress,
    Encrypt,
    Decrypt
};

struct Op
{
    OpKind kind;
};

enum class CompAlg
{
    Huffman /*, Deflate, LZ4, etc.*/
};
enum class EncAlg
{
    XOR /*, AES, ChaCha20, etc.*/
};

struct Options
{
    std::vector<Op> ops_in_order; // orden según flags (-ce => [C, E])
    std::optional<CompAlg> comp_alg;
    std::optional<EncAlg> enc_alg;
    fs::path input;
    fs::path output;
    std::optional<std::string> key;
    unsigned workers = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4;
};

static void print_help(const char *argv0)
{
    std::cout <<
        R"(Uso:
  )" << argv0 << R"( [operaciones] [opciones] -i <entrada> -o <salida>

Operaciones (pueden combinarse y el orden importa):
  -c    Comprimir
  -d    Descomprimir
  -e    Encriptar
  -u    Desencriptar
  Ej: -ce  (comprimir luego encriptar)
      -du  (desencriptar luego descomprimir)

Opciones:
  --comp-alg <nombre>    Algoritmo de compresión (ej: huffman)
  --enc-alg  <nombre>    Algoritmo de encriptación (ej: xor)
  -i <ruta>              Archivo o directorio de entrada
  -o <ruta>              Archivo o directorio de salida
  -k <clave>             Clave (requerida para -e/-u)
  --workers <N>          Número de hilos (por defecto: #CPUs)
  -h, --help             Ayuda

Ejemplos:
  )" << argv0 << R"( -ce --comp-alg huffman --enc-alg xor -i ./in -o ./out -k secreto
  )" << argv0 << R"( -d --comp-alg huffman -i file.huff -o file.raw
)";
}

static std::optional<CompAlg> parse_comp_alg(const std::string &s)
{
    if (s == "huffman")
        return CompAlg::Huffman;
    return std::nullopt;
}
static std::optional<EncAlg> parse_enc_alg(const std::string &s)
{
    if (s == "xor")
        return EncAlg::XOR;
    return std::nullopt;
}

static Options parse_args(int argc, char **argv)
{
    Options opt;
    if (argc == 1)
    {
        print_help(argv[0]);
        std::exit(0);
    }

    auto need_value = [&](int i)
    {
        if (i + 1 >= argc)
            throw std::runtime_error(std::string("Falta valor para ") + argv[i]);
    };

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];

        if (a == "-h" || a == "--help")
        {
            print_help(argv[0]);
            std::exit(0);
        }

        if (a.size() > 1 && a[0] == '-' && a[1] != '-')
        {
            // flags cortas combinadas, ej: -ce
            for (size_t j = 1; j < a.size(); ++j)
            {
                char f = a[j];
                switch (f)
                {
                case 'c':
                    opt.ops_in_order.push_back({OpKind::Compress});
                    break;
                case 'd':
                    opt.ops_in_order.push_back({OpKind::Decompress});
                    break;
                case 'e':
                    opt.ops_in_order.push_back({OpKind::Encrypt});
                    break;
                case 'u':
                    opt.ops_in_order.push_back({OpKind::Decrypt});
                    break;
                case 'i':
                    need_value(i);
                    opt.input = argv[++i];
                    j = a.size();
                    break;
                case 'o':
                    need_value(i);
                    opt.output = argv[++i];
                    j = a.size();
                    break;
                case 'k':
                    need_value(i);
                    opt.key = argv[++i];
                    j = a.size();
                    break;
                default:
                    throw std::runtime_error(std::string("Flag desconocida -") + f);
                }
            }
            continue;
        }

        if (a.rfind("--comp-alg", 0) == 0)
        {
            std::string v;
            if (a == "--comp-alg")
            {
                need_value(i);
                v = argv[++i];
            }
            else if (a.rfind("--comp-alg=", 0) == 0)
                v = a.substr(11);
            else
                throw std::runtime_error("Sintaxis --comp-alg inválida");
            opt.comp_alg = parse_comp_alg(v);
            if (!opt.comp_alg)
                throw std::runtime_error("Algoritmo de compresión no soportado: " + v);
            continue;
        }
        if (a.rfind("--enc-alg", 0) == 0)
        {
            std::string v;
            if (a == "--enc-alg")
            {
                need_value(i);
                v = argv[++i];
            }
            else if (a.rfind("--enc-alg=", 0) == 0)
                v = a.substr(10);
            else
                throw std::runtime_error("Sintaxis --enc-alg inválida");
            opt.enc_alg = parse_enc_alg(v);
            if (!opt.enc_alg)
                throw std::runtime_error("Algoritmo de encriptación no soportado: " + v);
            continue;
        }
        if (a == "--workers")
        {
            need_value(i);
            opt.workers = std::max(1, std::stoi(argv[++i]));
            continue;
        }

        // Posicional inesperado
        throw std::runtime_error("Argumento desconocido: " + a);
    }

    // Validaciones mínimas
    if (opt.ops_in_order.empty())
        throw std::runtime_error("Debes especificar al menos una operación (-c, -d, -e, -u).");
    if (opt.input.empty())
        throw std::runtime_error("Falta -i <entrada>.");
    if (opt.output.empty())
        throw std::runtime_error("Falta -o <salida>.");
    // Si hay e/u debe haber clave
    bool needs_key = std::any_of(opt.ops_in_order.begin(), opt.ops_in_order.end(),
                                 [](const Op &op)
                                 { return op.kind == OpKind::Encrypt || op.kind == OpKind::Decrypt; });
    if (needs_key && !opt.key)
        throw std::runtime_error("Debes pasar -k <clave> para encriptar/desencriptar.");
    // Si hay c/d debe haber comp-alg
    bool needs_comp = std::any_of(opt.ops_in_order.begin(), opt.ops_in_order.end(),
                                  [](const Op &op)
                                  { return op.kind == OpKind::Compress || op.kind == OpKind::Decompress; });
    if (needs_comp && !opt.comp_alg)
        throw std::runtime_error("Debes indicar --comp-alg <algoritmo>.");

    return opt;
}

// ====== Thread Pool simple ======
class ThreadPool
{
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    explicit ThreadPool(unsigned n)
    {
        for (unsigned i = 0; i < n; ++i)
        {
            workers_.emplace_back([this]
                                  {
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lk(m_);
                        cv_.wait(lk, [this]{ return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        job = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    job();
                } });
        }
    }
    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lk(m_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto &t : workers_)
            t.join();
    }
    void enqueue(std::function<void()> job)
    {
        {
            std::lock_guard<std::mutex> lk(m_);
            tasks_.push(std::move(job));
        }
        cv_.notify_one();
    }
};

// ====== Pipeline de archivo ======

static std::vector<char> apply_compress(const std::vector<char> &in, CompAlg alg)
{
    switch (alg)
    {
    case CompAlg::Huffman:
    {
        // Call the Huffman compressor implementation and return its buffer.
        return Huffman::HuffmanCompression(in);
    }
    }
    return in;
}

static std::vector<char> apply_decompress(const std::vector<char> &in, CompAlg alg)
{
    switch (alg)
    {
    case CompAlg::Huffman:
        return HuffmanDecompress(in);
    }
    return in;
}

static std::vector<char> apply_encrypt(const std::vector<char> &in, EncAlg alg, const std::string &key)
{
    switch (alg)
    {
    case EncAlg::XOR:
        return xor_encrypt(in, key);
    }
    return in;
}

static std::vector<char> apply_decrypt(const std::vector<char> &in, EncAlg alg, const std::string &key)
{
    switch (alg)
    {
    case EncAlg::XOR:
        return xor_decrypt(in, key);
    }
    return in;
}

static std::vector<char> run_pipeline(const std::vector<char> &input,
                                      const std::vector<Op> &ops,
                                      const Options &opt)
{
    std::vector<char> cur = input;
    for (const auto &op : ops)
    {
        switch (op.kind)
        {
        case OpKind::Compress:
            cur = apply_compress(cur, *opt.comp_alg);
            break;
        case OpKind::Decompress:
            cur = apply_decompress(cur, *opt.comp_alg);
            break;
        case OpKind::Encrypt:
            cur = apply_encrypt(cur, *opt.enc_alg, *opt.key);
            break;
        case OpKind::Decrypt:
            cur = apply_decrypt(cur, *opt.enc_alg, *opt.key);
            break;
        }
    }
    return cur;
}

// Calcula ruta de salida preservando estructura cuando input es directorio
static fs::path map_output_path(const fs::path &input_root, const fs::path &input_file, const fs::path &out_root)
{
    if (fs::is_regular_file(input_root))
    {
        // Usuario dio archivo: si salida es directorio, conservar nombre; si es archivo, usarlo tal cual
        if (fs::is_directory(out_root))
            return out_root / input_root.filename();
        return out_root;
    }
    else
    {
        // Usuario dio directorio: replicar estructura relativa
        auto rel = fs::relative(input_file, input_root);
        return out_root / rel;
    }
}

// ====== Main ======
int main(int argc, char **argv)
{
    try
    {
        Options opt = parse_args(argc, argv);

        // Construir lista de archivos a procesar
        std::vector<fs::path> files;
        if (fs::is_regular_file(opt.input))
        {
            files.push_back(opt.input);
        }
        else if (fs::is_directory(opt.input))
        {
            for (auto &entry : fs::recursive_directory_iterator(opt.input))
            {
                if (entry.is_regular_file())
                    files.push_back(entry.path());
            }
        }
        else
        {
            throw std::runtime_error("La entrada no existe o no es archivo/directorio válido.");
        }

        if (files.empty())
        {
            std::cerr << "No hay archivos que procesar.\n";
            return 0;
        }

        // Preparar salida
        if (fs::exists(opt.output) && fs::is_regular_file(opt.output) && files.size() > 1)
        {
            throw std::runtime_error("Salida apunta a archivo pero hay múltiples entradas.");
        }
        if (!fs::exists(opt.output))
        {
            // Si salida pretende ser directorio para múltiples entradas, créalo
            if (files.size() > 1 || fs::is_directory(opt.input))
            {
                fs::create_directories(opt.output);
            }
        }

        std::atomic<size_t> done{0};
        std::mutex log_m;
        ThreadPool pool(opt.workers);

        for (const auto &f : files)
        {
            pool.enqueue([&, f]
                         {
                try {
                    auto in_data = read_all(f);
                    auto out_data = run_pipeline(in_data, opt.ops_in_order, opt);

                    fs::path out_path = map_output_path(opt.input, f, opt.output);

                    // Opcional: extensions según operaciones (solo ejemplo)
                    // -c => añade ".cmp", -e => ".enc"; -d/-u => quita si corresponde
                    for (const auto& op : opt.ops_in_order) {
                        if (op.kind == OpKind::Compress) out_path += ".cmp";
                        if (op.kind == OpKind::Encrypt)  out_path += ".enc";
                        if (op.kind == OpKind::Decompress && out_path.extension() == ".cmp") out_path.replace_extension();
                        if (op.kind == OpKind::Decrypt    && out_path.extension() == ".enc") out_path.replace_extension();
                    }

                    write_all(out_path, out_data);

                    size_t cur = ++done;
                    std::lock_guard<std::mutex> lk(log_m);
                    std::cout << "[" << cur << "/" << files.size() << "] "
                              << f << " -> " << out_path << "\n";
                } catch (const std::exception& ex) {
                    std::lock_guard<std::mutex> lk(log_m);
                    std::cerr << "Error procesando " << f << ": " << ex.what() << "\n";
                } });
        }

        // Espera en destructor del pool
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Fallo: " << ex.what() << "\n";
        print_help(argv[0]);
        return 1;
    }
    return 0;
}
