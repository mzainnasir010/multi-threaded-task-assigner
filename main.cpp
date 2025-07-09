#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <vector>
#include <list>
#include <string>
#include <functional>
#include <random>

using namespace std;

struct Resource {
    int bricks;
    int cement;
    int tools;
};

struct Task {
    string name;
    int bricksRequired;
    int cementRequired;
    int toolsRequired;
    int priority;

    bool operator<(const Task& other) const {
        return priority > other.priority;
    }
};

struct Worker {
    string name;
    int proficiency;
    bool isOnBreak;
};

enum WeatherCondition { CLEAR, RAINY, STORMY };

Resource siteResources = { 100, 50, 10 };
mutex resourceMutex, taskMutex, workerMutex;
condition_variable cv;

priority_queue<Task> taskQueue;
// Worker pool and LRU for break management
list<Worker> workerPool = {
    {"Ali", 10, false},
    {"Bilal", 10, false},
    {"Mujtaba", 10, false},
    {"Afaq", 10, false},
    {"Usman", 10, false},
    {"Abdullah", 10, false},
    {"Afnan", 10, false},
    {"Shahzaib", 10, true},
};
deque<string> lruQueue;

//Function definition all the required function here
void displayResources();
bool checkWeather();
void addTask();
void assignTask();
void addTask();
void userMenu();
void displayWorkers();
void simulateWorkerReturn();
void addWorker();
void removeWorker();
void userMenu();

int main()
{
    userMenu();
    return 0;
}
//function definitions here
// Functions for user interaction
void displayResources() {
    lock_guard<mutex> lock(resourceMutex);
    cout << "\n-----------------\nResources Status\n-----------------\nBricks: " << siteResources.bricks
        << ", Cement: " << siteResources.cement
        << ", Tools: " << siteResources.tools << endl;
}

//function for checking weather condition
bool checkWeather()
{
    random_device rd;
    mt19937 gen(rd());
    // here 0: Clear, 1: Rainy, 2: Stormy
    uniform_int_distribution<> weatherDist(0, 2);
    WeatherCondition currentWeather = static_cast<WeatherCondition>(weatherDist(gen));
    if (currentWeather == RAINY)
    {
        cout << "[Weather Update] The current weather is rainy. No work today." << endl;
        return false;
    }
    string weatherStr = (currentWeather == CLEAR) ? "Clear" : "Stormy";
    cout << "[Weather Update] The current weather is: " << weatherStr << ". Work can proceed." << endl;
    return true;
}

//function to add new task/work
void addTask() {
    string name;
    int bricks, cement, tools, priority;

    cout << "Enter Task Name: ";
    cin >> name;
    cout << "Enter Bricks Required: ";
    cin >> bricks;
    cout << "Enter Cement Required: ";
    cin >> cement;
    cout << "Enter Tools Required: ";
    cin >> tools;
    cout << "Enter Task Priority (lower is higher priority): ";
    cin >> priority;

    Task newTask = { name, bricks, cement, tools, priority };
    lock_guard<mutex> lock(taskMutex);
    taskQueue.push(newTask);

    cout << "[Task Added] " << name << " (Bricks=" << bricks
        << ", Cement=" << cement << ", Tools=" << tools
        << ", Priority=" << priority << ")" << endl;

    cv.notify_all();
}


void assignTask() {
    // Check the weather before assigning the task
    bool status = checkWeather();
    if (!status) {
        cout << "[Task Assignment] Work cannot proceed today due to weather conditions." << endl;
        return;
    }
    // Display available resources
    displayResources();

    if (taskQueue.empty()) {
        cout << "\n[Info] No tasks available to assign." << endl;
        return;
    }
    // Lock for task queue
    lock_guard<mutex> taskLock(taskMutex);
    vector<Task> retryQueue;

    // Process tasks until queue is empty
    while (!taskQueue.empty()) {
        Task currentTask = taskQueue.top();
        taskQueue.pop();
        if (currentTask.bricksRequired > siteResources.bricks ||
            currentTask.cementRequired > siteResources.cement ||
            currentTask.toolsRequired > siteResources.tools) {
            // If not enough resources, add the task to the retry queue
            retryQueue.push_back(currentTask);
            cout << "[Deadlock Prevention] Task '" << currentTask.name
                 << "' cannot proceed due to insufficient resources. It will be retried later." << endl << endl;
            continue;
        }
        int bricksRemaining = currentTask.bricksRequired;
        int cementRemaining = currentTask.cementRequired;
        int toolsRemaining = currentTask.toolsRequired;
        auto workerIt = workerPool.begin(); 

        // Track resources allocated to workers (for returning later)
        int bricksAllocated = 0;
        int cementAllocated = 0;
        int toolsAllocated = 0;

        // Create a sorted list of workers based on proficiency (descending order)
        vector<Worker*> sortedWorkers;
        for (auto& worker : workerPool) {
            if (!worker.isOnBreak) {
                sortedWorkers.push_back(&worker);
            }
        }
        sort(sortedWorkers.begin(), sortedWorkers.end(), [](Worker* a, Worker* b) {
            return a->proficiency > b->proficiency;
        });

        cout << "TASK NAME : " << currentTask.name << endl;
        while (bricksRemaining > 0 || cementRemaining > 0 || toolsRemaining > 0) {
            if (sortedWorkers.empty()) {
                cout << "[Error] No available workers to complete the task." << endl << endl;
                taskQueue.push(currentTask); // Re-add the task to the queue
                return;
            }

            // Get the worker with the highest proficiency
            Worker* worker = sortedWorkers.front();
            lock_guard<mutex> resLock(resourceMutex);
            // Allocate one unit of each resource to the worker if available
            if (bricksRemaining > 0) {
                siteResources.bricks -= 1;
                bricksRemaining -= 1;
                bricksAllocated += 1;
                worker->proficiency -= 1;
                cout << "[Resource Allocation] Worker " << worker->name
                     << " takes 1 brick. Remaining bricks for task: " << bricksRemaining << endl;
            }

            if (cementRemaining > 0) {
                siteResources.cement -= 1;
                cementRemaining -= 1;
                cementAllocated += 1; 
                worker->proficiency -= 1;
                cout << "[Resource Allocation] Worker " << worker->name
                     << " takes 1 cement. Remaining cement for task: " << cementRemaining << endl;
            }

            if (toolsRemaining > 0) {
                siteResources.tools -= 1;
                toolsRemaining -= 1;
                toolsAllocated += 1; 
                worker->proficiency -= 1;
                cout << "[Resource Allocation] Worker " << worker->name
                     << " takes 1 tool. Remaining tools for task: " << toolsRemaining << endl << endl;
            }
            // If proficiency reaches 0, move the worker to break
            if (worker->proficiency <= 0) {
                worker->isOnBreak = true;
		lruQueue.push_back(worker->name);
            }
            sortedWorkers.push_back(sortedWorkers.front()); 
            sortedWorkers.erase(sortedWorkers.begin());     
        }
        cout << "\n[Task Assignment] Task '" << currentTask.name << "' is now fully assigned and completed!" << endl << endl;

        // Return allocated resources to the pool
        {
            lock_guard<mutex> resLock(resourceMutex);
            siteResources.bricks += bricksAllocated;
            siteResources.cement += cementAllocated;
            siteResources.tools += toolsAllocated;

            cout << "[Resource Return] Workers have returned their resources: "
                 << bricksAllocated << " bricks, " << cementAllocated
                 << " cement, " << toolsAllocated << " tools." << endl << endl;
        }
    }
    
    for (const auto& task : retryQueue) {
        taskQueue.push(task);
        cout << "\n[Info] Retrying Task '" << task.name << "' due to resource availability." << endl;
    }
}


void simulateWorkerReturn() {
    lock_guard<mutex> lock(workerMutex);

    //Condition to check if there are workers on break
    if (!lruQueue.empty()) {
        string returningWorkerName = lruQueue.front();
        lruQueue.pop_front();

        auto workerIt = find_if(workerPool.begin(), workerPool.end(),
            [&](Worker& w) { return w.name == returningWorkerName; });

        if (workerIt != workerPool.end())
        {
            workerIt->isOnBreak = false;
            workerIt->proficiency = 10;

            cout << "[Worker Return] Worker " << workerIt->name << " is now available." << endl;
        }
        else {
            cout << "[Error] Worker not found in the pool." << endl;
        }
    }
    else {
        cout << "[Info] No workers on break to return." << endl;
    }
}

//function to display workers
void displayWorkers() {
    lock_guard<mutex> lock(workerMutex);
    cout << "\n--------------------\n Available Workers\n--------------------\n " << endl;
    int i = 1;
    cout<<"  Worker Name\tWorker Proficiency"<<endl<<endl;
    for (auto& worker : workerPool)
    {
        if (!worker.isOnBreak)
        {
            cout << i << ". " << worker.name<<"         \t"<<worker.proficiency<<endl;
            i++;
        }

    }
    i = 1;
    cout << "\n--------------------\nWorkers on Break\n--------------------\n " << endl;
    for (auto& worker : workerPool)
    {
        if (worker.isOnBreak)
	{
	    cout << i << ". " << worker.name<<"         \t"<<worker.proficiency<<endl;
            i++;
        }
    }
}

void addWorker() {
    string name;
    int proficiency;

    cout << "Enter Worker Name: ";
    cin >> name;
    cout << "Enter Worker Proficiency (higher is better): ";
    cin >> proficiency;

    Worker newWorker = { name, proficiency, false };
    lock_guard<mutex> lock(workerMutex);
    workerPool.push_back(newWorker);
    cout << "[Worker Added] " << name << endl;
}

void removeWorker() {
    string name;

    cout << "Enter Worker Name to Remove: ";
    cin >> name;

    lock_guard<mutex> lock(workerMutex);
    auto it = find_if(workerPool.begin(), workerPool.end(), [&](Worker& w) { return w.name == name; });

    if (it != workerPool.end()) {
        workerPool.erase(it);
        cout << "[Worker Removed] " << name << endl;
    }
    else {
        cout << "[Error] Worker not found!" << endl;
    }
}

//Main menu for user interaction
void userMenu() {
    while (true) {
        cout << "\n[Menu]\n";
        cout << "1. Display Resources\n";
        cout << "2. Add Task\n";
        cout << "3. Assign Task\n";
        cout << "4. Simulate Worker Return\n";
        cout << "5. Display Workers\n";
        cout << "6. Add Worker\n";
        cout << "7. Remove Worker\n";
        cout << "8. Exit\n";
        cout << "Enter your choice: ";
        int choice;
        cin >> choice;

        switch (choice) {
        case 1:
            displayResources();
            system("read -p 'Press any key to continue...' var");
            system("clear");
            break;
        case 2:
            addTask();
            system("read -p 'Press any key to continue...' var");
            system("clear");
            break;
        case 3:
            assignTask();
            system("read -p 'Press any key to continue...' var");
            system("clear");
            break;
        case 4:
            simulateWorkerReturn();
            system("read -p 'Press any key to continue...' var");
            system("clear");
            break;
        case 5:
            displayWorkers();
            system("read -p 'Press any key to continue...' var");
            system("clear");
            break;
        case 6:
            addWorker();
            system("read -p 'Press any key to continue...' var");
            system("clear");
            break;
        case 7:
            removeWorker();
            system("read -p 'Press any key to continue...' var");
            system("clear");
            break;
        case 8:
            cout << "Exiting program..." << endl;
            return;
        default:
            cout << "Invalid choice. Please try again." << endl;
            system("read -p 'Press any key to continue...' var");
            system("clear");
        }
    }
}
