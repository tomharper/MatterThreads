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




#include <unordered_map>
#include <list>

    class LRUCache {
        int capacity;
        // List stores {key, value} pairs; MRU at front, LRU at back
        std::list<std::pair<int, int>> cacheList;
        // Map stores key -> iterator to the list node for O(1) access
        std::unordered_map<int, std::list<std::pair<int, int>>::iterator> cacheMap;

    public:
        LRUCache(int cap) : capacity(cap) {}

        int get(int key) {
            if (cacheMap.find(key) == cacheMap.end()) return -1;
            
            // Move accessed node to the front (MRU)
            cacheList.splice(cacheList.begin(), cacheList, cacheMap[key]);
            return cacheMap[key]->second;
        }

        void put(int key, int value) {
            if (cacheMap.find(key) != cacheMap.end()) {
                // Key exists: update value and move to front
                cacheList.splice(cacheList.begin(), cacheList, cacheMap[key]);
                cacheMap[key]->second = value;
            } else {
                // Evict LRU if at capacity
                if (cacheList.size() == capacity) {
                    int lastKey = cacheList.back().first;
                    cacheMap.erase(lastKey);
                    cacheList.pop_back();
                }
                // Insert new node at front
                cacheList.push_front({key, value});
                cacheMap[key] = cacheList.begin();
            }
        }
    };




#include <iostream>
#include <queue>
#include <string>
#include <vector>

// Define Task Priorities
enum Priority { LOW = 0, MEDIUM = 1, HIGH = 2 };

struct Task {
    int id;
    std::string name;
    Priority priority;

    // Operator overloading for priority queue
    bool operator<(const Task& other) const {
        return priority < other.priority; // Higher priority number comes first
    }
};

class TaskManager {
    std::priority_queue<Task> tasks;
public:
    void addTask(int id, std::string name, Priority p) {
        tasks.push({id, name, p});
    }

    void processTasks() {
        while (!tasks.empty()) {
            Task t = tasks.top();
            std::cout << "Processing: " << t.name << " (Priority: " << t.priority << ")\n";
            tasks.pop();
        }
    }
};

#ifdef LEETCODE_STANDALONE
int main() {
    TaskManager tm;
    tm.addTask(1, "Fix bugs", HIGH);
    tm.addTask(2, "Write docs", LOW);
    tm.addTask(3, "Implement feature", MEDIUM);

    tm.processTasks();
    return 0;
}
#endif




struct Node {
    int data;
    Node* next;
};

Node* deleteMiddle(Node* head) {
    // Edge case: Empty list or single node
    if (head == nullptr || head->next == nullptr) {
        delete head;
        return nullptr;
    }

    Node* slow = head;
    Node* fast = head;
    Node* prev = nullptr;

    // Fast moves 2 steps, slow moves 1 step
    while (fast != nullptr && fast->next != nullptr) {
        fast = fast->next->next;
        prev = slow;
        slow = slow->next;
    }

    // slow is now the middle node; prev is the node before it
    prev->next = slow->next;
    delete slow; // Free memory

    return head;
}


/*
 Prompt
 Given a 2D rectangular matrix, return all of the values in a single, linear array in spiral order. Start at (0, 0) and first include everything in the first row. Then down the last column, back the last row (in reverse), and finally up the first column before turning right and continuing into the interior of the matrix.

  

 For example:

  1  2  3  4
  5  6  7  8
  9 10 11 12
  13 14 15 16

 Returns:

  

 [1, 2, 3, 4, 8, 12, 16, 15, 14, 13, 9, 5, 6, 7, 11, 10]

 function spiralTraversal(matrix) {
   // your code here
 }
 */
