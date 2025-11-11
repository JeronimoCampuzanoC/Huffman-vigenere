#ifndef VIGENERE_H
#define VIGENERE_H

#include <string>
#include <vector>

class Vigenere
{
public:
    // Cifra el contenido usando el cifrado Vigenere
    static std::vector<char> VigenereEncryption(const std::vector<char> &data, const std::string &key);

    // Descifra el contenido usando el cifrado Vigenere
    static std::vector<char> VigenereDecryption(const std::vector<char> &data, const std::string &key);

private:
    // Normaliza la clave para que tenga el mismo tamaño que los datos
    static std::string normalizeKey(const std::string &key, size_t dataLength);

    // Cifra un solo carácter
    static char encryptChar(char plainChar, char keyChar);

    // Descifra un solo carácter
    static char decryptChar(char cipherChar, char keyChar);
};

#endif // VIGENERE_H
