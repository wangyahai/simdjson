#ifndef SIMDJSON_DOCUMENT_H
#define SIMDJSON_DOCUMENT_H

#include <cstring>
#include <memory>
#include <string>
#include <limits>
#include "simdjson/common_defs.h"
#include "simdjson/simdjson.h"
#include "simdjson/padded_string.h"

#define JSON_VALUE_MASK 0x00FFFFFFFFFFFFFF
#define DEFAULT_MAX_DEPTH 1024 // a JSON document with a depth exceeding 1024 is probably de facto invalid

namespace simdjson {

template<size_t max_depth> class document_iterator;

/**
 * A parsed JSON document.
 *
 * This class cannot be copied, only moved, to avoid unintended allocations.
 */
class document {
public:
  /**
   * Create a document container with zero capacity.
   *
   * The parser will allocate capacity as needed.
   */
  document() noexcept=default;
  ~document() noexcept=default;

  /**
   * Take another document's buffers.
   *
   * @param other The document to take. Its capacity is zeroed and it is invalidated.
   */
  document(document &&other) noexcept = default;
  document(const document &) = delete; // Disallow copying
  /**
   * Take another document's buffers.
   *
   * @param other The document to take. Its capacity is zeroed.
   */
  document &operator=(document &&other) noexcept = default;
  document &operator=(const document &) = delete; // Disallow copying

  // Nested classes
  class element;
  class array;
  class object;
  class key_value_pair;
  class parser;
  class stream;

  template<typename T=element>
  class element_result;
  class doc_result;
  class doc_ref_result;
  class stream_result;

  // Nested classes. See definitions later in file.
  using iterator = document_iterator<DEFAULT_MAX_DEPTH>;

  /**
   * Get the root element of this document as a JSON array.
   */
  element root() const noexcept;
  /**
   * Get the root element of this document as a JSON array.
   */
  element_result<array> as_array() const noexcept;
  /**
   * Get the root element of this document as a JSON object.
   */
  element_result<object> as_object() const noexcept;
  /**
   * Get the root element of this document.
   */
  operator element() const noexcept;
  /**
   * Read the root element of this document as a JSON array.
   *
   * @return The JSON array.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not an array
   */
  operator array() const noexcept(false);
  /**
   * Read this element as a JSON object (key/value pairs).
   *
   * @return The JSON object.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not an object
   */
  operator object() const noexcept(false);

  /**
   * Get the value associated with the given key.
   *
   * The key will be matched against **unescaped** JSON:
   *
   *   document::parse(R"({ "a\n": 1 })")["a\n"].as_uint64_t().value == 1
   *   document::parse(R"({ "a\n": 1 })")["a\\n"].as_uint64_t().error == NO_SUCH_FIELD
   *
   * @return The value associated with the given key, or:
   *         - NO_SUCH_FIELD if the field does not exist in the object
   *         - UNEXPECTED_TYPE if the document is not an object
   */
  element_result<element> operator[](const std::string_view &s) const noexcept;
  /**
   * Get the value associated with the given key.
   *
   * The key will be matched against **unescaped** JSON:
   *
   *   document::parse(R"({ "a\n": 1 })")["a\n"].as_uint64_t().value == 1
   *   document::parse(R"({ "a\n": 1 })")["a\\n"].as_uint64_t().error == NO_SUCH_FIELD
   *
   * @return The value associated with this field, or:
   *         - NO_SUCH_FIELD if the field does not exist in the object
   *         - UNEXPECTED_TYPE if the document is not an object
   */
  element_result<element> operator[](const char *s) const noexcept;

  /**
   * Print this JSON to a std::ostream.
   *
   * @param os the stream to output to.
   * @param max_depth the maximum JSON depth to output.
   * @return false if the tape is likely wrong (e.g., you did not parse a valid JSON).
   */
  bool print_json(std::ostream &os, size_t max_depth=DEFAULT_MAX_DEPTH) const noexcept;
  /**
   * Dump the raw tape for debugging.
   *
   * @param os the stream to output to.
   * @return false if the tape is likely wrong (e.g., you did not parse a valid JSON).
   */
  bool dump_raw_tape(std::ostream &os) const noexcept;

  /**
   * Parse a JSON document and return a reference to it.
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated. If realloc_if_needed is true,
   * it is assumed that the buffer does *not* have enough padding, and it is reallocated, enlarged
   * and copied before parsing.
   *
   * @param buf The JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes, unless
   *            realloc_if_needed is true.
   * @param len The length of the JSON.
   * @param realloc_if_needed Whether to reallocate and enlarge the JSON buffer to add padding.
   * @return the document, or an error if the JSON is invalid.
   */
  inline static doc_result parse(const uint8_t *buf, size_t len, bool realloc_if_needed = true) noexcept;

  /**
   * Parse a JSON document.
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated. If realloc_if_needed is true,
   * it is assumed that the buffer does *not* have enough padding, and it is reallocated, enlarged
   * and copied before parsing.
   *
   * @param buf The JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes, unless
   *            realloc_if_needed is true.
   * @param len The length of the JSON.
   * @param realloc_if_needed Whether to reallocate and enlarge the JSON buffer to add padding.
   * @return the document, or an error if the JSON is invalid.
   */
  really_inline static doc_result parse(const char *buf, size_t len, bool realloc_if_needed = true) noexcept;

  /**
   * Parse a JSON document.
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated. If `str.capacity() - str.size()
   * < SIMDJSON_PADDING`, the string will be copied to a string with larger capacity before parsing.
   *
   * @param s The JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes, or
   *          a new string will be created with the extra padding.
   * @return the document, or an error if the JSON is invalid.
   */
  really_inline static doc_result parse(const std::string &s) noexcept;

  /**
   * Parse a JSON document.
   *
   * @param s The JSON to parse.
   * @return the document, or an error if the JSON is invalid.
   */
  really_inline static doc_result parse(const padded_string &s) noexcept;

  // We do not want to allow implicit conversion from C string to std::string.
  doc_ref_result parse(const char *buf, bool realloc_if_needed = true) noexcept = delete;

  std::unique_ptr<uint64_t[]> tape;
  std::unique_ptr<uint8_t[]> string_buf;// should be at least byte_capacity

private:
  class tape_ref;
  enum class tape_type;
  bool set_capacity(size_t len);
}; // class document

/**
 * A parsed, *owned* document, or an error if the parse failed.
 *
 *     document &doc = document::parse(json);
 *
 * Returns an owned `document`. When the doc_result (or the document retrieved from it) goes out of
 * scope, the document's memory is deallocated.
 *
 * ## Error Codes vs. Exceptions
 *
 * This result type allows the user to pick whether to use exceptions or not.
 *
 * Use like this to avoid exceptions:
 *
 *     auto [doc, error] = document::parse(json);
 *     if (error) { exit(1); }
 *
 * Use like this if you'd prefer to use exceptions:
 *
 *     document doc = document::parse(json);
 *
 */
class document::doc_result {
public:
  /**
   * The parsed document. This is *invalid* if there is an error.
   */
  document doc;
  /**
   * The error code, or SUCCESS (0) if there is no error.
   */
  error_code error;

  /**
   * Return the document, or throw an exception if it is invalid.
   *
   * @return the document.
   * @exception simdjson_error if the document is invalid or there was an error parsing it.
   */
  operator document() noexcept(false);

  ~doc_result() noexcept=default;

private:
  doc_result(document &&_doc, error_code _error) noexcept;
  doc_result(document &&_doc) noexcept;
  doc_result(error_code _error) noexcept;
  friend class document;
}; // class document::doc_result

/**
 * A parsed document reference, or an error if the parse failed.
 *
 *     document &doc = document::parse(json);
 *
 * ## Document Ownership
 *
 * The `document &` refers to an internal document the parser reuses on each `parse()` call. It will
 * become invalidated on the next `parse()`.
 *
 * This is more efficient for common cases where documents are parsed and used one at a time. If you
 * need to keep the document around longer, you may *take* it from the parser by casting it:
 *
 *     document doc = parser.parse(); // take ownership
 *
 * If you do this, the parser will automatically allocate a new document on the next `parse()` call.
 *
 * ## Error Codes vs. Exceptions
 *
 * This result type allows the user to pick whether to use exceptions or not.
 *
 * Use like this to avoid exceptions:
 *
 *     auto [doc, error] = parser.parse(json);
 *     if (error) { exit(1); }
 *
 * Use like this if you'd prefer to use exceptions:
 *
 *     document &doc = document::parse(json);
 *
 */
class document::doc_ref_result {
public:
  /**
   * The parsed document. This is *invalid* if there is an error.
   */
  document &doc;
  /**
   * The error code, or SUCCESS (0) if there is no error.
   */
  error_code error;

  /**
   * A reference to the document, or throw an exception if it is invalid.
   *
   * @return the document.
   * @exception simdjson_error if the document is invalid or there was an error parsing it.
   */
  operator document&() noexcept(false);

  ~doc_ref_result()=default;

private:
  doc_ref_result(document &_doc, error_code _error) noexcept;
  friend class document::parser;
  friend class document::stream;
}; // class document::doc_ref_result

/**
  * The possible types in the tape. Internal only.
  */
enum class document::tape_type {
  ROOT = 'r',
  START_ARRAY = '[',
  START_OBJECT = '{',
  END_ARRAY = ']',
  END_OBJECT = '}',
  STRING = '"',
  INT64 = 'l',
  UINT64 = 'u',
  DOUBLE = 'd',
  TRUE_VALUE = 't',
  FALSE_VALUE = 'f',
  NULL_VALUE = 'n'
};

/**
 * A reference to an element on the tape. Internal only.
 */
class document::tape_ref {
protected:
  really_inline tape_ref() noexcept;
  really_inline tape_ref(const document *_doc, size_t _json_index) noexcept;
  inline size_t after_element() const noexcept;
  really_inline tape_type type() const noexcept;
  really_inline uint64_t tape_value() const noexcept;
  template<typename T>
  really_inline T next_tape_value() const noexcept;

  /** The document this element references. */
  const document *doc;

  /** The index of this element on `doc.tape[]` */
  size_t json_index;

  friend class document::key_value_pair;
};

/**
 * A JSON element.
 *
 * References an element in a JSON document, representing a JSON null, boolean, string, number,
 * array or object.
 */
class document::element : protected document::tape_ref {
public:
  /** Whether this element is a json `null`. */
  really_inline bool is_null() const noexcept;
  /** Whether this is a JSON `true` or `false` */
  really_inline bool is_bool() const noexcept;
  /** Whether this is a JSON number (e.g. 1, 1.0 or 1e2) */
  really_inline bool is_number() const noexcept;
  /** Whether this is a JSON integer (e.g. 1 or -1, but *not* 1.0 or 1e2) */
  really_inline bool is_integer() const noexcept;
  /** Whether this is a JSON string (e.g. "abc") */
  really_inline bool is_string() const noexcept;
  /** Whether this is a JSON array (e.g. []) */
  really_inline bool is_array() const noexcept;
  /** Whether this is a JSON array (e.g. []) */
  really_inline bool is_object() const noexcept;

  /**
   * Read this element as a boolean (json `true` or `false`).
   *
   * @return The boolean value, or:
   *         - UNEXPECTED_TYPE error if the JSON element is not a boolean
   */
  inline element_result<bool> as_bool() const noexcept;

  /**
   * Read this element as a null-terminated string.
   *
   * Does *not* convert other types to a string; requires that the JSON type of the element was
   * an actual string.
   *
   * @return A `string_view` into the string, or:
   *         - UNEXPECTED_TYPE error if the JSON element is not a string
   */
  inline element_result<const char *> as_c_str() const noexcept;

  /**
   * Read this element as a C++ string_view (string with length).
   *
   * Does *not* convert other types to a string; requires that the JSON type of the element was
   * an actual string.
   *
   * @return A `string_view` into the string, or:
   *         - UNEXPECTED_TYPE error if the JSON element is not a string
   */
  inline element_result<std::string_view> as_string() const noexcept;

  /**
   * Read this element as an unsigned integer.
   *
   * @return The uninteger value, or:
   *         - UNEXPECTED_TYPE if the JSON element is not an integer
   *         - NUMBER_OUT_OF_RANGE if the integer doesn't fit in 64 bits or is negative
   */
  inline element_result<uint64_t> as_uint64_t() const noexcept;

  /**
   * Read this element as a signed integer.
   *
   * @return The integer value, or:
   *         - UNEXPECTED_TYPE if the JSON element is not an integer
   *         - NUMBER_OUT_OF_RANGE if the integer doesn't fit in 64 bits
   */
  inline element_result<int64_t> as_int64_t() const noexcept;

  /**
   * Read this element as a floating point value.
   *
   * @return The double value, or:
   *         - UNEXPECTED_TYPE if the JSON element is not a number
   */
  inline element_result<double> as_double() const noexcept;

  /**
   * Read this element as a JSON array.
   *
   * @return The array value, or:
   *         - UNEXPECTED_TYPE if the JSON element is not an array
   */
  inline element_result<document::array> as_array() const noexcept;

  /**
   * Read this element as a JSON object (key/value pairs).
   *
   * @return The object value, or:
   *         - UNEXPECTED_TYPE if the JSON element is not an object
   */
  inline element_result<document::object> as_object() const noexcept;

  /**
   * Read this element as a boolean.
   *
   * @return The boolean value
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not a boolean.
   */
  inline operator bool() const noexcept(false);

  /**
   * Read this element as a null-terminated string.
   *
   * Does *not* convert other types to a string; requires that the JSON type of the element was
   * an actual string.
   *
   * @return The string value.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not a string.
   */
  inline explicit operator const char*() const noexcept(false);

  /**
   * Read this element as a null-terminated string.
   *
   * Does *not* convert other types to a string; requires that the JSON type of the element was
   * an actual string.
   *
   * @return The string value.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not a string.
   */
  inline operator std::string_view() const noexcept(false);

  /**
   * Read this element as an unsigned integer.
   *
   * @return The integer value.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not an integer
   * @exception simdjson_error(NUMBER_OUT_OF_RANGE) if the integer doesn't fit in 64 bits or is negative
   */
  inline operator uint64_t() const noexcept(false);
  /**
   * Read this element as an signed integer.
   *
   * @return The integer value.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not an integer
   * @exception simdjson_error(NUMBER_OUT_OF_RANGE) if the integer doesn't fit in 64 bits
   */
  inline operator int64_t() const noexcept(false);
  /**
   * Read this element as an double.
   *
   * @return The double value.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not a number
   * @exception simdjson_error(NUMBER_OUT_OF_RANGE) if the integer doesn't fit in 64 bits or is negative
   */
  inline operator double() const noexcept(false);
  /**
   * Read this element as a JSON array.
   *
   * @return The JSON array.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not an array
   */
  inline operator document::array() const noexcept(false);
  /**
   * Read this element as a JSON object (key/value pairs).
   *
   * @return The JSON object.
   * @exception simdjson_error(UNEXPECTED_TYPE) if the JSON element is not an object
   */
  inline operator document::object() const noexcept(false);

  /**
   * Get the value associated with the given key.
   *
   * The key will be matched against **unescaped** JSON:
   *
   *   document::parse(R"({ "a\n": 1 })")["a\n"].as_uint64_t().value == 1
   *   document::parse(R"({ "a\n": 1 })")["a\\n"].as_uint64_t().error == NO_SUCH_FIELD
   *
   * @return The value associated with this field, or:
   *         - NO_SUCH_FIELD if the field does not exist in the object
   *         - UNEXPECTED_TYPE if the document is not an object
   */
  inline element_result<element> operator[](const std::string_view &s) const noexcept;
  /**
   * Get the value associated with the given key.
   *
   * Note: The key will be matched against **unescaped** JSON:
   *
   *   document::parse(R"({ "a\n": 1 })")["a\n"].as_uint64_t().value == 1
   *   document::parse(R"({ "a\n": 1 })")["a\\n"].as_uint64_t().error == NO_SUCH_FIELD
   *
   * @return The value associated with this field, or:
   *         - NO_SUCH_FIELD if the field does not exist in the object
   *         - UNEXPECTED_TYPE if the document is not an object
   */
  inline element_result<element> operator[](const char *s) const noexcept;

private:
  really_inline element() noexcept;
  really_inline element(const document *_doc, size_t _json_index) noexcept;
  friend class document;
  template<typename T>
  friend class document::element_result;
};

/**
 * Represents a JSON array.
 */
class document::array : protected document::tape_ref {
public:
  class iterator : tape_ref {
  public:
    /**
     * Get the actual value
     */
    inline element operator*() const noexcept;
    /**
     * Get the next value.
     *
     * Part of the std::iterator interface.
     */
    inline void operator++() noexcept;
    /**
     * Check if these values come from the same place in the JSON.
     *
     * Part of the std::iterator interface.
     */
    inline bool operator!=(const iterator& other) const noexcept;
  private:
    really_inline iterator(const document *_doc, size_t _json_index) noexcept;
    friend class array;
  };

  /**
   * Return the first array element.
   *
   * Part of the std::iterable interface.
   */
  inline iterator begin() const noexcept;
  /**
   * One past the last array element.
   *
   * Part of the std::iterable interface.
   */
  inline iterator end() const noexcept;

private:
  really_inline array() noexcept;
  really_inline array(const document *_doc, size_t _json_index) noexcept;
  friend class document::element;
  template<typename T>
  friend class document::element_result;
};

/**
 * Represents a JSON object.
 */
class document::object : protected document::tape_ref {
public:
  class iterator : protected document::tape_ref {
  public:
    /**
     * Get the actual key/value pair
     */
    inline const document::key_value_pair operator*() const noexcept;
    /**
     * Get the next key/value pair.
     *
     * Part of the std::iterator interface.
     */
    inline void operator++() noexcept;
    /**
     * Check if these key value pairs come from the same place in the JSON.
     *
     * Part of the std::iterator interface.
     */
    inline bool operator!=(const iterator& other) const noexcept;
    /**
     * Get the key of this key/value pair.
     */
    inline std::string_view key() const noexcept;
    /**
     * Get the key of this key/value pair.
     */
    inline const char *key_c_str() const noexcept;
    /**
     * Get the value of this key/value pair.
     */
    inline element value() const noexcept;
  private:
    really_inline iterator(const document *_doc, size_t _json_index) noexcept;
    friend class document::object;
  };

  /**
   * Return the first key/value pair.
   *
   * Part of the std::iterable interface.
   */
  inline iterator begin() const noexcept;
  /**
   * One past the last key/value pair.
   *
   * Part of the std::iterable interface.
   */
  inline iterator end() const noexcept;

  /**
   * Get the value associated with the given key.
   *
   * The key will be matched against **unescaped** JSON:
   *
   *   document::parse(R"({ "a\n": 1 })")["a\n"].as_uint64_t().value == 1
   *   document::parse(R"({ "a\n": 1 })")["a\\n"].as_uint64_t().error == NO_SUCH_FIELD
   *
   * @return The value associated with this field, or:
   *         - NO_SUCH_FIELD if the field does not exist in the object
   */
  inline element_result<element> operator[](const std::string_view &s) const noexcept;
  /**
   * Get the value associated with the given key.
   *
   * Note: The key will be matched against **unescaped** JSON:
   *
   *   document::parse(R"({ "a\n": 1 })")["a\n"].as_uint64_t().value == 1
   *   document::parse(R"({ "a\n": 1 })")["a\\n"].as_uint64_t().error == NO_SUCH_FIELD
   *
   * @return The value associated with this field, or:
   *         - NO_SUCH_FIELD if the field does not exist in the object
   */
  inline element_result<element> operator[](const char *s) const noexcept;

private:
  really_inline object() noexcept;
  really_inline object(const document *_doc, size_t _json_index) noexcept;
  friend class document::element;
  template<typename T>
  friend class document::element_result;
};

/**
 * Key/value pair in an object.
 */
class document::key_value_pair {
public:
  std::string_view key;
  document::element value;

private:
  really_inline key_value_pair(std::string_view _key, document::element _value) noexcept;
  friend class document::object;
};


/**
 * The result of a JSON navigation or conversion, or an error (if the navigation or conversion
 * failed). Allows the user to pick whether to use exceptions or not.
 *
 * Use like this to avoid exceptions:
 *
 *     auto [str, error] = document::parse(json).root().as_string();
 *     if (error) { exit(1); }
 *     cout << str;
 *
 * Use like this if you'd prefer to use exceptions:
 *
 *     string str = document::parse(json).root();
 *     cout << str;
 *
 */
template<typename T>
class document::element_result {
public:
  /** The value */
  T value;
  /** The error code (or 0 if there is no error) */
  error_code error;

  inline operator T() const noexcept(false);

private:
  really_inline element_result(T value) noexcept;
  really_inline element_result(error_code _error) noexcept;
  friend class document;
  friend class element;
};

// Add exception-throwing navigation / conversion methods to element_result<element>
template<>
class document::element_result<document::element> {
public:
  /** The value */
  element value;
  /** The error code (or 0 if there is no error) */
  error_code error;

  /** Whether this is a JSON `null` */
  inline element_result<bool> is_null() const noexcept;
  inline element_result<bool> as_bool() const noexcept;
  inline element_result<std::string_view> as_string() const noexcept;
  inline element_result<const char *> as_c_str() const noexcept;
  inline element_result<uint64_t> as_uint64_t() const noexcept;
  inline element_result<int64_t> as_int64_t() const noexcept;
  inline element_result<double> as_double() const noexcept;
  inline element_result<array> as_array() const noexcept;
  inline element_result<object> as_object() const noexcept;

  inline operator bool() const noexcept(false);
  inline explicit operator const char*() const noexcept(false);
  inline operator std::string_view() const noexcept(false);
  inline operator uint64_t() const noexcept(false);
  inline operator int64_t() const noexcept(false);
  inline operator double() const noexcept(false);
  inline operator array() const noexcept(false);
  inline operator object() const noexcept(false);

  inline element_result<element> operator[](const std::string_view &s) const noexcept;
  inline element_result<element> operator[](const char *s) const noexcept;

private:
  really_inline element_result(element value) noexcept;
  really_inline element_result(error_code _error) noexcept;
  friend class document;
  friend class element;
};

// Add exception-throwing navigation methods to element_result<array>
template<>
class document::element_result<document::array> {
public:
  /** The value */
  array value;
  /** The error code (or 0 if there is no error) */
  error_code error;

  inline operator array() const noexcept(false);

  inline array::iterator begin() const noexcept(false);
  inline array::iterator end() const noexcept(false);

private:
  really_inline element_result(array value) noexcept;
  really_inline element_result(error_code _error) noexcept;
  friend class document;
  friend class element;
};

// Add exception-throwing navigation methods to element_result<object>
template<>
class document::element_result<document::object> {
public:
  /** The value */
  object value;
  /** The error code (or 0 if there is no error) */
  error_code error;

  inline operator object() const noexcept(false);

  inline object::iterator begin() const noexcept(false);
  inline object::iterator end() const noexcept(false);

  inline element_result<element> operator[](const std::string_view &s) const noexcept;
  inline element_result<element> operator[](const char *s) const noexcept;

private:
  really_inline element_result(object value) noexcept;
  really_inline element_result(error_code _error) noexcept;
  friend class document;
  friend class element;
};

/**
  * A persistent document parser.
  *
  * Use this if you intend to parse more than one document. It holds the internal memory necessary
  * to do parsing, as well as memory for a single document that is overwritten on each parse.
  *
  * This class cannot be copied, only moved, to avoid unintended allocations.
  *
  * @note This is not thread safe: one parser cannot produce two documents at the same time!
  */
class document::parser {
public:
  /**
  * Create a JSON parser with zero capacity. Call allocate_capacity() to initialize it.
  */
  parser()=default;
  ~parser()=default;

  /**
   * Take another parser's buffers and state.
   *
   * @param other The parser to take. Its capacity is zeroed.
   */
  parser(document::parser &&other) = default;
  parser(const document::parser &) = delete; // Disallow copying
  /**
   * Take another parser's buffers and state.
   *
   * @param other The parser to take. Its capacity is zeroed.
   */
  parser &operator=(document::parser &&other) = default;
  parser &operator=(const document::parser &) = delete; // Disallow copying

  /**
   * Parse a JSON document and return a reference to it.
   *
   * The JSON document still lives in the parser: this is the most efficient way to parse JSON
   * documents because it reuses the same buffers, but you *must* use the document before you
   * destroy the parser or call parse() again.
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated. If realloc_if_needed is true,
   * it is assumed that the buffer does *not* have enough padding, and it is reallocated, enlarged
   * and copied before parsing.
   *
   * @param buf The JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes, unless
   *            realloc_if_needed is true.
   * @param len The length of the JSON.
   * @param realloc_if_needed Whether to reallocate and enlarge the JSON buffer to add padding.
   * @return the document, or an error if the JSON is invalid.
   */
  inline doc_ref_result parse(const uint8_t *buf, size_t len, bool realloc_if_needed = true) noexcept;

  /**
   * Parse a JSON document and return a reference to it.
   *
   * The JSON document still lives in the parser: this is the most efficient way to parse JSON
   * documents because it reuses the same buffers, but you *must* use the document before you
   * destroy the parser or call parse() again.
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated. If realloc_if_needed is true,
   * it is assumed that the buffer does *not* have enough padding, and it is reallocated, enlarged
   * and copied before parsing.
   *
   * @param buf The JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes, unless
   *            realloc_if_needed is true.
   * @param len The length of the JSON.
   * @param realloc_if_needed Whether to reallocate and enlarge the JSON buffer to add padding.
   * @return the document, or an error if the JSON is invalid.
   */
  really_inline doc_ref_result parse(const char *buf, size_t len, bool realloc_if_needed = true) noexcept;

  /**
   * Parse a JSON document and return a reference to it.
   *
   * The JSON document still lives in the parser: this is the most efficient way to parse JSON
   * documents because it reuses the same buffers, but you *must* use the document before you
   * destroy the parser or call parse() again.
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated. If `str.capacity() - str.size()
   * < SIMDJSON_PADDING`, the string will be copied to a string with larger capacity before parsing.
   *
   * @param s The JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes, or
   *          a new string will be created with the extra padding.
   * @return the document, or an error if the JSON is invalid.
   */
  really_inline doc_ref_result parse(const std::string &s) noexcept;

  /**
   * Parse a JSON document and return a reference to it.
   *
   * The JSON document still lives in the parser: this is the most efficient way to parse JSON
   * documents because it reuses the same buffers, but you *must* use the document before you
   * destroy the parser or call parse() again.
   *
   * @param s The JSON to parse.
   * @return the document, or an error if the JSON is invalid.
   */
  really_inline doc_ref_result parse(const padded_string &s) noexcept;

  // We do not want to allow implicit conversion from C string to std::string.
  really_inline doc_ref_result parse(const char *buf) noexcept = delete;

  /**
   * Parse a buffer containing many JSON documents.
   *
   *   document::parser parser;
   *   for (const document &doc : parser.parse_many(buf, len)) {
   *     cout << std::string(doc["title"]) << endl;
   *   }
   *
   * ### Format
   *
   * The buffer must contain a series of one or more JSON documents, concatenated into a single
   * buffer, separated by whitespace. It effectively parses until it has a fully valid document,
   * then starts parsing the next document at that point. (It does this with more parallelism and
   * lookahead than you might think, though.)
   *
   * documents that consist of an object or array may omit the whitespace between them, concatenating
   * with no separator. documents that consist of a single primitive (i.e. documents that are not
   * arrays or objects) MUST be separated with whitespace.
   *
   * ### Error Handling
   *
   * All errors are returned during iteration: if there is a global error such as memory allocation,
   * it will be yielded as the first result. Iteration always stops after the first error.
   *
   * As with all other simdjson methods, non-exception error handling is readily available through
   * the same interface, requiring you to check the error before using the document:
   *
   *   document::parser parser;
   *   for (auto [doc, error] : parser.parse_many(buf, len)) {
   *     if (error) { cerr << error << endl; exit(1); }
   *     cout << std::string(doc["title"]) << endl;
   *   }
   *
   * ### REQUIRED: Buffer Padding
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated.
   *
   * ### Threads
   *
   * When compiled with SIMDJSON_THREADS_ENABLED, this method will use a single thread under the
   * hood to do some lookahead.
   *
   * ### Parser Capacity
   *
   * If the parser is unallocated, it will be auto-allocated to batch_size. If it is already
   * allocated, it must have a capacity at least as large as batch_size.
   *
   * @param buf The concatenated JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes.
   * @param len The length of the concatenated JSON.
   * @param batch_size The batch size to use. MUST be larger than the largest document. The sweet
   *                   spot is cache-related: small enough to fit in cache, yet big enough to
   *                   parse as many documents as possible in one tight loop.
   *                   Defaults to 10MB, which has been a reasonable sweet spot in our tests.
   * @return The stream. If there is an error, it will be returned during iteration. An empty input
   *         will yield 0 documents rather than an EMPTY error. Errors:
   *         - MEMALLOC if the parser is unallocated and memory allocation fails
   *         - CAPACITY if the parser already has a capacity, and it is less than batch_size
   *         - other json errors if parsing fails.
   */
  inline stream parse_many(const uint8_t *buf, size_t len, size_t batch_size = 1000000) noexcept;

  /**
   * Parse a buffer containing many JSON documents.
   *
   *   document::parser parser;
   *   for (const document &doc : parser.parse_many(buf, len)) {
   *     cout << std::string(doc["title"]) << endl;
   *   }
   *
   * ### Format
   *
   * The buffer must contain a series of one or more JSON documents, concatenated into a single
   * buffer, separated by whitespace. It effectively parses until it has a fully valid document,
   * then starts parsing the next document at that point. (It does this with more parallelism and
   * lookahead than you might think, though.)
   *
   * documents that consist of an object or array may omit the whitespace between them, concatenating
   * with no separator. documents that consist of a single primitive (i.e. documents that are not
   * arrays or objects) MUST be separated with whitespace.
   *
   * ### Error Handling
   *
   * All errors are returned during iteration: if there is a global error such as memory allocation,
   * it will be yielded as the first result. Iteration always stops after the first error.
   *
   * As with all other simdjson methods, non-exception error handling is readily available through
   * the same interface, requiring you to check the error before using the document:
   *
   *   document::parser parser;
   *   for (auto [doc, error] : parser.parse_many(buf, len)) {
   *     if (error) { cerr << error << endl; exit(1); }
   *     cout << std::string(doc["title"]) << endl;
   *   }
   *
   * ### REQUIRED: Buffer Padding
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated.
   *
   * ### Threads
   *
   * When compiled with SIMDJSON_THREADS_ENABLED, this method will use a single thread under the
   * hood to do some lookahead.
   *
   * ### Parser Capacity
   *
   * If the parser is unallocated, it will be auto-allocated to batch_size. If it is already
   * allocated, it must have a capacity at least as large as batch_size.
   *
   * @param buf The concatenated JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes.
   * @param len The length of the concatenated JSON.
   * @param batch_size The batch size to use. MUST be larger than the largest document. The sweet
   *                   spot is cache-related: small enough to fit in cache, yet big enough to
   *                   parse as many documents as possible in one tight loop.
   *                   Defaults to 10MB, which has been a reasonable sweet spot in our tests.
   * @return The stream. If there is an error, it will be returned during iteration. An empty input
   *         will yield 0 documents rather than an EMPTY error. Errors:
   *         - MEMALLOC if the parser is unallocated and memory allocation fails
   *         - CAPACITY if the parser already has a capacity, and it is less than batch_size
   *         - other json errors if parsing fails
   */
  inline stream parse_many(const char *buf, size_t len, size_t batch_size = 1000000) noexcept;

  /**
   * Parse a buffer containing many JSON documents.
   *
   *   document::parser parser;
   *   for (const document &doc : parser.parse_many(buf, len)) {
   *     cout << std::string(doc["title"]) << endl;
   *   }
   *
   * ### Format
   *
   * The buffer must contain a series of one or more JSON documents, concatenated into a single
   * buffer, separated by whitespace. It effectively parses until it has a fully valid document,
   * then starts parsing the next document at that point. (It does this with more parallelism and
   * lookahead than you might think, though.)
   *
   * documents that consist of an object or array may omit the whitespace between them, concatenating
   * with no separator. documents that consist of a single primitive (i.e. documents that are not
   * arrays or objects) MUST be separated with whitespace.
   *
   * ### Error Handling
   *
   * All errors are returned during iteration: if there is a global error such as memory allocation,
   * it will be yielded as the first result. Iteration always stops after the first error.
   *
   * As with all other simdjson methods, non-exception error handling is readily available through
   * the same interface, requiring you to check the error before using the document:
   *
   *   document::parser parser;
   *   for (auto [doc, error] : parser.parse_many(buf, len)) {
   *     if (error) { cerr << error << endl; exit(1); }
   *     cout << std::string(doc["title"]) << endl;
   *   }
   *
   * ### REQUIRED: Buffer Padding
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated.
   *
   * ### Threads
   *
   * When compiled with SIMDJSON_THREADS_ENABLED, this method will use a single thread under the
   * hood to do some lookahead.
   *
   * ### Parser Capacity
   *
   * If the parser is unallocated, it will be auto-allocated to batch_size. If it is already
   * allocated, it must have a capacity at least as large as batch_size.
   *
   * @param s The concatenated JSON to parse. Must have at least len + SIMDJSON_PADDING allocated bytes.
   * @param batch_size The batch size to use. MUST be larger than the largest document. The sweet
   *                   spot is cache-related: small enough to fit in cache, yet big enough to
   *                   parse as many documents as possible in one tight loop.
   *                   Defaults to 10MB, which has been a reasonable sweet spot in our tests.
   * @return he stream. If there is an error, it will be returned during iteration. An empty input
   *         will yield 0 documents rather than an EMPTY error. Errors:
   *         - MEMALLOC if the parser is unallocated and memory allocation fails
   *         - CAPACITY if the parser already has a capacity, and it is less than batch_size
   *         - other json errors if parsing fails
   */
  inline stream parse_many(const std::string &s, size_t batch_size = 1000000) noexcept;

  /**
   * Parse a buffer containing many JSON documents.
   *
   *   document::parser parser;
   *   for (const document &doc : parser.parse_many(buf, len)) {
   *     cout << std::string(doc["title"]) << endl;
   *   }
   *
   * ### Format
   *
   * The buffer must contain a series of one or more JSON documents, concatenated into a single
   * buffer, separated by whitespace. It effectively parses until it has a fully valid document,
   * then starts parsing the next document at that point. (It does this with more parallelism and
   * lookahead than you might think, though.)
   *
   * documents that consist of an object or array may omit the whitespace between them, concatenating
   * with no separator. documents that consist of a single primitive (i.e. documents that are not
   * arrays or objects) MUST be separated with whitespace.
   *
   * ### Error Handling
   *
   * All errors are returned during iteration: if there is a global error such as memory allocation,
   * it will be yielded as the first result. Iteration always stops after the first error.
   *
   * As with all other simdjson methods, non-exception error handling is readily available through
   * the same interface, requiring you to check the error before using the document:
   *
   *   document::parser parser;
   *   for (auto [doc, error] : parser.parse_many(buf, len)) {
   *     if (error) { cerr << error << endl; exit(1); }
   *     cout << std::string(doc["title"]) << endl;
   *   }
   *
   * ### REQUIRED: Buffer Padding
   *
   * The buffer must have at least SIMDJSON_PADDING extra allocated bytes. It does not matter what
   * those bytes are initialized to, as long as they are allocated.
   *
   * ### Threads
   *
   * When compiled with SIMDJSON_THREADS_ENABLED, this method will use a single thread under the
   * hood to do some lookahead.
   *
   * ### Parser Capacity
   *
   * If the parser is unallocated, it will be auto-allocated to batch_size. If it is already
   * allocated, it must have a capacity at least as large as batch_size.
   *
   * @param s The concatenated JSON to parse.
   * @param batch_size The batch size to use. MUST be larger than the largest document. The sweet
   *                   spot is cache-related: small enough to fit in cache, yet big enough to
   *                   parse as many documents as possible in one tight loop.
   *                   Defaults to 10MB, which has been a reasonable sweet spot in our tests.
   * @return he stream. If there is an error, it will be returned during iteration. An empty input
   *         will yield 0 documents rather than an EMPTY error. Errors:
   *         - MEMALLOC if the parser is unallocated and memory allocation fails
   *         - CAPACITY if the parser already has a capacity, and it is less than batch_size
   *         - other json errors if parsing fails
   */
  inline stream parse_many(const padded_string &s, size_t batch_size = 1000000) noexcept;

  // We do not want to allow implicit conversion from C string to std::string.
  really_inline doc_ref_result parse_many(const char *buf, size_t batch_size = 1000000) noexcept = delete;

  /**
   * Current capacity: the largest document this parser can support without reallocating.
   */
  really_inline size_t capacity() const noexcept;

  /**
   * The maximum level of nested object and arrays supported by this parser.
   */
  really_inline size_t max_depth() const noexcept;

  /**
   * Ensure this parser has enough memory to process JSON documents up to `capacity` bytes in length
   * and `max_depth` depth.
   */
  WARN_UNUSED inline bool allocate_capacity(size_t capacity, size_t max_depth = DEFAULT_MAX_DEPTH);

  // type aliases for backcompat
  using Iterator = document::iterator;
  using InvalidJSON = simdjson_error;

  // Next location to write to in the tape
  uint32_t current_loc{0};

  // structural indices passed from stage 1 to stage 2
  uint32_t n_structural_indexes{0};
  std::unique_ptr<uint32_t[]> structural_indexes;

  // location and return address of each open { or [
  std::unique_ptr<uint32_t[]> containing_scope_offset;
#ifdef SIMDJSON_USE_COMPUTED_GOTO
  std::unique_ptr<void*[]> ret_address;
#else
  std::unique_ptr<char[]> ret_address;
#endif

  // Next place to write a string
  uint8_t *current_string_buf_loc;

  bool valid{false};
  error_code error{UNINITIALIZED};

  // Document we're writing to
  document doc;

  //
  // TODO these are deprecated; use the results of parse instead.
  //

  // returns true if the document parsed was valid
  inline bool is_valid() const noexcept;

  // return an error code corresponding to the last parsing attempt, see
  // simdjson.h will return UNITIALIZED if no parsing was attempted
  inline int get_error_code() const noexcept;

  // return the string equivalent of "get_error_code"
  inline std::string get_error_message() const noexcept;

  // print the json to std::ostream (should be valid)
  // return false if the tape is likely wrong (e.g., you did not parse a valid
  // JSON).
  inline bool print_json(std::ostream &os) const noexcept;
  inline bool dump_raw_tape(std::ostream &os) const noexcept;

  //
  // Parser callbacks: these are internal!
  //
  // TODO find a way to do this without exposing the interface or crippling performance
  //

  // this should be called when parsing (right before writing the tapes)
  inline void init_stage2() noexcept;
  really_inline error_code on_error(error_code new_error_code) noexcept;
  really_inline error_code on_success(error_code success_code) noexcept;
  really_inline bool on_start_document(uint32_t depth) noexcept;
  really_inline bool on_start_object(uint32_t depth) noexcept;
  really_inline bool on_start_array(uint32_t depth) noexcept;
  // TODO we're not checking this bool
  really_inline bool on_end_document(uint32_t depth) noexcept;
  really_inline bool on_end_object(uint32_t depth) noexcept;
  really_inline bool on_end_array(uint32_t depth) noexcept;
  really_inline bool on_true_atom() noexcept;
  really_inline bool on_false_atom() noexcept;
  really_inline bool on_null_atom() noexcept;
  really_inline uint8_t *on_start_string() noexcept;
  really_inline bool on_end_string(uint8_t *dst) noexcept;
  really_inline bool on_number_s64(int64_t value) noexcept;
  really_inline bool on_number_u64(uint64_t value) noexcept;
  really_inline bool on_number_double(double value) noexcept;
  //
  // Called before a parse is initiated.
  //
  // - Returns CAPACITY if the document is too large
  // - Returns MEMALLOC if we needed to allocate memory and could not
  //
  WARN_UNUSED inline error_code init_parse(size_t len) noexcept;

private:
  //
  // The maximum document length this parser supports.
  //
  // Buffers are large enough to handle any document up to this length.
  //
  size_t _capacity{0};

  //
  // The maximum depth (number of nested objects and arrays) supported by this parser.
  //
  // Defaults to DEFAULT_MAX_DEPTH.
  //
  size_t _max_depth{0};

  // all nodes are stored on the doc.tape using a 64-bit word.
  //
  // strings, double and ints are stored as
  //  a 64-bit word with a pointer to the actual value
  //
  //
  //
  // for objects or arrays, store [ or {  at the beginning and } and ] at the
  // end. For the openings ([ or {), we annotate them with a reference to the
  // location on the doc.tape of the end, and for then closings (} and ]), we
  // annotate them with a reference to the location of the opening
  //
  //

  inline void write_tape(uint64_t val, tape_type t) noexcept;
  inline void annotate_previous_loc(uint32_t saved_loc, uint64_t val) noexcept;

  //
  // Set the current capacity: the largest document this parser can support without reallocating.
  //
  // This will allocate *or deallocate* as necessary.
  //
  // Returns false if allocation fails.
  //
  inline WARN_UNUSED bool set_capacity(size_t capacity);

  //
  // Set the maximum level of nested object and arrays supported by this parser.
  //
  // This will allocate *or deallocate* as necessary.
  //
  // Returns false if allocation fails.
  //
  inline WARN_UNUSED bool set_max_depth(size_t max_depth);

  // Used internally to get the document
  inline const document &get_document() const noexcept(false);

  template<size_t max_depth> friend class document_iterator;
}; // class parser

} // namespace simdjson

#endif // SIMDJSON_DOCUMENT_H