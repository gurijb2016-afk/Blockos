#include "../include/ctype.h"

extern "C"
{

int isspace(int c)
{
    return c == ' '  ||
           c == '\t' ||
           c == '\n' ||
           c == '\r' ||
           c == '\f' ||
           c == '\v';
}


int isdigit(int c)
{
    return c >= '0' && c <= '9';
}


int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}


int islower(int c)
{
    return c >= 'a' && c <= 'z';
}


int isalpha(int c)
{
    return isupper(c) || islower(c);
}


int toupper(int c)
{
    if (islower(c))
        return c - ('a' - 'A');

    return c;
}


int tolower(int c)
{
    if (isupper(c))
        return c + ('a' - 'A');

    return c;
}

}
