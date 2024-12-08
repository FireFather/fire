#pragma once
#include "main.h"

inline file operator+(const file f1, const file f2) {
  return static_cast<file>(static_cast<int>(f1) + static_cast<int>(f2));
}

inline file operator-(const file f1, const file f2) {
  return static_cast<file>(static_cast<int>(f1) - static_cast<int>(f2));
}

inline file& operator++(file& f) {
  return f = static_cast<file>(static_cast<int>(f) + 1);
}

inline rank operator+(const rank r1, const rank r2) {
  return static_cast<rank>(static_cast<int>(r1) + static_cast<int>(r2));
}

inline rank operator-(const rank r1, const rank r2) {
  return static_cast<rank>(static_cast<int>(r1) - static_cast<int>(r2));
}

inline rank& operator++(rank& r) {
  return r = static_cast<rank>(static_cast<int>(r) + 1);
}

inline rank& operator--(rank& r) {
  return r = static_cast<rank>(static_cast<int>(r) - 1);
}

inline score operator+(const score s1, const score s2) {
  return static_cast<score>(static_cast<int>(s1) + static_cast<int>(s2));
}

inline score operator-(const score s1, const score s2) {
  return static_cast<score>(static_cast<int>(s1) - static_cast<int>(s2));
}

inline score operator*(const int i, const score s) {
  return static_cast<score>(i * static_cast<int>(s));
}

inline score operator*(const score s, const int i) {
  return static_cast<score>(static_cast<int>(s) * i);
}

inline score& operator+=(score& s1, const score s2) {
  return s1 = s1 + s2;
}

inline score& operator-=(score& s1, const score s2) {
  return s1 = s1 - s2;
}

inline side operator+(const side c1, const side c2) {
  return static_cast<side>(static_cast<int>(c1) + static_cast<int>(c2));
}

inline side operator-(const side c1, const side c2) {
  return static_cast<side>(static_cast<int>(c1) - static_cast<int>(c2));
}

inline side operator*(const int i, const side c) {
  return static_cast<side>(i * static_cast<int>(c));
}

inline side& operator++(side& c) {
  return c = static_cast<side>(static_cast<int>(c) + 1);
}

inline square operator+(const square s1, const square s2) {
  return static_cast<square>(static_cast<int>(s1) + static_cast<int>(s2));
}

inline square operator-(const square s1, const square s2) {
  return static_cast<square>(static_cast<int>(s1) - static_cast<int>(s2));
}

inline square operator*(const int i, const square s) {
  return static_cast<square>(i * static_cast<int>(s));
}

inline square& operator+=(square& s1, const square s2) {
  return s1 = s1 + s2;
}

inline square& operator-=(square& s1, const square s2) {
  return s1 = s1 - s2;
}

inline square& operator++(square& s) {
  return s = static_cast<square>(static_cast<int>(s) + 1);
}

inline square& operator--(square& s) {
  return s = static_cast<square>(static_cast<int>(s) - 1);
}

inline square operator/(const square s, const int i) {
  return static_cast<square>(static_cast<int>(s) / i);
}
