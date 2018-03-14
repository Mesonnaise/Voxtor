#include<stdexcept>
#include<intrin.h> 
#include<algorithm>
#include"Index.h"

Index::Index(uint64_t index,size_t size):mIndex(index),mSize(size){}
Index::Index(size_t size):mIndex(0),mSize(size){}

Index::Index():mIndex(0),mSize(0){};
Index::Index(Vector vec,size_t size):mSize(size){
 // uint64_t levels=__lzcnt(vec.mX|vec.mY|vec.mZ);
  size=64-size;

  mIndex=0;
  /*
  for(int i=0;i<levels;i++){
    int off=i<<2;
    uint32_t mask=1<<i;
    mIndex|=(vec.mX&mask)<<(off);
    mIndex|=(vec.mY&mask)<<(1+off);
    mIndex|=(vec.mZ&mask)<<(2+off);
  }
  */
  //Not all processors support BMI2
  //PDEP redistributes a value based on the input mask
  //Mask every 3rd bit
  const uint64_t mask=0x1249249249249249ULL;
  mIndex=_pdep_u64(vec.mX,mask);
  mIndex|=_pdep_u64(vec.mY,mask<<1);
  mIndex|=_pdep_u64(vec.mZ,mask<<2);
  
}

size_t Index::Size()const{
  return mSize;
}

size_t Index::IntersectionPoint(const Index &b)const{
  size_t size=std::min<size_t>(mSize,b.mSize);

  uint64_t intersect=mIndex^b.mIndex;
  uint64_t offset=__lzcnt64(intersect);
  offset=((66-offset)/3);
  
  return std::min<size_t>(mSize-offset,size);
}

Index Index::Slice(size_t offset,size_t size)const{
  size=std::min<size_t>(mSize-offset,size);
  offset=mSize-(offset+size);
  return Index(mIndex>>(offset*3),size);
}

Index::Proxy Index::operator[](unsigned int idx){
  if(idx>=mSize)
    throw new std::range_error("Index out of bound");
  uint8_t pos=(mSize-(idx+1))*3;
  return {mIndex,pos};
}

bool Index::operator==(const Index &b)const{
  return mIndex==b.mIndex&&mSize==b.mSize;
}