# std/csv.rave

This module provides functionality for parsing and generating CSV (Comma-Separated Values) data.

## std::csv::Row

This structure represents a single row in a CSV document, containing multiple cells.

### add

Adds a cell to the row.

### get

Returns the cell at the specified index.

### length

The number of cells in the row.

### toString

Converts the row to a CSV-formatted string. Cells containing commas, quotes, or newlines are automatically quoted and escaped according to CSV standards.

## std::csv::Document

This structure represents a complete CSV document, containing multiple rows.

### addRow

Adds a row to the document. The row's memory is transferred to the document, so the original row should not be manually freed.

### get

Returns a pointer to the row at the specified index. Returns `null` if the index is out of bounds.

### length

The number of rows in the document.

### toString

Converts the entire document to a CSV-formatted string with each row on a new line.

## std::csv::parse

Parses a CSV string and returns a `std::csv::Document`. Accepts either `std::cstring` or `std::string`.

## CSV Format Support

The parser handles:
- Standard comma separators
- Quoted fields (cells containing commas, quotes, or newlines)
- Escaped double quotes (`""` inside quoted fields)
- Trailing/leading spaces in unquoted fields
