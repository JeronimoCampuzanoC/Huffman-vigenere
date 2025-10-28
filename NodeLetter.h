#ifndef NODELETTER_H
#define NODELETTER_H
class NodeLetter
{
public:
    NodeLetter(int identificador, char letra)
        : der(nullptr), izq(nullptr), id(identificador), letra(letra) {}

    NodeLetter *der;
    NodeLetter *izq;
    int id;
    char letra;
};

#endif