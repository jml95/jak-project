#include "SystemThread.h"

#include "common/log/log.h"

//////////////////////
// Thread Manager   //
//////////////////////

/*!
 * Create a new thread with the given name.
 */
SystemThread& SystemThreadManager::create_thread(const std::string& name) {
  if (thread_count >= MAX_SYSTEM_THREADS) {
    lg::die("Out of System Threads! MAX_SYSTEM_THREADS is ", MAX_SYSTEM_THREADS);
    ASSERT(false);
  }
  auto& thread = threads[thread_count];

  // reset thread
  thread.initialization_complete = false;
  thread.name = name;
  thread.id = thread_count;
  thread.manager = this;
  thread_count++;

  return thread;
}

/*!
 * Print the CPU usage statistics for all threads.
 */
void SystemThreadManager::print_stats() {
  double total_user = 0, total_kernel = 0;
  printf("%8s | %5s | %5s\n", "Name", "User", "Kernel");
  printf("--------------------------\n");
  for (int id = 0; id < thread_count; id++) {
    auto& thread = threads[id];
    printf("%8s | %5.1f | %5.1f\n", thread.name.c_str(), thread.cpu_user * 100.,
           thread.cpu_kernel * 100.);
    total_kernel += thread.cpu_kernel;
    total_user += thread.cpu_user;
  }
  printf("%8s | %5.1f | %5.1f\n\n", "#TOTAL#", total_user * 100., total_kernel * 100.);
}

/*!
 * Request all threads to stop
 */
void SystemThreadManager::shutdown() {
  for (int i = 0; i < thread_count; i++) {
    lg::debug("# Stop {}", threads[i].name.c_str());
    threads[i].stop();
  }
}

/*!
 * Join all threads, if they are running
 */
void SystemThreadManager::join() {
  for (int i = 0; i < thread_count; i++) {
    lg::debug(" # Join {}", threads[i].name.c_str());
    if (threads[i].running) {
      threads[i].join();
    }
  }
}

/*!
 * Return true if all threads are exiting (manager shutdown)
 */
bool SystemThreadManager::all_threads_exiting() {
  for (int i = 0; i < thread_count; i++) {
    if (!threads[i].want_exit)
      return false;
  }
  return true;
}

/*!
 * bootstrap function to call a SystemThread's function
 */
void* bootstrap_thread_func(void* x) {
  SystemThread* thd = (SystemThread*)x;
  SystemThreadInterface iface(thd);
  thd->function(iface);
  lg::debug("[SYSTEM] Thread {} is returning", thd->name.c_str());
  return nullptr;
}

/*!
 * Start a thread and wait for its initialization
 */
void SystemThread::start(std::function<void(SystemThreadInterface&)> f) {
  lg::debug("# Initialize {}...", name.c_str());

  function = f;
  thread = std::thread(bootstrap_thread_func, this);
  running = true;

  // and wait for initialization
  {
    std::unique_lock<std::mutex> mlk(initialization_mutex);
    while (!initialization_complete) {
      initialization_cv.wait(mlk);
    }
  }
}

/*!
 * Join a system thread
 */
void SystemThread::join() {
  thread.join();
  running = false;
}

/*!
 * Set flag in system thread so want_exit() returns true.
 */
void SystemThread::stop() {
  want_exit = true;
}

/*!
 * Signal from a thread that initialization has complete, and the caller of SystemThread::start()
 * will be unblocked.
 */
void SystemThreadInterface::initialization_complete() {
  std::unique_lock<std::mutex> mlk(thread.initialization_mutex);
  thread.initialization_complete = true;
  thread.initialization_cv.notify_all();
  lg::debug("# {} initialized", thread.name.c_str());
}

/*!
 * Should we try and exit?
 */
bool SystemThreadInterface::get_want_exit() const {
  return thread.want_exit;
}

/*!
 * Trigger a full system shutdown.
 */
void SystemThreadInterface::trigger_shutdown() {
  thread.manager->shutdown();
}
