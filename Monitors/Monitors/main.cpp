#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

struct Payload {
    int id;
};

class EventMonitor {
public:

    void send(Payload* data) {

        std::unique_lock<std::mutex> lock(m_);

        cv_.wait(lock, [this]() { return !hasEvent_; });

        payload_ = data;
        ++eventCounter_;
        hasEvent_ = true;

        std::cout << "Producer: send event #" << eventCounter_ << std::endl;

        cv_.notify_one();
    }


    Payload* wait() {

        std::unique_lock<std::mutex> lock(m_);

        cv_.wait(lock, [this]() { return hasEvent_ || stop_; });

        if (stop_ && !hasEvent_) {
            return nullptr;
        }

        Payload* result = payload_;
        hasEvent_ = false;

        std::cout << "Consumer: receive event #" << eventCounter_ << std::endl;

        cv_.notify_one();

        return result;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(m_);
        stop_ = true;
        cv_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable cv_;

    bool hasEvent_ = false;
    bool stop_ = false;
    Payload* payload_ = nullptr;
    int eventCounter_ = 0;
};

int main() {

    EventMonitor monitor;

    const int TOTAL_EVENTS = 60;

    std::thread producer( [&monitor]() {
        for (int i = 1; i <= TOTAL_EVENTS; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            Payload* p = new Payload{ i };
            monitor.send(p);
        }

        monitor.stop();
        });

    std::thread consumer([&monitor]() {
        while (true) {
            Payload* p = monitor.wait();
            if (!p) {
                break;
            }

            std::cout << "Consumer: event processing, id = " << p->id << std::endl;

            delete p;
        }
        });

    producer.join();
    consumer.join();

    return 0;
}
