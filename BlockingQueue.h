#pragma once

class InterruptedException : public std::runtime_error {
public:
    explicit InterruptedException() : std::runtime_error("InterruptedException") {}
};

template <typename T>
class BlockingQueue {
public:
    BlockingQueue() : mInterrupted(false) {}

    T pop() {
        std::unique_lock<std::mutex> lock(mMutex);
        while (mQueue.empty()) {
            mCondVar.wait(lock);
            if (mInterrupted) {
                throw InterruptedException();
            }
        }
        auto t = mQueue.front();
        mQueue.pop();
        return t;
    }

    void push(const T& t) {
        std::unique_lock<std::mutex> lock(mMutex);
        mQueue.push(t);
        lock.unlock();
        mCondVar.notify_one();
    }

    void interrupt() {
        std::unique_lock<std::mutex> lock(mMutex);
        mInterrupted = true;
        mCondVar.notify_all();
    }

private:
    std::mutex mMutex;
    std::condition_variable mCondVar;
    std::queue<T> mQueue;
    bool mInterrupted;
};
