#ifndef _PERFOSCOPE_TEXTTABLE_HPP_
#define _PERFOSCOPE_TEXTTABLE_HPP_

#include "texttablefwd.hpp"

#include <vector>
#include <string>

class TextTable {
friend std::ostream & operator << (std::ostream &stream, const TextTable &table);

public:
  TextTable(const size_t rows, const size_t columns, const size_t padding);
  
  std::string & at(const size_t row_index, const size_t column_index);
  const std::string & at(const size_t row_index, const size_t column_index) const;
  
private:
  size_t m_rows;
  size_t m_columns;
  size_t m_padding;
  std::vector<std::string> m_text_data;
};

#endif // #ifndef _PERFOSCOPE_TEXTTABLE_HPP_
