/**********************************************************************
 * File:        strngs.cpp  (Formerly strings.c)
 * Description: STRING class functions.
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

#include <tesseract/strngs.h>

#include "errcode.h"        // for ASSERT_HOST
#include <tesseract/genericvector.h>  // for GenericVector
#include <tesseract/helpers.h>        // for ReverseN
#include <tesseract/serialis.h>       // for TFile

#include <cassert>          // for assert
#include <cstdlib>          // for malloc, free
#include <locale>           // for std::locale::classic
#include <sstream>          // for std::stringstream

using tesseract::TFile;

// Size of buffer needed to host the decimal representation of the maximum
// possible length of an int (in 64 bits), being -<20 digits>.
const int kMaxIntSize = 22;

/**********************************************************************
 * STRING_HEADER provides metadata about the allocated buffer,
 * including total capacity and how much used (strlen with '\0').
 *
 * The implementation hides this header at the start of the data
 * buffer and appends the string on the end to keep sizeof(STRING)
 * unchanged from earlier versions so serialization is not affected.
 *
 * The collection of MACROS provide different implementations depending
 * on whether the string keeps track of its strlen or not so that this
 * feature can be added in later when consumers don't modify the string
 **********************************************************************/

// Smallest string to allocate by default
const int kMinCapacity = 16;

char* STRING_old::AllocData(int used, int capacity) {
  data_ = static_cast<STRING_HEADER *>(malloc(capacity + sizeof(STRING_HEADER)));

  // header is the metadata for this memory block
  STRING_HEADER* header = GetHeader();
  header->capacity_ = capacity;
  header->used_ = used;
  return GetCStr();
}

void STRING_old::DiscardData() {
  free(data_);
  data_ = nullptr;
}

// This is a private method; ensure FixHeader is called (or used_ is well defined)
// beforehand
char* STRING_old::ensure_cstr(int32_t min_capacity) {
  STRING_HEADER* orig_header = GetHeader();
  if (min_capacity <= orig_header->capacity_)
    return (reinterpret_cast<char *>(this->data_)) + sizeof(STRING_HEADER);

  // if we are going to grow bigger, than double our existing
  // size, but if that still is not big enough then keep the
  // requested capacity
  if (min_capacity < 2 * orig_header->capacity_)
    min_capacity = 2 * orig_header->capacity_;

  int alloc = sizeof(STRING_HEADER) + min_capacity;
  auto* new_header = static_cast<STRING_HEADER*>(malloc(alloc));

  memcpy(&new_header[1], GetCStr(), orig_header->used_);
  new_header->capacity_ = min_capacity;
  new_header->used_ = orig_header->used_;

  // free old memory, then rebind to new memory
  DiscardData();
  data_ = new_header;

  assert(InvariantOk());
  return (reinterpret_cast<char *>(data_)) + sizeof(STRING_HEADER);
}

// This is const, but is modifying a mutable field
// this way it can be used on const or non-const instances.
void STRING_old::FixHeader() const {
  const STRING_HEADER* header = GetHeader();
  if (header->used_ < 0)
    header->used_ = strlen(GetCStr()) + 1;
}


STRING_old::STRING_old() {
  // Empty STRINGs contain just the "\0".
  memcpy(AllocData(1, kMinCapacity), "", 1);
}

STRING_old::STRING_old(const STRING_old& str) {
  str.FixHeader();
  const STRING_HEADER* str_header  = str.GetHeader();
  const int str_used  = str_header->used_;
  char *this_cstr = AllocData(str_used, str_used);
  memcpy(this_cstr, str.GetCStr(), str_used);
  assert(InvariantOk());
}

STRING_old::STRING_old(const char* cstr) {
  if (cstr == nullptr) {
    // Empty STRINGs contain just the "\0".
    memcpy(AllocData(1, kMinCapacity), "", 1);
  } else {
    const int len = strlen(cstr) + 1;
    char* this_cstr = AllocData(len, len);
    memcpy(this_cstr, cstr, len);
  }
  assert(InvariantOk());
}

STRING_old::STRING_old(const char *data, int length) {
  if (data == nullptr) {
    // Empty STRINGs contain just the "\0".
    memcpy(AllocData(1, kMinCapacity), "", 1);
  } else {
    char* this_cstr = AllocData(length + 1, length + 1);
    memcpy(this_cstr, data, length);
    this_cstr[length] = '\0';
  }
}

STRING_old::~STRING_old() {
  DiscardData();
}

// TODO(rays) Change all callers to use TFile and remove the old functions.
// Writes to the given file. Returns false in case of error.
bool STRING_old::Serialize(FILE* fp) const {
  uint32_t len = length();
  return tesseract::Serialize(fp, &len) &&
         tesseract::Serialize(fp, GetCStr(), len);
}
// Writes to the given file. Returns false in case of error.
bool STRING_old::Serialize(TFile* fp) const {
  uint32_t len = length();
  return fp->Serialize(&len) &&
         fp->Serialize(GetCStr(), len);
}
// Reads from the given file. Returns false in case of error.
// If swap is true, assumes a big/little-endian swap is needed.
bool STRING_old::DeSerialize(bool swap, FILE* fp) {
  uint32_t len;
  if (!tesseract::DeSerialize(fp, &len)) return false;
  if (swap)
    ReverseN(&len, sizeof(len));
  // Arbitrarily limit the number of characters to protect against bad data.
  if (len > UINT16_MAX) return false;
  truncate_at(len);
  return tesseract::DeSerialize(fp, GetCStr(), len);
}
// Reads from the given file. Returns false in case of error.
// If swap is true, assumes a big/little-endian swap is needed.
bool STRING_old::DeSerialize(TFile* fp) {
  uint32_t len;
  if (!fp->DeSerialize(&len)) return false;
  truncate_at(len);
  return fp->DeSerialize(GetCStr(), len);
}

// As DeSerialize, but only seeks past the data - hence a static method.
bool STRING_old::SkipDeSerialize(TFile* fp) {
  uint32_t len;
  if (!fp->DeSerialize(&len)) return false;
  return fp->Skip(len);
}

bool STRING_old::contains(const char c) const {
  return (c != '\0') && (strchr (GetCStr(), c) != nullptr);
}

int32_t STRING_old::length() const {
  FixHeader();
  return GetHeader()->used_ - 1;
}

const char* STRING_old::c_str() const {
  const STRING_HEADER* header = GetHeader();
  if (!header || header->used_ == 0)
    return nullptr;

  // mark header length unreliable because tesseract might
  // cast away the const and mutate the string directly.
  header->used_ = -1;
  return GetCStr();
}

void STRING_old::truncate_at(int32_t index) {
  ASSERT_HOST(index >= 0);
  FixHeader();
  char* this_cstr = ensure_cstr(index + 1);
  this_cstr[index] = '\0';
  GetHeader()->used_ = index + 1;
  assert(InvariantOk());
}

char& STRING_old::operator[](int32_t index) const {
  // Code is casting away this const and mutating the string,
  // so mark used_ as -1 to flag it unreliable.
  GetHeader()->used_ = -1;
  return (const_cast<char *>(GetCStr()))[index];
}

void STRING::split(const char c, tesseract::GenericVector<STRING> *splited) {
    int start_index = 0;
    const int len = length();
    for (int i = 0; i < len; i++) {
        if ((*this)[i] == c) {
            if (i != start_index) {
                (*this)[i] = '\0';
                splited->push_back(STRING(c_str() + start_index, i - start_index));
                (*this)[i] = c;
            }
            start_index = i + 1;
        }
    }

    if (len != start_index) {
        splited->push_back(STRING(c_str() + start_index, len - start_index));
    }
}

/*void STRING_old::split(const char c, GenericVector<STRING_old> *splited) {
  int start_index = 0;
  const int len = length();
  for (int i = 0; i < len; i++) {
    if ((*this)[i] == c) {
      if (i != start_index) {
        (*this)[i] = '\0';
        splited->push_back(STRING_old(GetCStr() + start_index, i - start_index));
        (*this)[i] = c;
      }
      start_index = i + 1;
    }
  }

  if (len != start_index) {
    splited->push_back(STRING_old(GetCStr() + start_index, len - start_index));
  }
}*/

bool STRING_old::operator==(const STRING_old& str) const {
  FixHeader();
  str.FixHeader();
  const STRING_HEADER* str_header = str.GetHeader();
  const STRING_HEADER* this_header = GetHeader();
  const int this_used = this_header->used_;
  const int str_used  = str_header->used_;

  return (this_used == str_used)
          && (memcmp(GetCStr(), str.GetCStr(), this_used) == 0);
}

bool STRING_old::operator!=(const STRING_old& str) const {
  FixHeader();
  str.FixHeader();
  const STRING_HEADER* str_header = str.GetHeader();
  const STRING_HEADER* this_header = GetHeader();
  const int this_used = this_header->used_;
  const int str_used  = str_header->used_;

  return (this_used != str_used)
         || (memcmp(GetCStr(), str.GetCStr(), this_used) != 0);
}

bool STRING_old::operator!=(const char* cstr) const {
  FixHeader();
  const STRING_HEADER* this_header = GetHeader();

  if (cstr == nullptr)
    return this_header->used_ > 1;  // either '\0' or nullptr
  else {
    const int32_t length = strlen(cstr) + 1;
    return (this_header->used_ != length)
            || (memcmp(GetCStr(), cstr, length) != 0);
  }
}

STRING_old& STRING_old::operator=(const STRING_old& str) {
  str.FixHeader();
  const STRING_HEADER* str_header = str.GetHeader();
  const int str_used = str_header->used_;

  GetHeader()->used_ = 0;  // clear since ensure doesn't need to copy data
  char* this_cstr = ensure_cstr(str_used);
  STRING_HEADER* this_header = GetHeader();

  memcpy(this_cstr, str.GetCStr(), str_used);
  this_header->used_ = str_used;

  assert(InvariantOk());
  return *this;
}

STRING_old & STRING_old::operator+=(const STRING_old& str) {
  FixHeader();
  str.FixHeader();
  const STRING_HEADER* str_header = str.GetHeader();
  const char* str_cstr = str.GetCStr();
  const int  str_used  = str_header->used_;
  const int  this_used = GetHeader()->used_;
  char* this_cstr = ensure_cstr(this_used + str_used);

  STRING_HEADER* this_header = GetHeader();  // after ensure for realloc

  if (this_used > 1) {
    memcpy(this_cstr + this_used - 1, str_cstr, str_used);
    this_header->used_ += str_used - 1;  // overwrite '\0'
  } else {
    memcpy(this_cstr, str_cstr, str_used);
    this_header->used_ = str_used;
  }

  assert(InvariantOk());
  return *this;
}

void STRING::add_str_int(const char* str, int number) {
  if (str != nullptr)
    *this += str;
  // Allow space for the maximum possible length of int64_t.
  char num_buffer[kMaxIntSize];
  snprintf(num_buffer, kMaxIntSize - 1, "%d", number);
  num_buffer[kMaxIntSize - 1] = '\0';
  *this += num_buffer;
}
// Appends the given string and double (as a %.8g) to this.
void STRING::add_str_double(const char* str, double number) {
  if (str != nullptr)
    *this += str;
  std::stringstream stream;
  // Use "C" locale (needed for double value).
  stream.imbue(std::locale::classic());
  // Use 8 digits for double value.
  stream.precision(8);
  stream << number;
  *this += stream.str().c_str();
}

STRING_old & STRING_old::operator=(const char* cstr) {
  STRING_HEADER* this_header = GetHeader();
  if (cstr) {
    const int len = strlen(cstr) + 1;

    this_header->used_ = 0;  // don't bother copying data if need to realloc
    char* this_cstr = ensure_cstr(len);
    this_header = GetHeader();  // for realloc
    memcpy(this_cstr, cstr, len);
    this_header->used_ = len;
  } else {
    // Reallocate to same state as default constructor.
    DiscardData();
    // Empty STRINGs contain just the "\0".
    memcpy(AllocData(1, kMinCapacity), "", 1);
  }

  assert(InvariantOk());
  return *this;
}

void STRING_old::assign(const char *cstr, int len) {
  STRING_HEADER* this_header = GetHeader();
  this_header->used_ = 0;  // don't bother copying data if need to realloc
  char* this_cstr = ensure_cstr(len + 1);  // +1 for '\0'

  this_header = GetHeader();  // for realloc
  memcpy(this_cstr, cstr, len);
  this_cstr[len] = '\0';
  this_header->used_ = len + 1;

  assert(InvariantOk());
}

STRING_old STRING_old::operator+(const STRING_old& str) const {
  STRING_old result(*this);
  result += str;

  assert(InvariantOk());
  return result;
}


STRING_old STRING_old::operator+(const char ch) const {
  STRING_old result;
  FixHeader();
  const STRING_HEADER* this_header = GetHeader();
  const int this_used = this_header->used_;
  char* result_cstr = result.ensure_cstr(this_used + 1);
  STRING_HEADER* result_header = result.GetHeader();
  const int result_used = result_header->used_;

  // copies '\0' but we'll overwrite that
  memcpy(result_cstr, GetCStr(), this_used);
  result_cstr[result_used] = ch;      // overwrite old '\0'
  result_cstr[result_used + 1] = '\0';  // append on '\0'
  ++result_header->used_;

  assert(InvariantOk());
  return result;
}


STRING_old&  STRING_old::operator+=(const char *str) {
  if (!str || !*str)  // empty string has no effect
    return *this;

  FixHeader();
  const int len = strlen(str) + 1;
  const int this_used = GetHeader()->used_;
  char* this_cstr = ensure_cstr(this_used + len);
  STRING_HEADER* this_header = GetHeader();  // after ensure for realloc

  // if we had non-empty string then append overwriting old '\0'
  // otherwise replace
  if (this_used > 0) {
    memcpy(this_cstr + this_used - 1, str, len);
    this_header->used_ += len - 1;
  } else {
    memcpy(this_cstr, str, len);
    this_header->used_ = len;
  }

  assert(InvariantOk());
  return *this;
}


STRING_old& STRING_old::operator+=(const char ch) {
  if (ch == '\0')
    return *this;

  FixHeader();
  int   this_used = GetHeader()->used_;
  char* this_cstr = ensure_cstr(this_used + 1);
  STRING_HEADER* this_header = GetHeader();

  if (this_used > 0)
    --this_used; // undo old empty null if there was one

  this_cstr[this_used++] = ch;   // append ch to end
  this_cstr[this_used++] = '\0'; // append '\0' after ch
  this_header->used_ = this_used;

  assert(InvariantOk());
  return *this;
}
