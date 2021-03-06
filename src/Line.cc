#include "Line.h"
#include <sstream>

using namespace std;

Line::Line(const unsigned int number, const string str) : number(number), line(str) {}

bool Line::equals(const Line& other) const {
    return line == other.line;
}

unsigned int Line::getNumber() const {
    return number;
}

string Line::getString() const {
    return line;
}

void Line::setLineNumber(unsigned int newNumber) {
  number = newNumber;
}
