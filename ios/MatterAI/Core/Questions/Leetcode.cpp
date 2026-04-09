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
