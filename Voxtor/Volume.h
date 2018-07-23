#pragma once
#include"Tree.h"
template<TreeType IsDense> 
class Volume{

  Tree<IsDense> mBase;
public:
  Volume(size_t depth);
};