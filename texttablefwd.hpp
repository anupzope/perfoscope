#ifndef _PERFOSCOPE_TEXTTABLEFWD_HPP_
#define _PERFOSCOPE_TEXTTABLEFWD_HPP_

#include <iosfwd>

class TextTable;

std::ostream & operator << (std::ostream &stream, const TextTable &table);

#endif // #ifndef _PERFOSCOPE_TEXTTABLEFWD_HPP_
