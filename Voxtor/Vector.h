#pragma once
#include<cinttypes>
class Vector{
  friend class Index;
protected:
  uint64_t mX;
  uint64_t mY;
  uint64_t mZ;
public:
  Vector(uint64_t x,uint64_t y,uint64_t z);
  ~Vector();

  long double Distance(const Vector &b)const;

  Vector operator+(const Vector &b)const;
  Vector operator-(const Vector &b)const;
  Vector operator*(const Vector &b)const;
  Vector operator/(const Vector &b)const;

  Vector& operator+=(const Vector &b);
  Vector& operator-=(const Vector &b);
  Vector& operator*=(const Vector &b);
  Vector& operator/=(const Vector &b);

  bool operator==(const Vector &b)const;
  bool operator!=(const Vector &b)const;
  //bool operator<(const Vector &b)const;
  //bool operator>(const Vector &b)const;
  //bool operator<=(const Vector &b)const;
  //bool operator>=(const Vector &b)const;
};

