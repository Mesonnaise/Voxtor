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

  constexpr uint64_t X(){ return mX; }
  constexpr uint64_t Y(){ return mY; }
  constexpr uint64_t Z(){ return mZ; }

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

