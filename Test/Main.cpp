#include<intrin.h>
#include<cinttypes>
#include<algorithm>
#include<random>
#include<atomic>
#include<malloc.h>
#include<thread>
#include<vector>
#include"Tree.h"

void threadFunction(Tree *tree,Vector dim,uint64_t min,uint64_t max){
  for(uint64_t z=min;z<max;z++){
    for(uint64_t y=0;y<dim.Y();y++){
      for(uint64_t x=0;x<dim.X();x++)
        tree->Set({x,y,z});
    }
  }
}

int main(){


  
  Tree tree(8,false);
  auto dim=tree.Dimensions();

  for(uint64_t z=0;z<64;z++){
    for(uint64_t y=0;y<64;y++){
      for(uint64_t x=0;x<64;x++)
        tree.Set({x,y,z});
    }
  }
  /*
  std::vector<std::thread> threads;

  for(uint64_t z=0;z<dim.Z();z+=dim.Z()/16){
    threads.emplace_back(threadFunction,&tree,dim,z,z+dim.Z());
  }

  for(auto &thread:threads)
    thread.join();
  */
  return 0;
}