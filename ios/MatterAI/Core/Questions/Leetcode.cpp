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


class MergeTwoLists {
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




class TrapWater {
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
class SearchSuggestions {
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

class StockSale {
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

class BestTimeToBuyAndSellStock {
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


class ContainsDuplicate {
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
    std::list<std::pair<int, int>> cacheList;
    std::unordered_map<int, std::list<std::pair<int, int>>::iterator> cacheMap;
    mutable std::mutex cacheMutex;  // Add this

public:
    LRUCache(int cap) : capacity(cap) {}

    int get(int key) {
        std::lock_guard<std::mutex> lock(cacheMutex);  // Lock entire operation
        
        if (cacheMap.find(key) == cacheMap.end()) return -1;
        
        cacheList.splice(cacheList.begin(), cacheList, cacheMap[key]);
        return cacheMap[key]->second;
    }

    void put(int key, int value) {
        std::lock_guard<std::mutex> lock(cacheMutex);  // Lock entire operation
        
        if (cacheMap.find(key) != cacheMap.end()) {
            cacheList.splice(cacheList.begin(), cacheList, cacheMap[key]);
            cacheMap[key]->second = value;
        } else {
            if (cacheList.size() == capacity) {
                int lastKey = cacheList.back().first;
                cacheMap.erase(lastKey);
                cacheList.pop_back();
            }
            cacheList.push_front({key, value});
            cacheMap[key] = cacheList.begin();
        }
    }
};



#include <unordered_map>
#include <mutex>

class LRUCacheDLL {
    struct Node {
        int key, value;
        Node* prev;
        Node* next;
        Node(int k, int v) : key(k), value(v), prev(nullptr), next(nullptr) {}
    };

    int capacity;
    Node* head;  // Most recently used (sentinel)
    Node* tail;  // Least recently used (sentinel)
    std::unordered_map<int, Node*> cacheMap;
    mutable std::mutex cacheMutex;

    void removeNode(Node* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void insertFront(Node* node) {
        node->next = head->next;
        node->prev = head;
        head->next->prev = node;
        head->next = node;
    }

public:
    LRUCacheDLL(int cap) : capacity(cap) {
        head = new Node(0, 0);  // Sentinel head (MRU side)
        tail = new Node(0, 0);  // Sentinel tail (LRU side)
        head->next = tail;
        tail->prev = head;
    }

    ~LRUCacheDLL() {
        Node* curr = head;
        while (curr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
    }

    int get(int key) {
        std::lock_guard<std::mutex> lock(cacheMutex);

        auto it = cacheMap.find(key);
        if (it == cacheMap.end()) return -1;

        Node* node = it->second;
        removeNode(node);
        insertFront(node);
        return node->value;
    }

    void put(int key, int value) {
        std::lock_guard<std::mutex> lock(cacheMutex);

        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            Node* node = it->second;
            node->value = value;
            removeNode(node);
            insertFront(node);
        } else {
            if ((int)cacheMap.size() == capacity) {
                Node* lru = tail->prev;  // LRU node is just before sentinel tail
                removeNode(lru);
                cacheMap.erase(lru->key);
                delete lru;
            }
            Node* node = new Node(key, value);
            insertFront(node);
            cacheMap[key] = node;
        }
    }
};


/*
 2. Heap Algorithms (in <algorithm>)
 C++ offers several functions that can turn any random-access container (like a std::vector or an array) into a heap. This approach is more flexible as it allows you to manipulate the underlying data directly.
 GeeksforGeeks
 GeeksforGeeks
  +4
 std::make_heap: Rearranges elements in a range to satisfy heap properties (O(N) complexity).
 std::push_heap: Adds a new element to an existing heap (O(log N)).
 std::pop_heap: Moves the largest/smallest element to the end of the range so it can be removed (O(log N)).
 std::is_heap: Checks if a range is a valid heap.
 std::sort_heap: Converts a heap into a sorted range.
 GeeksforGeeks
 GeeksforGeeks
  +4
 */



bool isPalindrome(const std::string& s) {
    int left = 0;
    int right = s.length() - 1;
    
    while (left < right) {
        if (s[left] != s[right]) return false;
        left++;
        right--;
    }
    return true;
}

#include <cctype>
/*
 
 isalnum(c) - alphanumeric (a-z, A-Z, 0-9)
 isalpha(c) - alphabetic only
 isdigit(c) - digit only
 tolower(c) - convert to lowercase
 toupper(c) - convert to uppercase
 */

bool isPalindromeAscii(const std::string& s) {
    int left = 0;
    int right = s.length() - 1;
    
    while (left < right) {
        while (left < right && !isalnum(s[left])) left++;
        while (left < right && !isalnum(s[right])) right--;
        
        if (tolower(s[left]) != tolower(s[right])) return false;
        
        left++;
        right--;
    }
    return true;
}


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
int runTaskMgr() {
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
 In C++, a std::set is a container that stores only the unique elements in a sorted fashion.
 */
#include <iostream>
#include <set>
using namespace std;

int use_set()
{
    // creating a set of integer type
    set<int> st;

    // Inserting values in random order and with duplicates
    // in a set
    st.insert(10);
    st.insert(5);
    st.insert(10);
    st.insert(15);

    // printing the element in a set
    for (auto it : st) {
        cout << it << ' ';
    }
    return 0;
}


// C++ program to demonstrate the use of HashSet container
// uses insert;

#include <iostream>
#include <unordered_set>
using namespace std;

int use_hashset()
{
    // creating a HashSet of integer type
    unordered_set<int> ust;

    // Inserting values in random order and with duplicates
    // in a HashSet
    ust.insert(10);
    ust.insert(5);
    ust.insert(10);
    ust.insert(15);

    // printing the element in a set
    for (auto it : ust) {
        cout << it << ' ';
    }
    return 0;
}


#include <iostream>
#include <stack>
#include <vector>

class Graph {
public:
    Graph(int n) : adj_(n) {}

    void addEdge(int u, int v, bool directed = false) {
        adj_[u].push_back(v);
        if (!directed) {
            adj_[v].push_back(u);
        }
    }

    std::vector<int> dfsIterative(int start) const {
        std::vector<int> order;
        std::vector<bool> visited(adj_.size(), false);
        std::stack<int> s;

        s.push(start);

        while (!s.empty()) {
            int node = s.top();
            s.pop();

            // Mark visited on POP, not on push (see note below)
            if (visited[node]) {
                continue;
            }
            visited[node] = true;
            order.push_back(node);

            // Push neighbors — they'll be processed LIFO
            for (int neighbor : adj_[node]) {
                if (!visited[neighbor]) {
                    s.push(neighbor);
                }
            }
        }

        return order;
    }

    // Recursive version for comparison
    std::vector<int> dfsRecursive(int start) const {
        std::vector<int> order;
        std::vector<bool> visited(adj_.size(), false);
        dfsHelper(start, visited, order);
        return order;
    }

private:
    void dfsHelper(int node, std::vector<bool>& visited, std::vector<int>& order) const {
        visited[node] = true;
        order.push_back(node);
        for (int neighbor : adj_[node]) {
            if (!visited[neighbor]) {
                dfsHelper(neighbor, visited, order);
            }
        }
    }

    std::vector<std::vector<int>> adj_;
};


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

#include <vector>

std::vector<int> spiralTraversal(const std::vector<std::vector<int>>& matrix) {
    std::vector<int> output;

    if (matrix.empty()) return output;

    int rstart = 0, rend = static_cast<int>(matrix.size()) - 1;
    int cstart = 0, cend = static_cast<int>(matrix[0].size()) - 1;
    int direction = 0;

    while (rstart <= rend && cstart <= cend) {
        switch (direction) {
            case 0: // across left to right
                for (int i = cstart; i <= cend; i++)
                    output.push_back(matrix[rstart][i]);
                rstart++;
                break;
            case 1: // down
                for (int i = rstart; i <= rend; i++)
                    output.push_back(matrix[i][cend]);
                cend--;
                break;
            case 2: // across right to left
                for (int i = cend; i >= cstart; i--)
                    output.push_back(matrix[rend][i]);
                rend--;
                break;
            case 3: // up
                for (int i = rend; i >= rstart; i--)
                    output.push_back(matrix[i][cstart]);
                cstart++;
                break;
        }
        direction = (direction + 1) % 4;
    }

    return output;
}

/*
 std::queue<int> is fine for interview code. It wraps std::deque by default.
 If you need more performance, std::vector with head/tail indices can be faster than std::queue due to cache locality, but don't reach for that unless asked.
 For graphs with non-integer node identifiers, use std::unordered_map<NodeType, std::vector<NodeType>> for the adjacency list and std::unordered_set<NodeType> for visited.
 Pass adj_ by const reference in member functions when possible. Interviewers notice const correctness in C++.
 */
#include <iostream>
#include <queue>
#include <vector>
#include <unordered_set>

class GraphBFS {
public:
    GraphBFS(int n) : adj_(n) {}

    void addEdge(int u, int v, bool directed = false) {
        adj_[u].push_back(v);
        if (!directed) {
            adj_[v].push_back(u);
        }
    }

    std::vector<int> bfs(int start) const {
        std::vector<int> order;
        std::vector<bool> visited(adj_.size(), false);
        std::queue<int> q;

        visited[start] = true;
        q.push(start);

        while (!q.empty()) {
            int node = q.front();
            q.pop();
            order.push_back(node);

            for (int neighbor : adj_[node]) {
                if (!visited[neighbor]) {
                    visited[neighbor] = true;  // mark on enqueue, not on dequeue
                    q.push(neighbor);
                }
            }
        }

        return order;
    }

private:
    std::vector<std::vector<int>> adj_;
};

#ifdef LEETCODE_STANDALONE
int main() {
    GraphBFS g(6);
    g.addEdge(0, 1);
    g.addEdge(0, 2);
    g.addEdge(1, 3);
    g.addEdge(2, 4);
    g.addEdge(3, 5);

    auto order = g.bfs(0);
    for (int node : order) {
        std::cout << node << " ";
    }
    std::cout << "\n";  // prints: 0 1 2 3 4 5
}
#endif



#include <iostream>

struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x) : val(x), next(nullptr) {}
};

ListNode* reverseInGroupsOfK(ListNode* head, int k) {
    // 0 and 1 length lists do not require reversing, no matter what k is.
    if (!head || !head->next || k == 1) return head;

    // We'll need two pointers so that we can make changes to
    // pointers without losing our position.
    ListNode* prev = head;
    ListNode* curr = prev->next;

    // This makes sure that the first node will point to null
    // when it is the last node after reversal.
    prev->next = nullptr;

    // Now we'll count out k nodes to reverse.
    ListNode* last = prev;
    int count = 1;
    while (curr && count < k) {
        ListNode* temp = curr->next;
        curr->next = prev;
        prev = curr;
        curr = temp;
        count++;
    }

    // At this point we've reversed up to k nodes. If there
    // is anything left, reverse that and set as the next
    // node of the last of the current set.
    if (curr) {
        last->next = reverseInGroupsOfK(curr, k);
    }

    // Return the first node in this group of k.
    return prev;
}


/* teoplitz matrix*/

#include <vector>

bool isToeplitz(const std::vector<std::vector<int>>& matrix) {
    for (int r = 0; r < matrix.size(); r++) {
        for (int c = 0; c < matrix[r].size(); c++) {
            int value = matrix[r][c];
            if (r > 0 && c > 0 && matrix[r - 1][c - 1] != value) {
                return false;
            }
        }
    }
    return true;
}


/*
// Scratch pad / notes — not compiled into the app

const std::vector<std::vector<int>>& matrix =  {{1, 2, 3, 4}, {8, 12, 16, 15}, {14, 13, 9, 5}, {6, 7, 11, 10}};
unordered_set<int> ust;
// min heap
std::priority_queue<int> maxHeap;
std::priority_queue<int, std::vector<int>, std::greater<int>> minHeap;
std::mutex m;
//std::make_heap
std::lock_guard<std::mutex> lk(m);
std::vector<bool> visited(adj_.size(), false);
std::queue<int> q;
std::string name;
//std::jthread tThread;
 */


#include <vector>

bool isMatrixMonotonic(const std::vector<std::vector<int>>& matrix) {
    // These two loops form a typical row-major traversal
    // over the matrix.
    for (int r = 0; r < matrix.size(); r++) {
        for (int c = 0; c < matrix[r].size(); c++) {
            // Get the value at this location.
            int value = matrix[r][c];

            // If we are on a row past the first, then
            // look up one position and make sure that value
            // is not larger.
            if (r > 0 && matrix[r - 1][c] > value) {
                return false;
            }

            // If we are on a column past the first, then
            // look left one position and make sure that value
            // is not larger.
            if (c > 0 && matrix[r][c - 1] > value) {
                return false;
            }
        }
    }

    // If we didn't find any problems, then return true.
    return true;
}


// max sub array
 int maxSubArray(vector<int>& nums) {
     int maxSoFar = nums[0];
     int maxEndingHere = nums[0];
     
     for (int i = 1; i < nums.size(); i++) {
         maxEndingHere = max(nums[i], maxEndingHere + nums[i]);
         maxSoFar = max(maxSoFar, maxEndingHere);
     }
     
     return maxSoFar;
 }


// need to start in top left or bottom right
 bool findInMonotonic(const vector<vector<int>>& matrix, int k) {
     int nrows = matrix.size();
     if (nrows == 0) return false;
     
     // Start at bottom-left corner
     int row = nrows - 1;
     int col = 0;
     int ncols = matrix[row].size();
     
     while (row >= 0 && col < ncols) {
         int value = matrix[row][col];
         if (value == k) {
             return true;
         } else if (value < k) {
             // k must be to the right
             col++;
         } else {
             // k must be above
             row--;
         }
     }
     
     return false;
 }



int kadanes(vector<int>& array) {
    int maxVal = array[0];
    int curVal = array[0];
    
    for (int i = 1; i < array.size(); i++) {
        curVal = max(array[i], array[i] + curVal);
        maxVal = max(maxVal, curVal);
    }
    return maxVal;
}


bool findDuplicate(vector<int>& arr) {
    std::unordered_set<int> dups;
    
    for (int i = 0; i < arr.size(); i++) {
        if (dups.contains(arr[i])) {
            return true;
        }
        dups.insert(arr[i]);
    }
    return false;
}


bool isPalindromeAnagram(string str) {
    std::unordered_map<char, int> letters;
    int countOdd = 0;
    for (auto letter: str) {
        int val = letters[letter];
        if (val%2 == 0) {
            countOdd++;
        } else {
            countOdd--;
        }
        letters[letter]++;
    }
    if (countOdd<=1) { // allowed 1
        return true;
    }
    return false;
}

bool twoSum(vector<int> arr, int compare) {
    std::unordered_set<int> seen;
    for (auto val: arr) {
        int key = compare -  val;
        if (seen.contains(key)) {
            return true;
        }
        seen.insert(val);
    }
    return false;
}


// sort anagrams

vector<vector<string>> groupAnagrams(vector<string>& strs) {
    unordered_map<string, vector<string>> anagram_map;
    
    for (auto& word : strs) {
        string key = word;
        sort(key.begin(), key.end());
        anagram_map[key].push_back(word);
    }
    
    vector<vector<string>> result;
    for (auto& pair : anagram_map) {
        result.push_back(pair.second);
    }
    return result;
}


class NodeT {
public:
    int value;
    NodeT* left;
    NodeT* right;
    NodeT(int val, NodeT* left, NodeT* right) {
        value = val;
        this->left = left;
        this->right = right;
    }
};

// TREE TO LIST

void tree2list(NodeT* root) {
    std::stack<NodeT*> myStack;
    NodeT* prev = nullptr;
    NodeT* curr = root;

    while (curr || !myStack.empty()) {
        while (curr) {
            myStack.push(curr);
            curr = curr->left;
        }
        curr = myStack.top();
        myStack.pop();
        
        // link
        if (prev) {
            prev->right = curr;
            curr->left = prev;
        }
        prev = curr;
        curr = curr->right;
    }
};

// HOW MANY NUMBERS SMALLER THAN

vector<int> topKFrequent(vector<int> nums, int k) {
    if (k == (int)nums.size()) {
        return nums;
    }

    unordered_map<int, int> map;
    priority_queue<pair<int,int>> que;
    vector<int> result;
    
    // minheap
    //priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> minQue;
    //priority_queue<int, vector<int>, greater<int>>

    for (auto num : nums) {
        map[num]++;
    }

    for (auto key : map) {
        que.push({key.second, key.first}); // {frequency, number}
    }

    int i = 0;
    while (i++ < k) {
        auto res = que.top();
        que.pop();                  // remove after peeking
        result.push_back(res.second); // res.second is the number
    }

    return result;
}
               

class LongestNonDecreasingSubsequence {
public:
   int lengthOfLIS(vector<int>& nums) {
       if (nums.empty()) return 0;
       
       int n = nums.size();
       vector<int> maxLength(n, 1);
       int maximumSoFar = 1;
       
       for (int i = 1; i < n; i++) {
           for (int j = 0; j < i; j++) {
               if (nums[i] > nums[j]) {
                   maxLength[i] = max(maxLength[i], maxLength[j] + 1);
               }
           }
           maximumSoFar = max(maximumSoFar, maxLength[i]);
       }
       
       return maximumSoFar;
   }
};


int BinarySearch(vector<int> data, int k) {
    int start = 0;
    int end = data.size();

    while (start < end) {
        // Look in the middle between the start and end.
        int mid = (start + end) / 2;
        int value = data[mid];
        if (value == k) {
            return mid;
        } else if (value < k) {
            start = mid + 1;
        } else {
            end  = mid;
        }
    }
    return -1;
};
            
               
 int maxProfit(vector<int> prices) {
     int maxP = 0;
     int curP = 0;
     int r = 0;
     int l = 0;
     
     while (r!=prices.size()) {
         if (prices[l] < prices[r]) {
             curP = prices[r] - prices[l];
             maxP = max(maxP, curP);
         }
         else  {
             l = r;
         }
     }
     return maxP;
 }

// kadanes style alt
int maxProfit(vector<int>& prices) {
    int minPrice = prices[0];
    int maxProfit = 0;
    
    for (int i = 1; i < prices.size(); i++) {
        minPrice = min(minPrice, prices[i]);
        maxProfit = max(maxProfit, prices[i] - minPrice);
    }
    return maxProfit;
}




vector<int> sortedSquares(vector<int> input) {
    int l = input.size() -1;
    int r = 0;
    vector<int> output;
    
    while (l<=r) {
        int left = abs(input[l]);
        int right = abs(input[r]);
        if (left > r) {
            output.push_back(left*left);
            l+=1;
        } else {
            output.push_back(right*right);
            r+=1;
        }
    }
    return output;
}



std::vector<int> mergeSortHelper(std::vector<int> leftArr, std::vector<int> rightArr) {
    // Initialize vector to hold our merged elements
    std::vector<int> merged;
    merged.reserve(leftArr.size() + rightArr.size());

    // Indices to track position in each partition (avoids O(n) erase-from-front)
    size_t i = 0, j = 0;

    // Compare each array's values (pairwise, [x] vs. [y])
    while (i < leftArr.size() && j < rightArr.size()) {
        if (leftArr[i] > rightArr[j]) {
            merged.push_back(rightArr[j++]);
        } else {
            merged.push_back(leftArr[i++]);
        }
    }

    // Add straggling elements from left partition
    while (i < leftArr.size()) {
        merged.push_back(leftArr[i++]);
    }

    // Add straggling elements from right partition
    while (j < rightArr.size()) {
        merged.push_back(rightArr[j++]);
    }

    return merged;
}

std::vector<int> mergeSort(std::vector<int> data) {
    // Return if we have a single element (or empty) input
    if (data.size() <= 1) {
        return data;
    }

    // Establish middle index for partitioning
    size_t middleIdx = data.size() / 2;

    // Create left/right partitioned vectors
    std::vector<int> leftArr(data.begin(), data.begin() + middleIdx);
    std::vector<int> rightArr(data.begin() + middleIdx, data.end());

    // Sort left/right partitions recursively
    std::vector<int> leftSorted = mergeSort(leftArr);
    std::vector<int> rightSorted = mergeSort(rightArr);

    // Return merged array
    return mergeSortHelper(leftSorted, rightSorted);
}

void mergeSort(std::vector<int>& data) {
    // Merge two already-sorted sub-sections of data:
    //   [start, mid) and [mid, end)
    auto merge = [&](size_t start, size_t mid, size_t end) {
        // Accumulate sorted values here before copying back into place
        std::vector<int> temp;
        temp.reserve(end - start);

        size_t left = start;
        size_t right = mid;

        // While either side still has data to merge
        while (left < mid || right < end) {
            // Choose the smallest available
            if (right == end || (left < mid && data[left] <= data[right])) {
                temp.push_back(data[left++]);
            } else {
                temp.push_back(data[right++]);
            }
        }

        // Copy the sorted values back into place
        for (size_t i = 0; i < temp.size(); i++) {
            data[start + i] = temp[i];
        }
    };

    // Core recursive helper. std::function lets the lambda reference itself.
    std::function<void(size_t, size_t)> mergeSortRecursive = [&](size_t start, size_t end) {
        // Fewer than two values — nothing to do
        if (start + 2 > end) return;

        // Find midpoint and recursively sort each half
        size_t mid = (start + end) / 2;
        mergeSortRecursive(start, mid);
        mergeSortRecursive(mid, end);

        // Merge the sorted sub-sections
        merge(start, mid, end);
    };

    mergeSortRecursive(0, data.size());
}


struct TreeNode {
     int value;
     TreeNode* left;
     TreeNode* right;
     TreeNode(int v, TreeNode* l = nullptr, TreeNode* r = nullptr)
         : value(v), left(l), right(r) {}
};

int treeHeight(TreeNode* root) {
    // Empty tree has height 0 (or -1 depending on convention —
    // this matches "number of nodes on longest root-to-leaf path")
    if (!root) return 0;

    return 1 + std::max(treeHeight(root->left), treeHeight(root->right));
}

struct LLNode {
    int value;
    LLNode* next;
    LLNode* sublist;

    LLNode(int v, LLNode* n = nullptr, LLNode* s = nullptr)
        : value(v), next(n), sublist(s) {}
};

std::vector<int> flattenSublist(LLNode* inputList) {
    std::vector<int> output;
    if (!inputList) return output;

    std::stack<LLNode*> stk;
    stk.push(inputList);

    while (!stk.empty()) {
        LLNode* node = stk.top();
        stk.pop();
        if (!node) continue;

        output.push_back(node->value);

        // Push next first so sublist is processed first (LIFO)
        if (node->next)    stk.push(node->next);
        if (node->sublist) stk.push(node->sublist);
    }

    return output;
}


vector<int> productExceptSelf(vector<int> nums) {
    vector<int> result;
    int pre=1;
    int post=1;
    for (int i=0; i<nums.size(); i++) {
        result[i] = pre;
        pre = nums[i] * pre;
    }
    for (int i=(nums.size()-1); i>=0; i--) {
        result[i] = result[i] * post;
        post = nums[i] * post;
    }
    return result;
};


std::vector<std::pair<int, int>> mazeSolver(const std::vector<std::vector<char>>& maze) {
    std::vector<std::pair<int, int>> path;
    std::unordered_set<long long> visited;

    // Directions: right, left, up, down (matching original)
    const std::vector<std::pair<int, int>> directions = {
        {1, 0}, {-1, 0}, {0, -1}, {0, 1}
    };

    if (maze.empty() || maze[0].empty()) return path;

    const int rows = maze.size();
    const int cols = maze[0].size();

    // Pack (x, y) into a single key for the visited set
    auto key = [cols](int x, int y) -> long long {
        return static_cast<long long>(x) * 100000LL + y;
    };

    auto outOfBounds = [&](int x, int y) {
        return x < 0 || x >= cols || y < 0 || y >= rows;
    };

    // Recursive lambda via generic self-reference trick
    auto makeNextMoveFrom = [&](auto& self, int x, int y) -> bool {
        if (outOfBounds(x, y) || visited.count(key(x, y))) {
            return false;
        }

        // NOTE: original indexes maze[x][y] — preserving that convention.
        // If maze is row-major (maze[row][col]), this is actually maze[x=col?][y=row?].
        // The original JS has the same ambiguity; keeping behavior identical.
        char cell = maze[x][y];

        if (cell == 'G') return true;
        if (cell == '_') return false;

        path.push_back({x, y});
        visited.insert(key(x, y));

        for (const auto& [dx, dy] : directions) {
            if (self(self, x + dx, y + dy)) {
                return true;
            }
        }

        path.pop_back();
        return false;
    };

    makeNextMoveFrom(makeNextMoveFrom, 0, 0);
    return path;
}


    
    
#include <algorithm>
#include <vector>
using namespace std;

vector<vector<char>> generatePassword(vector<char> letters, int minLen, int maxLen) {
    vector<vector<char>> ret;
    sort(letters.begin(), letters.end());

    for (int len = minLen; len <= maxLen; len++) {
        if (len > (int)letters.size()) break;

        // Generate all permutations of size `len`:
        // pick each combination of `len` letters, then permute it
        vector<bool> mask(letters.size(), false);
        fill(mask.end() - len, mask.end(), true);

        do {
            vector<char> combo;
            for (int i = 0; i < (int)letters.size(); i++) {
                if (mask[i]) combo.push_back(letters[i]);
            }
            // Now permute this combination
            sort(combo.begin(), combo.end());
            do {
                ret.push_back(combo);
            } while (next_permutation(combo.begin(), combo.end()));
        } while (next_permutation(mask.begin(), mask.end()));
    }

    return ret;
}
    


#include <vector>
#include <string>
#include <unordered_set>
using namespace std;

class NQueens {
    vector<vector<string>> result;
    vector<int> queenRow;  // queenRow[col] = row where queen is placed
    unordered_set<int> rows, diag1, diag2;
    int n;

    void backtrack(int col) {
        if (col == n) {
            // All columns placed — build the board representation
            vector<string> board(n, string(n, '.'));
            for (int c = 0; c < n; c++) {
                board[queenRow[c]][c] = 'Q';
            }
            result.push_back(board);
            return;
        }

        for (int row = 0; row < n; row++) {
            // Pruning: skip if this row or either diagonal is taken
            if (rows.count(row)) continue;
            if (diag1.count(row - col)) continue;
            if (diag2.count(row + col)) continue;

            // Place queen
            queenRow[col] = row;
            rows.insert(row);
            diag1.insert(row - col);
            diag2.insert(row + col);

            backtrack(col + 1);

            // Undo — the backtracking step
            rows.erase(row);
            diag1.erase(row - col);
            diag2.erase(row + col);
        }
    }

public:
    vector<vector<string>> solveNQueens(int N) {
        n = N;
        queenRow.resize(n);
        backtrack(0);
        return result;
    }
};



#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

class AddOneCharacter {
    vector<string> passwords;
    vector<char> stack;
    unordered_map<char, int> counts;
    int minLength, maxLength;

    void addOneCharacter(const vector<char>& characters) {
        // Record if current length is in valid range
        if ((int)stack.size() >= minLength && (int)stack.size() <= maxLength) {
            passwords.emplace_back(stack.begin(), stack.end());
        }

        // Base case: hit max length, stop descending
        if ((int)stack.size() == maxLength) return;

        for (char c : characters) {
            int count = counts[c];  // default-constructs to 0 if absent

            // Two constraints:
            //   1. Not the same as the last character placed
            //   2. Used fewer than 2 times so far
            bool notConsecutive = stack.empty() || stack.back() != c;
            bool underLimit = count < 2;

            if (notConsecutive && underLimit) {
                stack.push_back(c);
                counts[c]++;

                addOneCharacter(characters);

                // Undo
                counts[c]--;
                stack.pop_back();
            }
        }
    }

public:
    vector<string> generatePassword(vector<char> characters, int minLen, int maxLen) {
        minLength = minLen;
        maxLength = maxLen;
        addOneCharacter(characters);
        return passwords;
    }
};


/*
 If you can reframe the problem as "at each step I pick one thing from a set of options, and my future choices depend on what I picked," you're looking at backtracking.
 Concrete keyword/shape triggers:

 "Generate all..." — all permutations, all combinations, all subsets, all valid strings, all paths, all arrangements. The plural output is a dead giveaway.
 "Find a valid..." — a valid sudoku, a valid word break, a valid placement of N queens, a valid coloring. Single output but requires trying assignments and undoing them.
 Constraints that can be checked incrementally — "no two queens on the same diagonal," "parentheses must balance," "sum must equal target." You can reject a partial solution before completing it, which is what makes backtracking efficient vs. brute force enumeration.
 Problem involves a small-ish decision tree — at each step you have a handful of choices (place a queen, pick a letter, include/exclude an element), and the tree depth is bounded.
 "With/without replacement," "in any order," "subject to constraint..." — phrasing that implies exploring an arrangement space.

 Canonical backtracking problem families:

 Permutations — LC 46, 47 (order matters, use each once)
 Combinations — LC 77, 39, 40 (order doesn't matter, subset of input)
 Subsets / Power set — LC 78, 90 (include or exclude each element)
 Partitioning — LC 131 (palindrome partition), 93 (restore IP addresses)
 Grid search — LC 79 (word search), 212 (word search II), 51 (N-Queens), 37 (Sudoku)
 Combinatorial search — LC 22 (generate parentheses), 17 (letter combinations of phone)
 Path enumeration — all paths from A to B with constraints
 */

    
#include <vector>
#include <unordered_set>
using namespace std;

class BacktrackingPermutations {
    vector<vector<char>> ret;
    vector<char> current;
    unordered_set<char> used;

    void backtrack(const vector<char>& letters, int minLen, int maxLen) {
        if ((int)current.size() >= minLen) {
            ret.push_back(current);
        }
        if ((int)current.size() == maxLen) return;

        for (char c : letters) {
            if (used.count(c)) continue;

            current.push_back(c);
            used.insert(c);

            backtrack(letters, minLen, maxLen);

            current.pop_back();
            used.erase(c);
        }
    }

public:
    vector<vector<char>> generatePassword(vector<char> letters, int minLen, int maxLen) {
        if ((int)letters.size() < minLen) return ret;
        backtrack(letters, minLen, maxLen);
        return ret;
    }
};
    
#include <vector>
#include <string>
#include <unordered_set>
using namespace std;

class Solution {
    vector<vector<string>> finalBoard;
    unordered_set<int> cols, diagonals, antiDiagonals;
    vector<string> board;
    int n;

    void backtrack(int row) {
        if (row == n) {
            finalBoard.push_back(board);
            return;
        }

        for (int col = 0; col < n; col++) {
            int diag = row - col;
            int antiDiag = row + col;

            if (cols.count(col) || diagonals.count(diag) || antiDiagonals.count(antiDiag))
                continue;

            // Place
            cols.insert(col);
            diagonals.insert(diag);
            antiDiagonals.insert(antiDiag);
            board[row][col] = 'Q';

            backtrack(row + 1);

            // Remove
            cols.erase(col);
            diagonals.erase(diag);
            antiDiagonals.erase(antiDiag);
            board[row][col] = '.';
        }
    }

public:
    vector<vector<string>> solveNQueens(int N) {
        n = N;
        board = vector<string>(n, string(n, '.'));
        backtrack(0);
        return finalBoard;
    }
};
/*
 
 recursive_func(state) {
     if (terminal condition) { record or return }
     for (each choice) {
         if (invalid) continue          // pruning
         save(choice)                   // modify state
         recursive_func(state)          // descend
         unsave(choice)                 // undo
     }
 }
 
 For N ≤ 32 (or 64), replace the three unordered_set<int> with three int (or long) bitmasks. Set operations become bitwise OR/AND, conflict check is one bitwise AND. This is ~10-20x faster in practice:*/
*/
void backtrack(int col, int rowMask, int d1Mask, int d2Mask) {
     if (col == n) { return; }

     int available = ((1 << n) - 1) & ~(rowMask | d1Mask | d2Mask);
     while (available) {
         int pick = available & -available;  // lowest set bit
         available ^= pick;                   // remove it
         int row = __builtin_ctz(pick);       // bit index → row number

         queenRow[col] = row;
         backtrack(col + 1,
                   rowMask | pick,
                   (d1Mask | pick) << 1,    // shift diagonals for next col
                   (d2Mask | pick) >> 1);
     }
 }



#ifdef LEETCODE_STANDALONE
//Drill these tonight if you have more coding rounds coming:

Kadane's (max subarray)
Two pointers (container with most water, 3sum)
Sliding window (longest substring without repeating chars)
Prefix sum (subarray sum equals k)
Stack monotonic (largest rectangle in histogra
Binary search variations
Hash map patterns (two sum, subarray sum)
                 
                 //Same pattern as Kadane's:

 Track running optimum (minPrice)
 Update global optimum (maxProfit)
 Single pass, O(n)
 The Family
 These are all variations of the same greedy pattern:

 Max subarray (Kadane's)
 Max product subarray
 Buy/sell stock
 House robber
 Once you internalize Kadane's, these all click.

 Good catch - you're seeing the pattern connections.




                 

 Kadane's variants (1-2 hours)

 Max subarray sum
 Max product subarray
 Best time to buy/sell stock


 Sliding window (1-2 hours)

 Longest substring without repeating
 Minimum window substring
 Max consecutive ones


 Hash map patterns (1-2 hours)

 Two sum
 Subarray sum equals k
 Longest consecutive sequence

#endif // LEETCODE_STANDALONE

               
               
               
               
               
bool isSudokuValid(vector<vector<int>> sudoku) {
     
 };

//monotonic stack problem
//               Sequence of choices? No — each output is determined by the input
//               Choices affect future options? No
//               Need to explore multiple branches? No
               

#include <vector>
#include <stack>
using namespace std;

vector<int> daysToHigherTemp(vector<int>& dailyHighs) {
    int n = dailyHighs.size();
    vector<int> result(n, 0);
    stack<int> waiting;  // indices of days still waiting for a hotter day

    for (int i = 0; i < n; i++) {
        // Today resolves any waiting day cooler than today
        while (!waiting.empty() && dailyHighs[waiting.top()] < dailyHighs[i]) {
            int prevIdx = waiting.top();
            waiting.pop();
            result[prevIdx] = i - prevIdx;  // distance forward
        }
        waiting.push(i);
    }

    // Anything left on the stack never found a hotter day — result stays 0
    return result;
 };

               
               
               
vector<int> daysToHigherTemp(vector<int>& dailyHighs) {
   int n = dailyHighs.size();
   vector<int> output(n, 0);

   for (int day = 0; day < n; day++) {
       int highTemp = dailyHighs[day];

       // Look forward for a hotter day
       for (int i = day + 1; i < n; i++) {
           if (dailyHighs[i] > highTemp) {
               output[day] = i - day;
               break;
           }
       }
       // If no hotter day found, output[day] stays 0 (from initialization)
   }

   return output;
 };

               
               
               
               class ParseCodes {
     vector<string, int> codes;
     vector<char> queue;
     int maxlen = 0;
     ParseCodes(unordered_map<string, int> input) {
         codes=input;
         for (auto pair: input) {
             if (pair.first.size()>maxlen) {
                 maxlen =pair.first.size();
             }
         }
     }
     virtual ~ParseCodes() {
         
     }
     int encode(string str) {
         if (codes.contains(str)) {
             return codes[str];
         }
     }
     int encode(char s) {
         queue.push_back(s);
         string str(queue.data(), queue.size());
         if (codes.contains(str)) {
             queue.clear();
             return codes[str];
         } else {
             if (queue.size()>maxlen) {
                 queue.top();
                 return -1;
             }
         }
         
     }
     
     
     int encode2(char s) {
         queue.push_back(s);
         string str(queue.begin(), queue.end());
         
         if (codes.count(str)) {
             int val = codes[str];
             queue.clear();
             return val;
         }
         
         // Check if `str` is a prefix of any code
         bool isPrefix = false;
         for (const auto& [code, _] : codes) {
             if (code.size() > str.size() &&
                 code.compare(0, str.size(), str) == 0) {
                 isPrefix = true;
                 break;
             }
         }
         
         if (!isPrefix) {
             queue.clear();
             return -2;  // invalid stream
         }
         
         return -1;  // still buffering
     }
 };
               

vector<vector<int>> kClosest(vector<vector<int>>& points, int k) {
   // Max-heap ordered by squared distance
   // pair: {squared_distance, index_into_points}
   priority_queue<pair<int, int>> maxHeap;

   for (int i = 0; i < (int)points.size(); i++) {
       int x = points[i][0], y = points[i][1];
       int distSq = x * x + y * y;

       if ((int)maxHeap.size() < k) {
           maxHeap.push({distSq, i});
       } else if (distSq < maxHeap.top().first) {
           maxHeap.pop();
           maxHeap.push({distSq, i});
       }
   }

   vector<vector<int>> result;
   result.reserve(k);
   while (!maxHeap.empty()) {
       result.push_back(points[maxHeap.top().second]);
       maxHeap.pop();
   }
   return result;
}












#include <queue>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
using namespace std;

struct Node {
    int freq;
    char ch;              // only meaningful for leaves
    Node* left;
    Node* right;

    Node(int f, char c) : freq(f), ch(c), left(nullptr), right(nullptr) {}
    Node(int f, Node* l, Node* r) : freq(f), ch('\0'), left(l), right(r) {}

    bool isLeaf() const { return !left && !right; }
};

struct Compare {
    bool operator()(Node* a, Node* b) {
        return a->freq > b->freq;  // min-heap: smallest freq on top
    }
};

Node* buildHuffman(const unordered_map<char, int>& freqs) {
    priority_queue<Node*, vector<Node*>, Compare> pq;

    // Seed the heap with one leaf per character
    for (auto& [ch, f] : freqs) {
        pq.push(new Node(f, ch));
    }

    // Edge case: single unique character
    if (pq.size() == 1) {
        Node* only = pq.top(); pq.pop();
        return new Node(only->freq, only, nullptr);
    }

    // Merge two smallest until one tree remains
    while (pq.size() > 1) {
        Node* a = pq.top(); pq.pop();
        Node* b = pq.top(); pq.pop();
        pq.push(new Node(a->freq + b->freq, a, b));
    }

    return pq.top();
}

// Walk the tree to extract codes
void generateCodes(Node* node, const string& path, unordered_map<char, string>& codes) {
    if (!node) return;
    if (node->isLeaf()) {
        codes[node->ch] = path.empty() ? "0" : path;  // single-char edge case
        return;
    }
    generateCodes(node->left, path + "0", codes);
    generateCodes(node->right, path + "1", codes);
}

unordered_map<char, string> huffmanCodes(const unordered_map<char, int>& freqs) {
    Node* root = buildHuffman(freqs);
    unordered_map<char, string> codes;
    generateCodes(root, "", codes);
    return codes;
}
