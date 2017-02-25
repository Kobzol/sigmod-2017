#include "input.h"

bool input_read_line(std::istream& input, std::string& line)
{
    line.clear();

    const int eof = EOF;
    int c = input.rdbuf()->sgetc();
    while (c != eof && c != '\n')
    {
        line += (char) c;
        c = input.rdbuf()->snextc();
    }

    if (c == EOF)
    {
        return false;
    }
    else input.rdbuf()->sbumpc();   // skip over newline

    return line.size() > 0;
}
