#pragma once

#include "aion_abstract.hpp"
#include "aion_bitset.hpp"
#include "aion_hash.hpp"



#include <unordered_map>
#include <vector>
#include <set>
#include<cmath>
#include<stack>


#include <iostream>
#include <vector>
#include <algorithm>

#include <vector>
#include <algorithm>

namespace aion {

using namespace std;


template<typename DATA_TYPE, typename COUNT_TYPE>
class BH_BF : public Abstract<DATA_TYPE, COUNT_TYPE>{
public: const uint64_t hash_num=3;

const vector<uint64_t>bh_list={1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535,131070};
const vector<uint64_t>bh_list1={1, 11, 121,1331, 14641,161051,1771561,19487171,214358881,2357947691};

        const uint64_t length=3074;
    struct BH{

const vector<uint64_t>bh_list={1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535,131070};
const vector<uint64_t>bh_list1={1, 11, 121,1331, 14641,161051,1771561,19487171,214358881,2357947691};

    #define BFSEED 1001
         const size_t arraySize = 90240;
        // 成员变量
        BHSet* bh;
        const uint64_t hash_num=3;
        const uint64_t length=3074;
int changeh(uint64_t to){
for(int i=0;i<bh_list.size();i++){//10
   if(to==bh_list[i])return i;
}
//std::cout<<"tonofind"<<to;
return -1;
}
// 降序排列辅助函数
static bool descending(int a, int b) {
    return a > b;
}

// 回溯函数
static bool backtrack(const vector<uint64_t>& D, int start, int remaining_count, 
              int target_sum, vector<int>& path) {
              //  std::cout<<"break"<<target_sum<<" "<<remaining_count<<std::endl;
    if (remaining_count == 0 && target_sum == 0) return true;
    if (remaining_count <= 0 || target_sum <= 0) return false;

    for (int i = start; i < D.size(); ++i) {
        // 剪枝：剩余元素不足时提前终止
        if (D.size() - i < remaining_count) break;

        // 剪枝：当前元素过大时跳过
        if (D[i] > target_sum) continue;

        path.push_back(D[i]);
        if (backtrack(D, i+1, remaining_count-1, target_sum-D[i], path)) {
            return true;
        }
        path.pop_back();
    }
    return false;
}

// 主函数接口
static vector<uint64_t> decomposeSum(uint64_t sum, uint64_t count, vector<uint64_t>&D) {
    // 预处理：降序排列
    sort(D.begin(), D.end(), descending);   
    vector<uint64_t> result;
    if (backtrack(D, 0, count, sum, result)) {
        return result;
    }
    return vector<uint64_t>(); // 无解返回空
}
 bool backtrack(const vector<uint64_t>& nums, uint64_t targetSum, uint64_t targetCount, 
              uint64_t start, uint64_t currentSum, uint64_t currentCount, 
              vector<uint64_t>& path, vector<uint64_t>& result) {
                //std::cout<<"cou"<<currentCount<<" "<<targetCount<<std::endl;
                //std::cout<<"sum"<<currentSum<<" "<<targetSum<<std::endl;
    if (currentCount == targetCount) {
        if (currentSum == targetSum) {
            result = path;
            return true;
        }
        return false;
    }
                //std::cout<<"size"<<nums.size()<<"start"<<start<<std::endl;
    for (uint64_t i = start; i < nums.size(); ++i) {
//std::cout<<"i="<<i<<" "<<nums[i]<<"currentsum"<<currentSum<<"target"<<targetSum<<std::endl;
        // 剪枝：当前和溢出或剩余元素不足
    //std::cout<<"currentcoun"<<currentCount<<"targetcount "<<targetCount<<std::endl;
        if (currentSum + nums[i] > targetSum || 
            nums.size() - i < targetCount - currentCount) {
            break;
        }
        path.push_back(nums[i]);
        if (backtrack(nums, targetSum, targetCount, i + 1, 
                     currentSum + nums[i], currentCount + 1, path, result)) {
            return true; // 找到解，提前终止
        }
        path.pop_back();
    }


               // std::cout<<"size"<<nums.size()<<"end"<<std::endl;
    return false;
}


    vector<uint64_t> result;
    vector<uint64_t> path;
 vector<uint64_t> findCombination(uint64_t sum, uint64_t counter) {
    // 创建可修改的排序副本（若全局数组未排序）
while(!result.empty())
result.pop_back();
while(!path.empty())
path.pop_back();
  if (backtrack(bh_list, sum, counter, 0, 0, 0, path, result)) {
    return result;
    }
    else{
    return {}; // 无解返回空vector
    }
}

 



        BH(uint64_t _hash_num, uint64_t _length):hash_num(_hash_num), length(_length){
            bh = new BHSet(length);
        }

            vector<uint64_t> now;
            vector<uint64_t> fl;

            int tolerance = 17;

        bool insert(uint64_t ip,uint64_t window){

            while(!now.empty())
            now.pop_back();
            int sum;
            int flag=0;
//std::cout<<"ok"<<std::endl;
            int kel=0;
            uint64_t sip=ip & 0xFFFFFFFF;
            uint64_t* ip_h=new uint64_t[hash_num];
            uint64_t* window_h=new uint64_t[hash_num];
            //std::cout<<"hash"<<hash_num<<std::endl;
            for(uint64_t j = 0; j < hash_num; j++) {
                
                flag=0;
                ip_h [j]= (hash(ip,  BFSEED+j) % length);
                window_h [0]= window;//(hash(window, BFSEED) % 10);
               now=findCombination(bh->bh_sum[ip_h [j]],bh->bh_count[ip_h [j]]);
               
                //if(ip_h[j]==4598)std::cout<<"adds"<<sip<<std::endl;
//if(sip==1364)std::cout<<"addpo"<<ip_h[j]<<j<<std::endl;
               for( uint64_t tt:now ){  //std::cout<<"now"<<tt<<std::endl;
                if(tt==bh_list[window]){flag++;kel++;break;}
                }
               // }
               
            if(flag==0)
{   
bh->add(ip_h [j], bh_list[window]);//return true;
}
            }
            if(kel==3)return false;
            return true;
           // std::cout<<"ip"<<ip_h [0]<<" "<<bh_list[ window_h[0]]<<std::endl;
//std::cout<<"insert_e"<<std::endl;
        }

     



int findda(vector<uint64_t> pos,uint64_t now){
int k=11;
for(int i=0;i<pos.size();i++)
if(pos[i]==bh_list[now])k=(i+pos.size()-1)%pos.size();

return k;
}



        bool find(uint64_t window){

//std::cout<<"find_b"<<length<<std::endl;
            int zero_value_count = 0;
          

        uint64_t window_h;

for(uint64_t j = 0; j <length; j++) {
            
            window_h=window;//hash(window,  BFSEED) % 10;
            
 
          // std::cout<<bh->bh_sum[j]<<" "<<bh->bh_count[j]<<" "<<fl.size()<<std::endl;

            fl=findCombination(bh->bh_sum[j],bh->bh_count[j]);
            //std::cout<<"fl"<<fl.size()<<std::endl;
            for (uint64_t it:fl){
               // std::cout<<"sd"<<it<<std::endl;
             if(it==bh_list[window_h]){
             bh->bh_count[j]-=1;
             bh->bh_sum[j]-=bh_list[window_h];
             }

             }
            }

if(zero_value_count==3)return true;
else return false;
        }




// for std::sort and std::set_intersection

void computeIntersection(std::vector<uint64_t> three[], int hash_num, std::vector<uint64_t>& result) {
    if (hash_num == 0) {
        result.clear(); // 如果没有输入，结果为空
        return;
    }

    // 初始化为第一个数组
    result = three[0];
    std::sort(result.begin(), result.end()); // 先排序

    std::vector<uint64_t> temp; // 临时存储交集结果

    for (int i = 1; i < hash_num; ++i) {
        std::vector<uint64_t> current = three[i]; // 当前数组
        std::sort(current.begin(), current.end()); // 排序

        // 计算当前数组与 result 的交集，存入 temp
        std::set_intersection(
            result.begin(), result.end(),
            current.begin(), current.end(),
            std::back_inserter(temp)
        );

        result = temp; // 更新 result
        temp.clear();  // 清空 temp

        // 如果 result 已经为空，提前终止
        if (result.empty()) break;
    }
}

//std::vector<uint64_t> three[3];
std::vector<uint64_t> result1;
int period[8];//tolerance
int small=tolerance+1;
uint64_t tule=0;
uint64_t isperiod(uint64_t ip,uint64_t window){


            uint64_t sip=ip & 0xFFFFFFFF;
for(int i=0;i<8;i++)
period[i]=0;
           // if(sip==682)
//std::cout<<"shima"<<sip<<" "<<window<<std::endl;

while(!result1.empty()){
    result1.pop_back();
}
//for(int i=0;i<hash_num;i++)
//while(!three[i].empty()){
   // three[i].pop_back();
//}
uint64_t* ip_h=new uint64_t[hash_num];
uint64_t* window_h=new uint64_t[hash_num];
 small=bh_list.size()+1;
 tule=0;
for(uint64_t j = 0; j <hash_num; j++) {
            ip_h[j]= hash(ip, BFSEED+j) % length;
            window_h [j]=window;//hash(window,  BFSEED) % 10;
            if(bh->bh_count[ip_h [j]]<small){
            small=bh->bh_count[ip_h [j]];
            tule=ip_h [j];}

//if(sip==1364)std::cout<<hash_num<<"tule"<<tule<<"sip"<<sip<<" "<<bh->bh_count[ip_h[j]]<<" "<<ip_h[j]<<std::endl;
}
result1=findCombination(bh->bh_sum[tule],bh->bh_count[tule]);

uint64_t fwindow=window;
   //for(int i=0;i<hash_num;i++)

//std::partial_sort(result1.begin(), result1.end(), result1.end());
uint64_t pe=0;
//uint64_t period=0;

//if(sip==1364)
//std::cout<<"sip"<<sip<<"tule"<<tule<<std::endl;
//std::cout<<"window"<<window<<std::endl;
for(int i=0;i<result1.size();i++)
{

//if(sip==1364)
//std::cout<<"interva"<<(window+tolerance-changeh(result1[i]))%tolerance;//10

//if(sip==1364)
//std::cout<<"lol"<<result1[i]<<" "<<changeh(result1[i])<<std::endl;

if((window+tolerance-changeh(result1[i]))%tolerance==0) //10
{period[0]++;period[1]++;period[2]++;period[3]++;
period[4]++;period[5]++;period[6]++;period[7]++;
}
//else
 if((window+tolerance-changeh(result1[i]))%tolerance==1)
{period[0]++;

}
    else if((window+tolerance-changeh(result1[i]))%tolerance==2)
{period[0]++;period[1]++;

}
    else if((window+tolerance-changeh(result1[i]))%tolerance==3)
{period[0]++;period[2]++;

}
  else if((window+tolerance-changeh(result1[i]))%tolerance==4)
{period[0]++;period[1]++;period[3]++;  period[1]++;

}
    else if((window+tolerance-changeh(result1[i]))%tolerance==5)
{period[0]++;//period[1]++;
period[4]++;
}
    else if((window+tolerance-changeh(result1[i]))%tolerance==6)
{period[0]++;period[1]++;period[2]++;
period[5]++;
}
    else if((window+tolerance-changeh(result1[i]))%tolerance==7)
{period[0]++;//period[1]++;period[2]++;
period[6]++;
} 
    else if((window+tolerance-changeh(result1[i]))%tolerance==8)
{period[0]++;period[1]++;period[3]++;//period[2]++;
period[7]++;
}
    else if((window+tolerance-changeh(result1[i]))%tolerance==9)
{period[0]++;period[2]++;   period[2]++;//period[1]++;period[3]++;
//period[8]++;
}

    else if((window+tolerance-changeh(result1[i]))%tolerance==10)
{period[0]++;period[1]++;//period[2]++;period[3]++;
period[4]++;
}
    else if((window+tolerance-changeh(result1[i]))%tolerance==11)
{period[0]++;//period[1]++;period[2]++;period[3]++;

}
    else if((window+tolerance-changeh(result1[i]))%tolerance==12)
{period[0]++;period[1]++;period[2]++;period[3]++;
period[5]++;
}
    else if((window+tolerance-changeh(result1[i]))%tolerance==13)
{period[0]++;//period[1]++;period[2]++;period[3]++;

}
    else if((window+tolerance-changeh(result1[i]))%tolerance==14)
{period[0]++;period[1]++;//period[2]++;period[3]++;
period[6]++;
}
    else if((window+tolerance-changeh(result1[i]))%tolerance==15)
{period[0]++;period[2]++;//period[1]++;period[3]++;
period[4]++;
}
    else if((window+tolerance-changeh(result1[i]))%tolerance==16)
{period[0]++;period[1]++;period[3]++; period[3]++;//period[2]++;
period[7]++;
}
else if((window+tolerance-changeh(result1[i]))%tolerance==17)
{period[0]++;//period[2]++;
period[7]++;
}
}

for(int i=0;i<8;i++){
//std::cout<<"ppp"<<i<<period[i]<<std::endl;
     

if(tolerance>=3&&period[0]>tolerance)pe=1;
else if(tolerance>=4&&period[1]>(tolerance/2))pe=2;
else if(tolerance>=6&&period[2]>(tolerance/3))pe=3;
else if(tolerance>=8&&period[3]>(tolerance/4))pe=4;
else if(tolerance>=10&&period[4]>(tolerance/5))pe=5;
else if(tolerance>=12&&period[5]>(tolerance/6))pe=6;
else if(tolerance>=14&&period[6]>(tolerance/7))pe=7;
else if(tolerance>=16&&period[7]>(tolerance/8))pe=8;
//if(tolerance>=16&&period[7]>(tolerance/8))pe=8;
//else if(tolerance>=14&&period[6]>(tolerance/7))pe=7;
//else if(tolerance>=12&&period[5]>(tolerance/6))pe=6;
//else if(tolerance>=10&&period[4]>(tolerance/5))pe=5;
//else if(tolerance>=8&&period[3]>(tolerance/4))pe=4;
//else if(tolerance>=6&&period[2]>(tolerance/3))pe=3;
//else if(tolerance>=4&&period[1]>(tolerance/2))pe=2;
//else if(tolerance>=3&&period[0]>=tolerance)pe=1;
     
else pe=0;
}



if(pe==0&&sip<=2000){
  // std::cout<<"wrong ip"<<sip<<" p"<<pe<<" "<<window<<std::endl;
///for(int i=0;i<8;i++)
   //std::cout<<"ppp"<<i<<" "<<period[i]<<std::endl;

//for(int i=0;i<result1.size();i++)
  { //std::cout<<"lol"<<changeh(result1[i])<<std::endl;
  }
    }
//if(period[3]>=3)pe=4;
//else if(period[2]>=3)pe=3;
//else if(period[1]>=5)pe=2;
//else if(period[0]>=10)pe=1;

//std::cout<<"pe"<<pe<<std::endl;

return pe;

////uint64_t period=0;

////if(result1.size()<=2)return 0;

////int keey=0;
          // std::cout<<"snums"<<three[0].size()<<std::endl;
  //         for( uint64_t rr:result1){
            //std::cout<<"lol"<<rr<<std::endl;
    //        if(bh_list[fwindow]==rr){
      //          keey=0;}
       //    }
          /// std::cout<<"fwindow"<<fwindow<<"bh"<<bh_list[fwindow]<<std::endl;
          //// int begin=findda(result1,fwindow);
///std::cout<<"begin"<<begin<<" "<<result1[begin]<<" "<<bh_list[changeh(result1[begin])]<<std::endl;
   ////int left=changeh(result1[begin]);
   ////int right=fwindow;
   ////while(result1[begin]!=bh_list[right]){

     ////keey=0;
   ////period=(fwindow+tolerance-changeh(result1[begin]))%tolerance; //10
   ////keey+=period;
   ////if(period>=tolerance/2)return 0;//10
   ///std::cout<<"pe"<<period<<"begin"<<begin<<"left"<<left<<"right"<<right<<"changed"<<changeh(result1[begin])<<std::endl;
   
    ////while(left!=right){
    ////bool exists = std::binary_search(result1.begin(), result1.end(), bh_list[left]);
       ////   if(exists) {
            
      ///     std::cout<<"left"<<left<<"keyy"<<keey<<std::endl;
       ////left=(left+tolerance-period)%tolerance;//10
        //// keey+=period;
         ///  std::cout<<"left"<<left<<"keyy"<<keey<<std::endl;
      //// }
          //// else 
          //// {
         //// break;
         //// }
           ///std::cout<<exists<<"left"<<left<<std::endl;
           ////if(keey>=tolerance){pe=period;//
           ///std::cout<<"per"<<pe<<std::endl;
          ////return pe;}
           ////if(left==right){pe=period;
           ///std::cout<<"per"<<pe<<std::endl;
           ////return pe;
          // break;
          //// }
  //  }
   ////}
//else {
       ////     if(left==right)break;
          /// std::cout<<"beginp:"<<begin<<std::endl;
  ////begin=(begin+result1.size()-1)%result1.size();//fincdda(result1,begin);
  
          /// std::cout<<"begina:"<<begin<<std::endl;
    ////left=changeh(result1[begin]);
   //std::cout<<"ki"<<begin<<" "<<three[0].size()<<"left"<<left<<"real"<<bh_list[right]<<std::endl;
           
   //// }
  // }
  // std::cout<<"end_b"<<ip<<std::endl;
////return pe;
    }


        inline uint64_t hash(uint64_t data, uint64_t seed){
            //return rangehash(data);
            return Hash::BOBHash64((uint8_t*)&data, sizeof(DATA_TYPE), seed);
        }

    };

        BH* bh; 
        BH_BF(): hash_num(3), length(683*6+17) { // 256 512 1024-30280  2500  3413 4551 5688 6826
        bh = new BH(hash_num, length );
        }
         ~BH_BF(){
       
        delete bh;
    }
    
};



} // namespace aion
