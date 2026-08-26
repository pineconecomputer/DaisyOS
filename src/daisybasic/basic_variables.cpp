/*
 * DaisyOS - main firmware for the Daisy computer.
 * Copyright (C) 2026 Joe Cassara
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "daisybasic/basic_internal.h"

// Looks up a variable by name, case-insensitively.
Variable* FindVariable(const char* name) {
  for (int i = 0; i < variableCount; i++) {
    if (strcasecmp(variables[i].name, name) == 0) {
      return &variables[i];
    }
  }
  return NULL;
}

// Returns the named variable, creating it if new. Reusing a name at a different
// type releases any string it held, so switching types does not leak.
Variable* CreateVariable(const char* name, VarType type) {
  Variable* existing = FindVariable(name);
  if (existing) {
    if (existing->type == VAR_STRING && type != VAR_STRING &&
        existing->strVal) {
      heapBytesUsed -= strlen(existing->strVal) + 1;
      free(existing->strVal);
      existing->strVal = NULL;
    }
    existing->type = type;
    return existing;
  }
  if (!EnsureCapacity((void**)&variables, &variableCapacity, variableCount + 1,
                      sizeof(Variable), 8, true)) {
    PrintError(ERR_OUT_OF_MEMORY);
    return NULL;
  }
  Variable* v = &variables[variableCount++];
  strncpy(v->name, name, MAX_VAR_NAME - 1);
  v->name[MAX_VAR_NAME - 1] = '\0';
  v->type = type;
  return v;
}

// Assigns a string, reallocating storage and tracking the heap total that
// FRE() reports. Values longer than the limit are silently truncated.
bool SetStringVar(Variable* v, const char* str) {
  if (!str) {
    str = "";
  }
  size_t srcLen = strlen(str);
  if (srcLen > MAX_STRING_VAR_LEN) {
    srcLen = MAX_STRING_VAR_LEN;
  }
  size_t newLen = srcLen + 1;
  if (v->strVal) {
    heapBytesUsed -= strlen(v->strVal) + 1;
    free(v->strVal);
    v->strVal = NULL;
  }
  v->strVal = (char*)malloc(newLen);
  if (!v->strVal) {
    PrintError(ERR_OUT_OF_MEMORY);
    return false;
  }
  heapBytesUsed += newLen;
  memcpy(v->strVal, str, srcLen);
  v->strVal[srcLen] = '\0';
  return true;
}

// True for a %-suffixed name, meaning a 16-bit integer array.
bool IsIntArrayVar(const char* name) {
  size_t len = strlen(name);
  return len > 1 && name[len - 1] == '%';
}

// True if the name is a built-in string function. Needed because these all end
// in $ and would otherwise be mistaken for string variables.
bool IsBuiltinStringFunction(const char* name) {
  return strcasecmp(name, "CHR$") == 0 || strcasecmp(name, "MID$") == 0 ||
         strcasecmp(name, "LEFT$") == 0 || strcasecmp(name, "RIGHT$") == 0 ||
         strcasecmp(name, "STR$") == 0 || strcasecmp(name, "HEX$") == 0 ||
         strcasecmp(name, "BIN$") == 0 || strcasecmp(name, "TOUPPER$") == 0 ||
         strcasecmp(name, "TOLOWER$") == 0 || strcasecmp(name, "CHOMP$") == 0 ||
         strcasecmp(name, "DATE$") == 0 || strcasecmp(name, "TIME$") == 0 ||
         strcasecmp(name, "WIFI$") == 0;
}

// True for a $-suffixed name that is not a built-in string function.
bool IsStringArrayVar(const char* name) {
  size_t len = strlen(name);
  if (len == 0 || name[len - 1] != '$') {
    return false;
  }
  return !IsBuiltinStringFunction(name);
}

// Bytes per element for an array type.
uint16_t GetElementSize(ArrayType type) {
  switch (type) {
    case ARRAY_TYPE_INT:
      return sizeof(int16_t);
    case ARRAY_TYPE_FLOAT:
      return sizeof(float);
    case ARRAY_TYPE_STRING:
      return STRING_ELEMENT_LEN;
    default:
      return 0;
  }
}

// Infers an array's element type from its name suffix; no suffix means float.
ArrayType GetArrayTypeFromName(const char* name) {
  size_t len = strlen(name);
  if (len > 0) {
    if (name[len - 1] == '%') {
      return ARRAY_TYPE_INT;
    }
    if (name[len - 1] == '$') {
      return ARRAY_TYPE_STRING;
    }
  }
  return ARRAY_TYPE_FLOAT;
}

// Looks up a dimensioned array by name. A scalar and an array may share a name
// and are kept in separate tables.
ArrayDescriptor* FindArray(const char* name) {
  for (int i = 0; i < arrayDescriptorCount; i++) {
    if (arrayDescriptors[i].isDimmed &&
        strcasecmp(arrayDescriptors[i].name, name) == 0) {
      return &arrayDescriptors[i];
    }
  }
  return NULL;
}

// Flattens subscripts to a linear index, row-major for two dimensions. Returns
// -1 for an out-of-range subscript and -2 when the subscript count does not
// match how the array was dimensioned, so the caller can report either.
int CalculateArrayIndex(ArrayDescriptor* arr, int idx1, int idx2,
                        bool has2ndIndex) {
  if (arr->dim2Size == 0) {
    if (has2ndIndex) {
      return -2;
    }
    if (idx1 < 0 || idx1 >= arr->dim1Size) {
      return -1;
    }
    return idx1;
  } else {
    if (!has2ndIndex) {
      return -2;
    }
    if (idx1 < 0 || idx1 >= arr->dim1Size || idx2 < 0 ||
        idx2 >= arr->dim2Size) {
      return -1;
    }
    return idx1 * arr->dim2Size + idx2;
  }
}

// Address of an element, given its linear index.
void* GetArrayElementPtr(ArrayDescriptor* arr, int linearIndex) {
  return arr->data + linearIndex * GetElementSize(arr->type);
}

// Allocates and zeroes an array's storage, so elements read as 0 or "" before
// they are assigned.
bool AllocateArray(ArrayDescriptor* desc, uint16_t dim1, uint16_t dim2) {
  uint32_t totalElements = (dim2 == 0) ? dim1 : (uint32_t)dim1 * dim2;
  uint32_t totalBytes = totalElements * GetElementSize(desc->type);
  uint8_t* data = (uint8_t*)malloc(totalBytes);
  if (!data) {
    return false;
  }
  memset(data, 0, totalBytes);
  heapBytesUsed += totalBytes;
  desc->dim1Size = dim1;
  desc->dim2Size = dim2;
  desc->data = data;
  desc->totalBytes = totalBytes;
  desc->isDimmed = true;
  return true;
}

// Frees every array and resets the table, used by NEW, RUN, and CLR.
void ClearAllArrays(void) {
  for (int i = 0; i < arrayDescriptorCount; i++) {
    if (arrayDescriptors[i].data) {
      heapBytesUsed -= arrayDescriptors[i].totalBytes;
      free(arrayDescriptors[i].data);
      arrayDescriptors[i].data = NULL;
    }
  }
  if (arrayDescriptors) {
    memset(arrayDescriptors, 0,
           arrayDescriptorCapacity * sizeof(ArrayDescriptor));
  }
  arrayDescriptorCount = 0;
}
