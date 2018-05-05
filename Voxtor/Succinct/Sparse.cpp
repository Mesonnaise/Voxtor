#include<algorithm>
#include<stdexcept>
#include"Sparse.h"
#include"Util.h"
//#include<Windows.h>
namespace Succinct{
  L0Block* Sparse::FindBlock(L0Block *curBlock,uint64_t &subCount,uint64_t &pos)const{

    while(curBlock!=nullptr&&curBlock->Size()<pos){
      subCount+=curBlock->Size();
      pos-=curBlock->Size();
      curBlock=curBlock->Next();
    }

    return curBlock;
  }

  Sparse::Sparse(size_t initialSize):mBitCount(initialSize){
    mRoot=new L0Block(2351104);

    mSize=mRoot->ReserveSize();

    L0Block *curBlock=mRoot;

    size_t size=std::min(curBlock->ReserveSize(),initialSize);
    curBlock->Size(size);
    initialSize-=size;

    while(initialSize){
      auto newBlock=new L0Block(2351104);
      mSize+=newBlock->ReserveSize();
      curBlock->AppendBlock(newBlock);
      curBlock=newBlock;
    }
  }

  Sparse::~Sparse(){
    if(mRoot)
      delete mRoot;
  }

  uint64_t Sparse::Rank(uint64_t pos,Cache *c)const{
    if(pos>=Size())
      throw std::range_error("Succinct::Sparse::Rank: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Block *curBlock;

    CacheVars(c,global,pos,curBlock);

    curBlock=FindBlock(curBlock,global,pos);

    CacheAssign(c,global,pos,curBlock);

    return curBlock->Rank(pos);
  }

  uint64_t Sparse::Select(uint64_t count)const{
    L0Block *curBlock=mRoot;
    L0Block *resultBlock=nullptr;

    uint64_t rollPosition=0;

    while(curBlock!=nullptr&&curBlock->BaseCounter()<count){
      rollPosition+=curBlock->Size();
      resultBlock=curBlock;
      curBlock=curBlock->Next();
    }

    return rollPosition+resultBlock->Select(count);
  }

  void Sparse::Set(uint64_t pos,Cache *c){
    if(pos>=Size())
      throw std::range_error("Succinct::Sparse::Set: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Block *curBlock;

    CacheVars(c,global,pos,curBlock);

    curBlock=FindBlock(curBlock,global,pos);

    curBlock->Set(pos);

    mRevision++;

    CacheAssign(c,global,pos,curBlock);
  }

  void Sparse::Clear(uint64_t pos,Cache *c){
    if(pos>=Size())
      throw std::range_error("Succinct::Sparse::Clear: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Block *curBlock;

    CacheVars(c,global,pos,curBlock);

    curBlock=FindBlock(curBlock,global,pos);

    curBlock->Clear(pos);

    mRevision++;

    CacheAssign(c,global,pos,curBlock);
  }

  bool Sparse::Get(uint64_t pos,Cache *c)const{
    if(pos>=Size())
      throw std::range_error("Succinct::Sparse::Get: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Block *curBlock;

    CacheVars(c,global,pos,curBlock);

    L0Block *curBlock=FindBlock(mRoot,global,pos);

    CacheAssign(c,global,pos,curBlock);

    return curBlock->Get(pos);
  }

  void Sparse::Insert(uint64_t pos,size_t ammount,uint64_t value,Cache *c){
    if(pos>Size())
      throw std::range_error("Succinct::Sparse::Insert: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Block *curBlock;

    CacheVars(c,global,pos,curBlock);

    L0Block *curBlock=FindBlock(mRoot,global,pos);
    uint64_t carry;

    carry=curBlock->Insert(pos,ammount,value);

    if(curBlock->Full()){
      auto nextBlock=curBlock->Next();
      if((nextBlock->Size()+ammount)<nextBlock->ReserveSize())
        nextBlock->Insert(0,ammount,carry);
      else{
        nextBlock=new L0Block(2351104);
        nextBlock->Insert(0,ammount,carry);
        curBlock->AppendBlock(nextBlock);
      }
    }
    
    mRevision++;

    CacheAssign(c,global,pos,curBlock);
  }

  uint64_t Sparse::Remove(uint64_t pos,size_t ammount){
    if(pos+ammount>Size())
      throw std::range_error("Succinct::Sparse::Remove: position exceeds upper bounds of bit array");

    L0Block *curBlock=mRoot;
    L0Block *lastSegment=mRoot;

    while(curBlock!=nullptr&&curBlock->Size()<pos){
      pos-=curBlock->Size();
      lastSegment=curBlock;
      curBlock=curBlock->Next();
    }

    uint64_t carry=curBlock->Remove(pos,ammount);

    if(curBlock->Empty())
      delete curBlock;

    mRevision++;

    return carry;
  }

  void Sparse::Filter(Sparse *mask,uint64_t maskScale){
    L0Block *curBlock=mRoot;
    L0Block *maskBlock=mask->mRoot;
    size_t count=0;

    while(curBlock&&maskBlock){
      while(curBlock&&count<mask->Size()){
        count+=curBlock->Filter(maskBlock,count,maskScale);
        curBlock=curBlock->Next();
      }

      count=0;
      maskBlock=maskBlock->Next();
    }
  }

  void Sparse::Compact(){
    L0Block *curBlock=mRoot;
    L0Block *donorBlock=curBlock->Next();

    while(donorBlock){
      curBlock->AppendDonation(donorBlock);

      if(donorBlock->Empty()){
        auto next=donorBlock->Next();
        delete donorBlock;
        donorBlock=next;
      }else{
        curBlock=donorBlock;
        donorBlock=donorBlock->Next();
      }
    }
  }

  size_t Sparse::MemUsage()const{
    size_t total=sizeof(Sparse);

    L0Block *curBlock=mRoot;

    while(curBlock){
      total+=sizeof(L0Block);
      total+=curBlock->AllocatedSize();
      curBlock=curBlock->Next();
    }

    return total;
  }
}