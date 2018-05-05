#include "Vector.h"
#include<cmath>


Vector::Vector(uint64_t x,uint64_t y,uint64_t z):mX(x),mY(y),mZ(z){
}


Vector::~Vector(){
}

long double Vector::Distance(const Vector &b)const{
  int64_t x=mX-b.mX;
  int64_t y=mY-b.mY;
  int64_t z=mZ-b.mZ;
  return std::sqrtl(double(x*x+y*y+z*z));
}

Vector Vector::operator+(const Vector &b)const{
  return Vector(mX+b.mX,mY+b.mY,mZ+b.mZ);
}
Vector Vector::operator-(const Vector &b)const{
  return Vector(mX-b.mX,mY-b.mY,mZ-b.mZ);
}
Vector Vector::operator*(const Vector &b)const{
  return Vector(mX*b.mX,mY*b.mY,mZ*b.mZ);
}
Vector Vector::operator/(const Vector &b)const{
  return Vector(mX/b.mX,mY/b.mY,mZ/b.mZ);
}

Vector& Vector::operator+=(const Vector &b){
  mX+=b.mX;
  mY+=b.mY;
  mZ+=b.mZ;
  return *this;
}
Vector& Vector::operator-=(const Vector &b){
  mX-=b.mX;
  mY-=b.mY;
  mZ-=b.mZ;
  return *this;
}
Vector& Vector::operator*=(const Vector &b){
  mX*=b.mX;
  mY*=b.mY;
  mZ*=b.mZ;
  return *this;
}
Vector& Vector::operator/=(const Vector &b){
  mX/=b.mX;
  mY/=b.mY;
  mZ/=b.mZ;
  return *this;
}

bool Vector::operator==(const Vector &b)const{
  return mY==b.mY&&mZ==b.mZ&&mX==b.mX;
}
bool Vector::operator!=(const Vector &b)const{
  return mX!=b.mX||mY!=b.mY||mZ!=b.mZ;
}
