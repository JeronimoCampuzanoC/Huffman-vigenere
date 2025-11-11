#include "Vigenere.h"
#include <stdexcept>
#include <unordered_map>

// Diccionario para mapear letras a sus posiciones en el alfabeto
static const std::unordered_map<char, int> letterToPosition = {
    // Letras minúsculas (0-25)
    {'a', 0},
    {'b', 1},
    {'c', 2},
    {'d', 3},
    {'e', 4},
    {'f', 5},
    {'g', 6},
    {'h', 7},
    {'i', 8},
    {'j', 9},
    {'k', 10},
    {'l', 11},
    {'m', 12},
    {'n', 13},
    {'o', 14},
    {'p', 15},
    {'q', 16},
    {'r', 17},
    {'s', 18},
    {'t', 19},
    {'u', 20},
    {'v', 21},
    {'w', 22},
    {'x', 23},
    {'y', 24},
    {'z', 25},
    // Letras mayúsculas (26-51)
    {'A', 26},
    {'B', 27},
    {'C', 28},
    {'D', 29},
    {'E', 30},
    {'F', 31},
    {'G', 32},
    {'H', 33},
    {'I', 34},
    {'J', 35},
    {'K', 36},
    {'L', 37},
    {'M', 38},
    {'N', 39},
    {'O', 40},
    {'P', 41},
    {'Q', 42},
    {'R', 43},
    {'S', 44},
    {'T', 45},
    {'U', 46},
    {'V', 47},
    {'W', 48},
    {'X', 49},
    {'Y', 50},
    {'Z', 51}};

// Cifra el contenido usando el cifrado Vigenere
std::vector<char> Vigenere::VigenereEncryption(const std::vector<char> &data, const std::string &key)
{
    const std::string normalizedKey = normalizeKey(key, data.size());
    std::string encryptedMessage;

    if (normalizedKey.empty())
    {
        throw std::runtime_error("La clave no puede estar vacía");
    }

    if (data.empty())
    {
        return std::vector<char>();
    }

    for (int i = 0; i < data.size(); i++)
    {
        int value = letterToPosition.at(data[i]) + letterToPosition.at(normalizedKey[i]);

        if (value > 51)
        {
            value = value % 51;
        }
        encryptedMessage[i] = letterToPosition.at(data[i]) + letterToPosition.at(normalizedKey[i]);
    }

    return std::vector<char>(encryptedMessage.begin(), encryptedMessage.end());
}

// Descifra el contenido usando el cifrado Vigenere
std::vector<char> Vigenere::VigenereDecryption(const std::vector<char> &data, const std::string &key)
{
    const std::string normalizedKey = normalizeKey(key, data.size());
    std::string encryptedMessage;

    if (normalizedKey.empty())
    {
        throw std::runtime_error("La clave no puede estar vacía");
    }

    if (data.empty())
    {
        return std::vector<char>();
    }

    for (int i = 0; i < data.size(); i++)
    {
        int value = letterToPosition.at(data[i]) + letterToPosition.at(normalizedKey[i]);

        if (value > 51)
        {
            value = value % 51;
        }
        encryptedMessage[i] = letterToPosition.at(data[i]) - letterToPosition.at(normalizedKey[i]);
    }

    return std::vector<char>(encryptedMessage.begin(), encryptedMessage.end());
}

// Normaliza la clave para que tenga el mismo tamaño que los datos
std::string Vigenere::normalizeKey(const std::string &key, size_t dataLength)
{
    std::string normalizedKey;
    normalizedKey.reserve(dataLength);

    // Repetir la clave cíclicamente hasta alcanzar dataLength
    for (size_t i = 0; i < dataLength; i++)
    {
        normalizedKey += key[i % key.size()];
    }

    return normalizedKey;
}

// Cifra un solo carácter
char Vigenere::encryptChar(char plainChar, char keyChar)
{
    // TODO: Implementar cifrado de un carácter
    // Formula: C = (P + K) mod 256 (para bytes completos)
    // o usar el alfabeto A-Z tradicional

    return plainChar;
}

// Descifra un solo carácter
char Vigenere::decryptChar(char cipherChar, char keyChar)
{
    // TODO: Implementar descifrado de un carácter
    // Formula: P = (C - K) mod 256 (para bytes completos)
    // o usar el alfabeto A-Z tradicional

    return cipherChar;
}
