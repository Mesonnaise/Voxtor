#pragma once
#include<cinttypes>
#include"L0Block.h"
namespace Succinct{
  class Cursor{
  private:
    const   L0Block  *mRoot;
    mutable L0Block  *mCursor;
    mutable uint64_t  mOffset=0;

  public:
    //nullptr constructor is just needed for default setting
    //the array constructor will set this before its used
    Cursor():mRoot(nullptr),mCursor(nullptr){}
    Cursor(L0Block *root):mRoot(root),mCursor(root){}

    constexpr const L0Block* Root()  const{ return mRoot; }
    constexpr       uint64_t Offset()const{ return mOffset; }

    inline void Reset()const{
      mCursor=(L0Block*)mRoot;
      mOffset=0;
    }

    inline void Reset(bool r)const{ 
      if(r){
        mCursor=(L0Block*)mRoot;
        mOffset=0;
      }
    }

    inline void Seek(uint64_t &pos)const{
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

    inline operator bool()     const{ return mCursor!=nullptr; }
    inline operator L0Block*() const{ return mCursor; };

    inline L0Block* operator->()const{ return mCursor; }
    inline L0Block* operator*() const{ return mCursor; }

    inline Cursor operator++(int)const{
      auto ret=*this;
      mOffset+=mCursor->Size();
      mCursor=mCursor->Next();
      
      return ret;
    }

    inline Cursor operator--(int)const{
      auto ret=*this;
      mCursor=mCursor->Prev();
      mOffset-=mCursor->Size();
      return ret;
    }
  };
}