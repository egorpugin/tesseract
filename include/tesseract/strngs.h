/**********************************************************************
 * File:        strngs.h  (Formerly strings.h)
 * Description: STRING class definition.
 * Author:      Ray Smith
 *
 * (C) Copyright 1991, Hewlett-Packard Ltd.
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 ** http://www.apache.org/licenses/LICENSE-2.0
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 *
 **********************************************************************/

#ifndef STRNGS_H
#define STRNGS_H

#include <cassert>  // for assert
#include <cstdint>  // for uint32_t
#include <cstdio>   // for FILE
#include <cstring>  // for strncpy
#include <string>

#include "platform.h"  // for TESS_API

namespace tesseract {
class TFile;

template <typename T>
class GenericVector;
}  // namespace tesseract.

class STRING : public std::string {
public:
    using std::string::string;

    inline char* strdup() const {
        int32_t len = length() + 1;
        return strncpy(new char[len], c_str(), len);
    }

    void split(char c, tesseract::GenericVector<STRING>* splited);

    // Appends the given string and int (as a %d) to this.
    // += cannot be used for ints as there as a char += operator that would
    // be ambiguous, and ints usually need a string before or between them
    // anyway.
    void add_str_int(const char* str, int number);
    // Appends the given string and double (as a %.8g) to this.
    void add_str_double(const char* str, double number);
};

class TESS_API STRING_old {
 public:
  STRING_old();
  STRING_old(const STRING_old& string);
  STRING_old(const char* string);
  STRING_old(const char* data, int length);
  ~STRING_old();

  // Writes to the given file. Returns false in case of error.
  bool Serialize(FILE* fp) const;
  // Reads from the given file. Returns false in case of error.
  // If swap is true, assumes a big/little-endian swap is needed.
  bool DeSerialize(bool swap, FILE* fp);
  // Writes to the given file. Returns false in case of error.
  bool Serialize(tesseract::TFile* fp) const;
  // Reads from the given file. Returns false in case of error.
  // If swap is true, assumes a big/little-endian swap is needed.
  bool DeSerialize(tesseract::TFile* fp);
  // As DeSerialize, but only seeks past the data - hence a static method.
  static bool SkipDeSerialize(tesseract::TFile* fp);

  bool contains(char c) const;
  int32_t length() const;
  int32_t size() const {
    return length();
  }
  // Workaround to avoid g++ -Wsign-compare warnings.
  uint32_t unsigned_size() const {
    const int32_t len = length();
    assert(0 <= len);
    return static_cast<uint32_t>(len);
  }
  const char* c_str() const;

  inline char* strdup() const {
    int32_t len = length() + 1;
    return strncpy(new char[len], GetCStr(), len);
  }

  char& operator[](int32_t index) const;
  //void split(char c, GenericVector_old<STRING_old>* splited);
  void truncate_at(int32_t index);

  bool operator==(const STRING_old& string) const;
  bool operator!=(const STRING_old& string) const;
  bool operator!=(const char* string) const;

  STRING_old& operator=(const char* string);
  STRING_old& operator=(const STRING_old& string);

  STRING_old operator+(const STRING_old& string) const;
  STRING_old operator+(char ch) const;

  STRING_old& operator+=(const char* string);
  STRING_old& operator+=(const STRING_old& string);
  STRING_old& operator+=(char ch);

  // Assignment for strings which are not null-terminated.
  void assign(const char* cstr, int len);

  // Appends the given string and int (as a %d) to this.
  // += cannot be used for ints as there as a char += operator that would
  // be ambiguous, and ints usually need a string before or between them
  // anyway.
  void add_str_int(const char* str, int number);
  // Appends the given string and double (as a %.8g) to this.
  void add_str_double(const char* str, double number);

  // ensure capacity but keep pointer encapsulated
  inline void ensure(int32_t min_capacity) {
    ensure_cstr(min_capacity);
  }

 private:
  typedef struct STRING_HEADER {
    // How much space was allocated in the string buffer for char data.
    int capacity_;

    // used_ is how much of the capacity is currently being used,
    // including a '\0' terminator.
    //
    // If used_ is 0 then string is nullptr (not even the '\0')
    // else if used_ > 0 then it is strlen() + 1 (because it includes '\0')
    // else strlen is >= 0 (not nullptr) but needs to be computed.
    //      this condition is set when encapsulation is violated because
    //      an API returned a mutable string.
    //
    // capacity_ - used_ = excess capacity that the string can grow
    //                     without reallocating
    mutable int used_;
  } STRING_HEADER;

  // To preserve the behavior of the old serialization, we only have space
  // for one pointer in this structure. So we are embedding a data structure
  // at the start of the storage that will hold additional state variables,
  // then storing the actual string contents immediately after.
  STRING_HEADER* data_;

  // returns the header part of the storage
  inline STRING_HEADER* GetHeader() {
    return data_;
  }
  inline const STRING_HEADER* GetHeader() const {
    return data_;
  }

  // returns the string data part of storage
  inline char* GetCStr() {
    return (reinterpret_cast<char*>(data_)) + sizeof(STRING_HEADER);
  }

  inline const char* GetCStr() const {
    return (reinterpret_cast<const char*>(data_)) + sizeof(STRING_HEADER);
  }
  inline bool InvariantOk() const {
#if STRING_IS_PROTECTED
    return (GetHeader()->used_ == 0)
               ? (c_str() == nullptr)
               : (GetHeader()->used_ == (strlen(c_str()) + 1));
#else
    return true;
#endif
  }

  // Ensure string has requested capacity as optimization
  // to avoid unnecessary reallocations.
  // The return value is a cstr buffer with at least requested capacity
  char* ensure_cstr(int32_t min_capacity);

  void FixHeader() const;  // make used_ non-negative, even if const

  char* AllocData(int used, int capacity);
  void DiscardData();
};

#endif
