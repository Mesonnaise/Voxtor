#pragma once
#include<cinttypes>
#include"Vector.h"
class Index{
public:
  struct Proxy{
  private:
    uint64_t &index;
    size_t pos;
  public:
    Proxy(uint64_t &i,size_t p):index(i),pos(p){}
    operator const uint8_t()const{
      return (index>>pos)&0x07;
    }
    Proxy& operator=(const uint64_t& rhs){
      index&=~(0x07<<pos);
      index|=(rhs&0x07)<<pos;
      return *this;
    }
  };
private:
  uint64_t mIndex;
  size_t   mSize;
protected:
  Index(uint64_t index,size_t size);
public:
  Index();
  Index(size_t size);
  Index(Vector vec,size_t size);
  size_t Size()const;
  size_t IntersectionPoint(const Index &b)const;
  Index Slice(size_t offset,size_t size)const;

  Vector GetVector()const;

  Proxy operator[](size_t idx);
  uint8_t operator[](size_t idx)const;

  bool operator==(const Index &b)const;
  
};