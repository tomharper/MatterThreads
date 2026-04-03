//
//  Leetcode.cpp
//  MatterAI
//
//  Created by Tom Harper on 3/28/26.
//

#include "Leetcode.hpp"
#include <vector>
#include <string>
#include <set>
#include <algorithm>

using namespace std;


class Solution {
    struct ListNode {
        int val;
        ListNode *next;
        ListNode() : val(0), next(nullptr) {}
        ListNode(int x) : val(x), next(nullptr) {}
        ListNode(int x, ListNode *next) : val(x), next(next) {}
   };
public:
    ListNode* mergeTwoLists(ListNode* list1, ListNode* list2) {

        if (!list2 && !list1) {
            return NULL;
        }

        ListNode* retList = new ListNode();
        ListNode* ret = retList;



        while (list1 || list2) {
            if (!list2) {
                retList->next = list1;
                list1 = list1->next;
            }
            else if (!list1) {
                retList->next  = list2;
                list2 = list2->next;
            }
            else if (list1->val < list2->val) {
                retList->next = list1;
                list1 = list1->next;
            }
            else {
                retList->next  = list2;
                list2 = list2->next;
            }
            retList = retList->next;
        }
        return ret->next;
    }
};


class Solution2 {
public:
    int trap(vector<int>& height) {

        int start = 0;
        int end = (int)height.size() - 1;

        int leftMax = 0;
        int rightMax = 0;
        int totalWater = 0;

        while(start < end){

            leftMax = max(leftMax, height[start]);
            rightMax = max(rightMax, height[end]);

            if(leftMax < rightMax){
                totalWater += leftMax - height[start];
                start++;
            }
            else{
                totalWater += rightMax - height[end];
                end--;
            }
        }

        return totalWater;
    }
};

class TrieNode {
public:
    vector <TrieNode*> children;
    vector<string> suggestions;
    TrieNode() : children(26, nullptr) {}
};

class Trie {
private:
    TrieNode* root;
public:
    
    Trie() {
        root = new TrieNode();
    }
    void insert(string product) {
        TrieNode* node = root;
        for (char c: product) {
            int index  = c - 'a';
            if (node->children[index] == nullptr) {
                node->children[index] = new TrieNode();
            }
            node = node->children[index];
            if (node->suggestions.size()<3) {
                node->suggestions.push_back(product);
            }
        }
        
    }
    
    vector<string> search(string prefix) {
        TrieNode* node = root;
        for (char c: prefix) {
            int index  = c - 'a';
            if (node->children[index] == nullptr) {
                return {};
            }
            node = node->children[index];
        }
        return node->suggestions;
    }
};

//1268 leetcode
class Solution3 {
public:
    vector<vector<string>> suggestionProducts(vector<string>& products, string searchWord) {
        Trie trie;
        sort(products.begin(), products.end());
        for (string product: products) {
            trie.insert(product);
        }
        vector<vector<string>> result;
        string prefix = "";
        for (char c: searchWord) {
            prefix += c;
            result.push_back(trie.search(prefix));
        }
        return result;
    }
                                                                                             
};

// best time to buy and selll stock

class Solution4 {
public:
    vector<vector<int>> getSkyline(vector<vector<int>>& buildings) {
        vector<vector<int>> ans;
        multiset <int> pq{0};
        
        vector<pair<int, int>> points;
        
        for(auto b: buildings){
            points.push_back({b[0], -b[2]});
            points.push_back({b[1], b[2]});
        }
        
        sort(points.begin(), points.end());
        
        int ongoingHeight = 0;
        
        // points.first = x coordinate, points.second = height
        for(int i = 0; i < points.size(); i++){
            int currentPoint = points[i].first;
            int heightAtCurrentPoint = points[i].second;
            
            if(heightAtCurrentPoint < 0){
                pq.insert(-heightAtCurrentPoint);
            } else {
                pq.erase(pq.find(heightAtCurrentPoint));
            }
            
            // after inserting/removing heightAtI, if there's a change
            auto pqTop = *pq.rbegin();
            if(ongoingHeight != pqTop){
                ongoingHeight = pqTop;
                ans.push_back({currentPoint, ongoingHeight});
            }
        }
        
        return ans;
    }
};

// buy and sell stock

class Solution5 {
public:
    int maxProfit(vector<int>& prices) {
        int n = (int)prices.size();
        vector<int> maxValueUntilEnd(n, 0);
        maxValueUntilEnd[n -1] = prices[n -1];
        for (int i=1; i<n; i++) {
            maxValueUntilEnd[n - i -1] = max(maxValueUntilEnd[n-1], prices[n-i-1]);
        }
        int profit = 0;
        for (int i=0; i<n-1;i++) {
            profit = max(maxValueUntilEnd[i+1]-prices[i], profit);
        }
        return profit;
    }
};


class Solution6 {
public:
    bool containsDuplicate(vector<int>& nums) {
        set<int> st;
        for (int i: nums) {
            if (st.contains(i))
                return true;
            else st.insert(i);
        }
        return false;
    }
};



class Solution7 {
};
