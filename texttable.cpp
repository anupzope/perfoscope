#include "texttable.hpp"

#include <iomanip>
#include <sstream>

TextTable::TextTable(const size_t rows, const size_t columns, const size_t padding) : m_rows(rows), m_columns(columns), m_padding(padding), m_text_data(rows*columns) {
}

std::string & TextTable::at(const size_t row_index, const size_t column_index) {
  size_t linear_index = row_index + column_index*m_rows;
  return m_text_data[linear_index];
}

const std::string & TextTable::at(const size_t row_index, const size_t column_index) const {
  size_t linear_index = row_index + column_index*m_rows;
  return m_text_data[linear_index];
}

std::ostream & operator << (std::ostream &stream, const TextTable &table) {
  std::vector<size_t> column_width(table.m_columns, 0);
  
  size_t total_width = 0;
  size_t linear_index = 0;
  for(size_t ci = 0; ci < table.m_columns; ++ci) {
    for(size_t ri = 0; ri < table.m_rows; ++ri) {
      //if(table.m_text_data[linear_index].find('\n') != std::string::npos) {
      //  std::cerr << "Multiline text not allowed in TextTable cell" << table.m_text_data[linear_index] << std::endl;
      //  std::exit(1);
      //}
      size_t length = table.m_text_data[linear_index].length();
      if(column_width[ci] < length) {
        column_width[ci] = length;
      }
      ++linear_index;
    }
    total_width += column_width[ci] + table.m_padding;
  }
  total_width -= table.m_padding;
  
  std::string header_seperator(total_width, '-');
  std::string seperator(total_width, '=');
  std::string margin(table.m_padding, ' ');
  std::stringstream output;
  output << seperator << std::endl;
  for(size_t ri = 0; ri < table.m_rows; ++ri) {
    for(size_t ci = 0; ci < table.m_columns; ++ci) {
      const size_t linear_index = ri + ci*table.m_rows;
      output << std::setw(column_width[ci]) << table.m_text_data[linear_index];
      if(ci < table.m_columns-1) {
        output << margin;
      }
    }
    output << std::endl;
    if(ri == 0) {
      output << header_seperator << std::endl;
    }
  }
  output << seperator << std::endl;
  
  stream << output.str();
  
  return stream;
}
