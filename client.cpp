#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>
#include <cctype>

template<typename T>
class BlockingQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cond_var;
    bool finished = false; 
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(item);
        cond_var.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex);
        cond_var.wait(lock, [this]() { return !queue.empty() || finished; });

        if (queue.empty()) return false;
        item = queue.front();
        queue.pop();
        return true;
    }

    void set_finished() {
        std::lock_guard<std::mutex> lock(mutex);
        finished = true;
        cond_var.notify_all();
    }

    bool is_finished() const {
        std::lock_guard<std::mutex> lock(mutex);
        return finished && queue.empty();
    }
};

void producer(BlockingQueue<std::string>& queue, const std::string& filename) {
    std::ifstream infile(filename);
    std::string line;

    if (!infile.is_open()) {
        std::cerr << "Error opening input file: " << filename << std::endl;
        return; 
    }

    while (std::getline(infile, line)) {
        queue.push(line);
    }
    queue.set_finished();
    infile.close(); 
}

void consumer(BlockingQueue<std::string>& queue, std::map<char, std::vector<std::string>>& results, std::mutex& results_mutex) {
    std::string line;
    while (queue.pop(line)) {
        char initial = toupper(line[0]);
        std::lock_guard<std::mutex> lock(results_mutex);
        results[initial].push_back(line);
    }
}

int main() {
    BlockingQueue<std::string> queue;
    std::map<char, std::vector<std::string>> results;
    std::mutex results_mutex;

    std::string input_file = "contacts.txt";
    std::string output_file = "output.txt";


    std::thread producer_thread(producer, std::ref(queue), std::ref(input_file));

    const int num_consumers = 3;
    std::vector<std::thread> consumer_threads;
    for (int i = 0; i < num_consumers; ++i) {
        consumer_threads.emplace_back(consumer, std::ref(queue), std::ref(results), std::ref(results_mutex));
    }

    producer_thread.join();
    for (auto& thread : consumer_threads) {
        thread.join();
    }

    std::ofstream outfile(output_file);
    if (!outfile.is_open()) {
        std::cerr << "Error opening output file: " << output_file << std::endl;
        return 1;
    }

    for (const auto& [key, group] : results) {
        outfile << "Group " << key << ":\n";
        for (const auto& name : group) {
            outfile << "  " << name << "\n";
        }
    }

    outfile.close();

    return 0;
}
