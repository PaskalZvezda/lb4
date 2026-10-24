#include <cstddef>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

class Arena {
public:
    explicit Arena(std::size_t chunk_bytes) : chunk_bytes_(chunk_bytes) {}

    void *allocate(std::size_t bytes) {
        bytes = align(bytes);
        auto &free = free_[bytes];
        if (!free.empty()) {
            void *p = free.back();
            free.pop_back();
            return p;
        }
        if (offset_ + bytes > capacity_) {
            capacity_ = bytes > chunk_bytes_ ? bytes : chunk_bytes_;
            chunks_.emplace_back(new char[capacity_]);
            offset_ = 0;
        }
        offset_ += bytes;
        return chunks_.back().get() + offset_ - bytes;
    }

    void deallocate(void *p, std::size_t bytes) { free_[align(bytes)].push_back(p); }

private:
    static std::size_t align(std::size_t bytes) {
        const std::size_t a = alignof(std::max_align_t);
        return (bytes + a - 1) / a * a;
    }

    std::size_t chunk_bytes_;
    std::vector<std::unique_ptr<char[]>> chunks_;
    std::size_t capacity_ = 0;
    std::size_t offset_ = 0;
    std::unordered_map<std::size_t, std::vector<void *>> free_;
};

template<typename T, std::size_t N>
class PoolAllocator {
public:
    using value_type = T;
    template<typename U>
    struct rebind {
        using other = PoolAllocator<U, N>;
    };

    PoolAllocator() : arena_(std::make_shared<Arena>(N * sizeof(T))) {}
    template<typename U>
    PoolAllocator(const PoolAllocator<U, N> &other) noexcept : arena_(other.arena_) {}

    T *allocate(std::size_t n) { return static_cast<T *>(arena_->allocate(n * sizeof(T))); }
    void deallocate(T *p, std::size_t n) { arena_->deallocate(p, n * sizeof(T)); }

    template<typename U>
    bool operator==(const PoolAllocator<U, N> &other) const { return arena_ == other.arena_; }
    template<typename U>
    bool operator!=(const PoolAllocator<U, N> &other) const { return arena_ != other.arena_; }

private:
    template<typename U, std::size_t M>
    friend class PoolAllocator;
    std::shared_ptr<Arena> arena_;
};

template<typename T, typename Allocator = std::allocator<T>>
class MyList {
    struct Node {
        T value;
        Node *next;
    };
    using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using NodeTraits = std::allocator_traits<NodeAllocator>;

public:
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        explicit Iterator(Node *node) : node_(node) {}
        T &operator*() const { return node_->value; }
        T *operator->() const { return &node_->value; }
        Iterator &operator++() { node_ = node_->next; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; node_ = node_->next; return tmp; }
        bool operator==(const Iterator &other) const { return node_ == other.node_; }
        bool operator!=(const Iterator &other) const { return node_ != other.node_; }

    private:
        Node *node_;
    };

    MyList() = default;
    MyList(const MyList &) = delete;
    MyList &operator=(const MyList &) = delete;

    ~MyList() {
        while (head_) {
            Node *next = head_->next;
            NodeTraits::destroy(alloc_, head_);
            NodeTraits::deallocate(alloc_, head_, 1);
            head_ = next;
        }
    }

    void push_back(const T &value) {
        Node *node = NodeTraits::allocate(alloc_, 1);
        NodeTraits::construct(alloc_, node, Node{value, nullptr});
        (tail_ ? tail_->next : head_) = node;
        tail_ = node;
        ++size_;
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    Iterator begin() const { return Iterator(head_); }
    Iterator end() const { return Iterator(nullptr); }

private:
    NodeAllocator alloc_;
    Node *head_ = nullptr;
    Node *tail_ = nullptr;
    std::size_t size_ = 0;
};

int factorial(int n) {
    return n < 2 ? 1 : n * factorial(n - 1);
}

int main() {
    std::map<int, int> std_map;
    std::map<int, int, std::less<int>, PoolAllocator<std::pair<const int, int>, 10>> pool_map;
    for (int i = 0; i < 10; ++i) {
        std_map[i] = factorial(i);
        pool_map[i] = factorial(i);
    }
    for (const auto &item : pool_map)
        std::cout << item.first << " " << item.second << std::endl;

    MyList<int> std_list;
    MyList<int, PoolAllocator<int, 10>> pool_list;
    for (int i = 0; i < 10; ++i) {
        std_list.push_back(i);
        pool_list.push_back(i);
    }
    for (const auto &value : pool_list)
        std::cout << value << std::endl;
    return 0;
}
