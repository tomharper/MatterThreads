//
//  Leetcode.swift
//  MatterAI
//
//  Created by Tom Harper on 4/10/26.
//

import Foundation



// Tasks form a tree - child tasks are automatically cancelled when parent is
func fetchData() async throws -> Data {
    async let user = fetchUser()        // Child task 1
    async let posts = fetchPosts()      // Child task 2
    
    return try await combine(user, posts)  // Waits for both
}

// TaskGroup for dynamic parallelism
func processAll() async throws -> [Result] {
    try await withThrowingTaskGroup(of: Result.self) { group in
        for item in items {
            group.addTask { try await process(item) }
        }
        
        var results: [Result] = []
        for try await result in group {
            results.append(result)
        }
        return results
    }
}

// C++ function (synchronous)
@_cdecl("processData")
func processData(_ ptr: UnsafePointer<UInt8>, _ len: Int) -> Int {
    // This runs on whatever thread calls it
    // Cannot be 'async' - C++ has no concept of Swift tasks
}

// Wrapping C++ in Swift async
actor CppWrapper {
    private let queue = DispatchQueue(label: "cpp-work")
    
    func callCppAsync() async -> Int {
        await withCheckedContinuation { continuation in
            queue.async {
                let result = cppFunction()  // Synchronous C++ call
                continuation.resume(returning: result)
            }
        }
    }
}

/*
 Critical constraints:

 C++ functions called from Swift are always synchronous from Swift's perspective
 No direct actor/async support across the boundary
 Must manually bridge with DispatchQueue or Task.detached
 C++ threading primitives (std::thread, std::mutex) are invisible to Swift's cooperative scheduler
 */



///Combine as C++/Swift Bridge
import Combine

// C++ audio callback (pseudocode)
class AudioProcessor {
    var callback: ((AudioBuffer) -> Void)?
    
    func start() {
        // C++ processing thread calls callback repeatedly
    }
}

// Wrap C++ in Combine Publisher
class AudioPublisher: Publisher {
    typealias Output = AudioBuffer
    typealias Failure = Never
    
    private let processor = AudioProcessor()
    
    func receive<S>(subscriber: S) where S: Subscriber,
                    S.Input == AudioBuffer, S.Failure == Never {
        let subscription = AudioSubscription(subscriber: subscriber,
                                              processor: processor)
        subscriber.receive(subscription: subscription)
    }
}

class AudioSubscription<S: Subscriber>: Subscription
    where S.Input == AudioBuffer, S.Failure == Never {
    
    private var subscriber: S?
    private let processor: AudioProcessor
    
    init(subscriber: S, processor: AudioProcessor) {
        self.subscriber = subscriber
        self.processor = processor
        
        // Bridge C++ callback to Combine
        processor.callback = { [weak self] buffer in
            _ = self?.subscriber?.receive(buffer)
        }
    }
    
    func request(_ demand: Subscribers.Demand) {
        processor.start()
    }
    
    func cancel() {
        processor.callback = nil
        subscriber = nil
    }
}


//Simpler Pattern: PassthroughSubject
//For one-off C++ callbacks:
class CppBridge {
    private let subject = PassthroughSubject<Data, Error>()
    
    var publisher: AnyPublisher<Data, Error> {
        subject.eraseToAnyPublisher()
    }
    
    func callCppAsync() {
        DispatchQueue.global().async {
            do {
                let result = cppSyncFunction()  // C++ call
                self.subject.send(result)
            } catch {
                self.subject.send(completion: .failure(error))
            }
        }
    }
}

// Usage
let bridge = CppBridge()
bridge.publisher
    .receive(on: DispatchQueue.main)
    .sink(
        receiveCompletion: { completion in
            // Handle error/completion
        },
        receiveValue: { data in
            // Got data from C++
        }
    )
    .store(in: &cancellables)






actor LRUCache {
    private let capacity: Int
    private var cache: [String: Node] = [:]
    private var head: Node?
    private var tail: Node?
    
    class Node {
        let key: String
        var value: Data
        var prev: Node?
        var next: Node?
        
        init(key: String, value: Data) {
            self.key = key
            self.value = value
        }
    }
    
    init(capacity: Int) {
        self.capacity = capacity
    }
    
    func get(_ key: String) -> Data? {
        guard let node = cache[key] else { return nil }
        
        moveToFront(node)
        return node.value
    }
    
    func set(_ key: String, _ value: Data) {
        if let node = cache[key] {
            node.value = value
            moveToFront(node)
        } else {
            let newNode = Node(key: key, value: value)
            cache[key] = newNode
            addToFront(newNode)
            
            if cache.count > capacity {
                removeLRU()
            }
        }
    }
    
    private func moveToFront(_ node: Node) {
        remove(node)
        addToFront(node)
    }
    
    private func addToFront(_ node: Node) {
        node.next = head
        node.prev = nil
        head?.prev = node
        head = node
        if tail == nil { tail = node }
    }
    
    private func remove(_ node: Node) {
        node.prev?.next = node.next
        node.next?.prev = node.prev
        if head === node { head = node.next }
        if tail === node { tail = node.prev }
    }
    
    private func removeLRU() {
        guard let lru = tail else { return }
        cache.removeValue(forKey: lru.key)
        remove(lru)
    }
}





// C++ → Queue → Swift consumer
actor QueueBridge {
    private var buffers: [AudioBuffer] = []
    
    func enqueue(_ buffer: AudioBuffer) {  // Called from C++
        buffers.append(buffer)
        if buffers.count > 100 {
            buffers.removeFirst()  // Manual backpressure: drop old
        }
    }
    
    func dequeue() async -> AudioBuffer? {
        guard !buffers.isEmpty else { return nil }
        return buffers.removeFirst()
    }
}

// Consumer
while let buffer = await bridge.dequeue() {
    process(buffer)
}


//Or Simple Blocking Queue
//cpp// C++ side - thread-safe queue
class MessageQueue {
    std::queue<AudioBuffer> queue;
    std::mutex mutex;
    std::condition_variable cv;
    
    void push(AudioBuffer buffer) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(buffer);
        cv.notify_one();
    }
    
    AudioBuffer pop() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !queue.empty(); });
        auto buffer = queue.front();
        queue.pop();
        return buffer;
    }
};

//Combine is MORE than GCD, but uses GCD underneath via Schedulers.


func maxSubArray(_ nums: [Int]) -> Int {
    var maxSoFar = nums[0]
    var maxEndingHere = nums[0]
    for i in 1..< nums.count {
    //for num in nums.dropFirst() {
    //for num in nums where num > 10 {
    //for index in nums.indices
        maxEndingHere = max(nums[i], maxEndingHere + nums[i])
        maxSoFar = max(maxSoFar, maxEndingHere)
    }
    return maxSoFar
}


func findDuplicate(_ nums: [Int]) -> Boolean {
    var dups = Set<Int>()
    
    for num in nums {
        if (dups.contains(num)) {
            return true
        }
        dups.insert(num)
    }
    return false
}

func isPalindromeAnagram(_ str: String) -> Bool {
    var letters: [Character: Int] = [:]
    var countOdd: Int = 0
    for letter in str {
        let count = letters[letter, default: 0]
        letters[letter] = count + 1
        if (count.isMultiple(of: 2)) {
            countOdd += 1
        } else {
            countOdd -= 1
        }
    }
}

/*
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
 */

func twoSum(_ arr: [Int], _ compare: Int) -> Bool {
    
    var seen: Set<Int> = []
    
    for val in arr {
        val key = compare -  val
        if (seen.contains(key)) {
            return true
        }
        seen.insert(val);
    }
    return false;
}

func groupAnagrams(_ strs: [String]) [[String]]{
    var anagram_map: Dictionary[String, [String]]
    
    for (word) in strs {
        sort(anagram_map.keys)
        anagram_map[word].push_back(word)
    }
    
    var result: [[String]] = []
    for (pair) in anagram_map {
        result
    }
}

/// group anagrams
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
    NodeT left;
    NodeT right;
    NodeT(int val, NodeT left, NodeT right) {
        value = val;
        this->left = left;
        this->right = right;
    }
};




func tree2list(_ root: NodeT?) {
    var stack = [NodeT]()
    var prev: NodeT? = nil
    var curr: NodeT? = root

    while curr != nil || !stack.isEmpty {
        while let node = curr {
            stack.append(node)
            curr = node.left
        }
        curr = stack.removeLast()
        
        // link
        if let p = prev {
            p.right = curr
            curr?.left = p
        }
        prev = curr
        curr = curr?.right
    }
}


func BinarySearch(_ data: [Int], _ k: Int) {
    var start: Int = 0
    var end: Int = data.count - 1
    
    while start < end {
        let mid: Int = (start + end) / 2
        let value: Int = data[mid]
        if (value == k) {
            return mid;
        } else if (value < k) {
            start = mid + 1
        } else {
            end = mid
        }
    }
    return -1
}

func sortedSquares(_ input: [Int]) -> [Int] {
    var l = input.count - 1
    var r = 0
    let n: Int = input.count
    var output: [Int] = []
    
    while (l<=r) {
        let left: Int = abs(input[l])
        let right: Int = abs(input[r])
        if (left > right) {
            output.append(left*left)
            l+=1
        } else {
            output.append(right*right)
        }
    }
    return output
}


actor EventBus {
    private var handlers: [ObjectIdentifier: [(Any) async -> Void]] = [:]

    func subscribe<E>(_ eventType: E.Type, handler: @escaping (E) async -> Void) {
        let key = ObjectIdentifier(eventType)
        handlers[key, default: []].append { event in
            if let e = event as? E { await handler(e) }
        }
    }

    func publish<E>(_ event: E) async {
        let key = ObjectIdentifier(E.self)
        guard let list = handlers[key] else { return }
        for h in list {
            await h(event)
        }
    }
}
