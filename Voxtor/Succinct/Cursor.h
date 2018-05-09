#pragma once
#include<cinttypes>
#include"L0Block.h"
namespace Succinct{
  template<class T>
  class Cursor{
  private:
    T        *mRoot;
    T        *mCursor;
    uint64_t  mOffset=0;

  public:
    Cursor():mRoot(nullptr),mCursor(nullptr){}
    Cursor(T *root):mRoot(root),mCursor(root){}

    constexpr uint64_t Offset(){ return mOffset; }

    void Reset(){
      mCursor=mRoot;
      mOffset=0;
    }

    void Reset(bool r){ 
      if(r){
        mCursor=mRootBlock;
        mOffset=0;
      }
    }

     void Seek(uint64_t &pos){
      /*
      while(mCursorBlock&&pos>(mCursorOffset+mCursorBlock->Size())){
        mCursorOffset+=mCursorBlock->Size();
        mCursorBlock=mCursorBlock->Next();
      }
      */

      while(mCursor->Next()&&pos>(mOffset+mCursor->Size())){
        mOffset+=mCursor->Size();
        mCursor=mCursor->Next();
      }

      while(pos<mOffset&&mCursor->Prev()){
        mCursor=mCursor->Prev();
        mOffset-=mCursor->Size();
      }

      pos-=mOffset;
    }

    operator bool(){ return mCursor!=nullptr; }
    operator T*(){ return mCursor; };

    T* operator->(){ return mCursor; }
    T* operator*(){  return mCursor; }

    Cursor<T> operator++(int){
      auto ret=*this;
      mOffset+=mCursor->Size();
      mCursor=mCursor->Next();
      
      return ret;
    }

    Cursor<T> operator--(int){
      auto ret=*this;
      mCursor=mCursor->Prev();
      mOffset-=mCursor->Size();
      return ret;
    }
  };
}