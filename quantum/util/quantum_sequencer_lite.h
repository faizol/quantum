/*
** Copyright 2018 Bloomberg Finance L.P.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#ifndef BLOOMBERG_QUANTUM_SEQUENCER_LITE_H
#define BLOOMBERG_QUANTUM_SEQUENCER_LITE_H

#if __cplusplus >= 201402L
#define BLOOMBERG_QUANTUM_SEQUENCER_LITE_SUPPORT
#endif // __cplusplus >= 201402L

#ifdef BLOOMBERG_QUANTUM_SEQUENCER_LITE_SUPPORT

#include <quantum/quantum_dispatcher.h>
#include <quantum/interface/quantum_iqueue.h>
#include <quantum/interface/quantum_ithread_context_base.h>
#include <quantum/util/quantum_sequencer_lite_configuration.h>
#include <quantum/util/quantum_sequence_key_statistics.h>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace Bloomberg {
namespace quantum {

//==============================================================================================
//                                      class SequencerLite
//==============================================================================================
/// @class SequencerLite.
/// @brief Implementation of a key-based task sequencing with quantum.
/// @tparam SequenceKey Type of the key based that sequenced tasks are associated with
/// @tparam Hash Hash-function used for storing instances of SequenceKey in hash maps
/// @tparam KeyEqual The equal-function used for storing instances of SequenceKey in hash maps
/// @tparam Allocator The allocator used for storing instances of SequenceKey in hash maps
/// @note The interfaces of quantum::SequencerLite and quantum::Sequencer are the same. The major difference
/// between their functionalities is that quantum::SequencerLite does not rely on quantum::Dispatcher to order tasks 
/// based on their interdependence. In contrast, quantum::SequencerLite manages task ordering itself 
/// by construcing a DAG of pending tasks. In quantum::SequencerLite, a task is pushed into quantum::Dispatcher 
/// only when it's ready to be executed (i.e. when it has no pending dependents). This typically results 
/// in executing scheduled tasks faster (w.r.t. quantum::Dispatcher) and wasting fewer CPU cycles in 
/// quantum::Dispatcher.

template <class SequenceKey,
          class Hash = std::hash<SequenceKey>,
          class KeyEqual = std::equal_to<SequenceKey>,
          class Allocator = std::allocator<std::pair<const SequenceKey, SequencerLiteKeyData<SequenceKey>>>>
class SequencerLite
{
public:
    /// @brief Configuration class for SequencerLite
    using Configuration = SequencerLiteConfiguration<SequenceKey, Hash, KeyEqual, Allocator>;

    /// @brief Constructor.
    /// @param[in] dispatcher Dispatcher for all task dispatching
    /// @param[in] configuration the configuration object
    SequencerLite(Dispatcher& dispatcher, const Configuration& configuration = Configuration());

    /// @brief Enqueue a coroutine to run asynchronously.
    /// @details This method will post the coroutine on any thread available and will run when the previous coroutine
    ///          associated with the same 'sequenceKey' completes. If there are none, it will run immediately.
    ///          (@see Dispatcher::post for more details).
    /// @tparam FUNC Callable object type which will be wrapped in a coroutine with signature 'int(VoidContextPtr, Args...)'
    /// @tparam ARGS Argument types passed to FUNC (@see Dispatcher::post for more details).
    /// @param[in] sequenceKey SequenceKey object that the posted task is associated with
    /// @param[in] func Callable object.
    /// @param[in] args Variable list of arguments passed to the callable object.
    /// @note This function is non-blocking and returns immediately.
    /// @warning The VoidContextPtr can be used to yield() or to post additional coroutines or IO tasks.
    ///          However it should *not* be set and this will result in undefined behavior.
    template <class FUNC, class ... ARGS>
    void
    enqueue(const SequenceKey& sequenceKey, FUNC&& func, ARGS&&... args);

    /// @brief Enqueue a coroutine to run asynchronously on a specific queue (thread).
    /// @details This method will post the coroutine on any thread available and will run when the previous coroutine
    ///          associated with the same 'sequenceKey' completes. If there are none, it will run immediately.
    ///          (@see Dispatcher::post for more details).
    /// @tparam FUNC Callable object type which will be wrapped in a coroutine with signature 'int(VoidContextPtr, Args...)'
    /// @tparam ARGS Argument types passed to FUNC (@see Dispatcher::post for more details).
    /// @param[in] queueId Id of the queue where this coroutine should run. Note that the user can
    ///                    specify IQueue::QueueId::Any as a value, which is equivalent to running
    ///                    the simpler version of post() above. Valid range is [0, numCoroutineThreads) or
    ///                    IQueue::QueueId::Any.
    /// @param[in] isHighPriority If set to true, the sequencer coroutine will be scheduled right 
    ///                           after the currently executing coroutine on 'queueId'.
    /// @param[in] opaque pointer to opaque data that is passed to the exception handler (if provided)
    ///            if an unhandled exception is thrown in func
    /// @param[in] sequenceKey SequenceKey object that the posted task is associated with
    /// @param[in] func Callable object.
    /// @param[in] args Variable list of arguments passed to the callable object.
    /// @note This function is non-blocking and returns immediately.
    /// @warning The VoidContextPtr can be used to yield() or to post additional coroutines or IO tasks.
    ///          However it should *not* be set and this will result in undefined behavior.
    template <class FUNC, class ... ARGS>
    void
    enqueue(void* opaque, int queueId, bool isHighPriority, const SequenceKey& sequenceKey, FUNC&& func, ARGS&&... args);

    /// @brief Enqueue a coroutine to run asynchronously.
    /// @details This method will post the coroutine on any thread available and will run when the previous coroutine(s)
    ///          associated with all the 'sequenceKeys' complete. If there are none, then it will run immediately.
    ///          (@see Dispatcher::post for more details).
    /// @tparam FUNC Callable object type which will be wrapped in a coroutine with signature 'int(VoidContextPtr, Args...)'
    /// @tparam ARGS Argument types passed to FUNC (@see Dispatcher::post for more details).
    /// @param[in] sequenceKeys A collection of sequenceKey objects that the posted task is associated with
    /// @param[in] func Callable object.
    /// @param[in] args Variable list of arguments passed to the callable object.
    /// @note This function is non-blocking and returns immediately.
    /// @warning The VoidContextPtr can be used to yield() or to post additional coroutines or IO tasks.
    ///          However it should *not* be set and this will result in undefined behavior.
    template <class FUNC, class ... ARGS>
    void
    enqueue(const std::vector<SequenceKey>& sequenceKeys, FUNC&& func, ARGS&&... args);

    /// @brief Enqueue a coroutine to run asynchronously on a specific queue (thread).
    /// @details This method will post the coroutine on any thread available and will run when the previous coroutine(s)
    ///          associated with all the 'sequenceKeys' complete. If there are none, then it will run immediately.
    ///          (@see Dispatcher::post for more details).
    /// @tparam FUNC Callable object type which will be wrapped in a coroutine with signature 'int(VoidContextPtr, Args...)'
    /// @tparam ARGS Argument types passed to FUNC (@see Dispatcher::post for more details).
    /// @param[in] queueId Id of the queue where this coroutine should run. Note that the user 
    ///                    can specify IQueue::QueueId::Any as a value, which is equivalent to running 
    ///                    the simpler version of post() above. Valid range is [0, numCoroutineThreads) or 
    ///                    IQueue::QueueId::Any.
    /// @param[in] isHighPriority If set to true, the sequencer coroutine will be scheduled right 
    ///                           after the currently executing coroutine on 'queueId'.
    /// @param[in] opaque pointer to opaque data that is passed to the exception handler (if provided)
    ///            if an unhandled exception is thrown in func
    /// @param[in] sequenceKeys A collection of sequenceKey objects that the posted task is associated with
    /// @param[in] func Callable object.
    /// @param[in] args Variable list of arguments passed to the callable object.
    /// @note This function is non-blocking and returns immediately.
    /// @warning The VoidContextPtr can be used to yield() or to post additional coroutines or IO tasks.
    ///          However it should *not* be set and this will result in undefined behavior.
    template <class FUNC, class ... ARGS>
    void
    enqueue(void* opaque,
            int queueId,
            bool isHighPriority,
            const std::vector<SequenceKey>& sequenceKeys,
            FUNC&& func,
            ARGS&&... args);

    /// @brief Enqueue a coroutine to run asynchronously after all keys have run.
    /// @details This method will post the coroutine on any thread available. The posted task is assumed to be associated
    ///          with the entire universe of sequenceKeys already running or pending, which means that it will wait
    ///          until all tasks complete. This task can be considered as having a 'universal' key.
    /// @tparam FUNC Callable object type which will be wrapped in a coroutine with signature 'int(VoidContextPtr, Args...)'
    /// @tparam ARGS Argument types passed to FUNC (@see Dispatcher::post for more details).
    /// @param[in] func Callable object.
    /// @param[in] args Variable list of arguments passed to the callable object.
    /// @note This function is non-blocking and returns immediately.
    /// @warning The VoidContextPtr can be used to yield() or to post additional coroutines or IO tasks.
    ///          However it should *not* be set and this will result in undefined behavior.
    template <class FUNC, class ... ARGS>
    void
    enqueueAll(FUNC&& func, ARGS&&... args);

    /// @brief Enqueue a coroutine to run asynchronously on a specific queue (thread), after all keys have run.
    /// @details This method will post the coroutine on any thread available. The posted task is assumed to be associated
    ///          with the entire universe of sequenceKeys already running or pending, which means that it will wait
    ///          until all tasks complete. This task can be considered as having a 'universal' key.
    /// @tparam FUNC Callable object type which will be wrapped in a coroutine with signature 'int(VoidContextPtr, Args...)'
    /// @tparam ARGS Argument types passed to FUNC (@see Dispatcher::post for more details).
    /// @param[in] queueId Id of the queue where this coroutine should run. Note that the user 
    ///                    can specify IQueue::QueueId::Any as a value, which is equivalent to running 
    ///                    the simpler version of post() above. Valid range is [0, numCoroutineThreads) or 
    ///                    IQueue::QueueId::Any.
    /// @param[in] isHighPriority If set to true, the sequencer coroutine will be scheduled right 
    ///                           after the currently executing coroutine on 'queueId'.
    /// @param[in] opaque pointer to opaque data that is passed to the exception handler (if provided)
    ///            if an unhandled exception is thrown in func
    /// @param[in] func Callable object.
    /// @param[in] args Variable list of arguments passed to the callable object.
    /// @note This function is non-blocking and returns immediately.
    /// @warning The VoidContextPtr can be used to yield() or to post additional coroutines or IO tasks.
    ///          However it should *not* be set and this will result in undefined behavior.
    template <class FUNC, class ... ARGS>
    void
    enqueueAll(void* opaque, int queueId, bool isHighPriority, FUNC&& func, ARGS&&... args);

    /// @brief Trims the sequence keys not used by the sequencer anymore.
    /// @details It's recommended to call this function periodically to clean up state sequence keys.
    /// @remark This call clears all the statistics for trimmed keys.
    /// @return The number of sequenceKeys after the trimming.
    size_t trimSequenceKeys();

    /// @brief Gets the number of tracked sequence keys
    /// @return sequence key count
    size_t getSequenceKeyCount();

    /// @brief Gets the sequencer statistics for a specific sequence key
    /// @param sequenceKey the key
    /// @return the statistics objects for the specified key
    /// @note This function blocks until the statistics computation job posted to the dispatcher is finished.
    SequenceKeyStatistics getStatistics(const SequenceKey& sequenceKey);

    /// @brief Gets the sequencer statistics for the 'universal key', a.k.a. posted via postAll() method.
    /// @return the statistics objects
    /// @note This function blocks until the statistics computation job posted to the dispatcher is finished.
    SequenceKeyStatistics getStatistics();

    /// @brief Gets the sequencer statistics for all jobs.
    /// @return the statistics objects
    /// @note The difference with the previous two statistics methods is that it aggregates stats on a per-task basis,
    ///       not on per-key basis.
    SequenceKeyStatistics getTaskStatistics();

    /// @brief Drains all sequenced tasks.
    /// @param[in] timeout Maximum time for this function to wait. Set to -1 to wait indefinitely until all sequences drain.
    /// @param[in] isFinal If set to true, the sequencer will not allow any more processing after the drain completes.
    /// @return True if everything drains before timeout, false otherwise.
    /// @note This function blocks until all sequences have completed. During this time, posting
    ///       of new tasks is disabled unless they are posted from within an already executing coroutine.
    ///       Since this function posts a task which will wait on all others, getStatistics().getPostedTaskCount()
    ///       will contain one extra count.
    bool drain(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1),
               bool isFinal = false);

private:
    using PendingTaskQueueMap = std::unordered_map<SequenceKey, SequencerLiteKeyData<SequenceKey>, Hash, KeyEqual, Allocator>;
    using ExceptionCallback = typename Configuration::ExceptionCallback;

    /// @brief Adds the task to the pending queue
    /// @param key the key associated with the queue to add to
    /// @param task the task to add
    /// @return true if the added task is the only enqueued task
    bool addPendingTask(const SequenceKey& key,
                        const std::shared_ptr<SequencerLiteTask<SequenceKey>>& task);

    /// @brief Adds the task to the universal pending queue
    /// @param task the task to add
    /// @return true if the added task is the only enqueued task
    bool addPendingTask(const std::shared_ptr<SequencerLiteTask<SequenceKey>>& task);

    /// @brief Schedules the task via the dispatcher
    /// @param task the task to schedule
    void scheduleTask(
        const std::shared_ptr<SequencerLiteTask<SequenceKey>>& task);

    /// @brief Removes the task from the pending queues and schedule next tasks
    /// @param task the task to remove
    void removePending(
        const std::shared_ptr<SequencerLiteTask<SequenceKey>>& task);

    /// @brief Removes the task from the pending queue
    /// @param entry the queue container
    /// @param task the task to remove
    /// @return next task to be scheduled
    std::shared_ptr<SequencerLiteTask<SequenceKey>> removePending(
        SequencerLiteKeyData<SequenceKey>& entry,
        const std::shared_ptr<SequencerLiteTask<SequenceKey>>& task);

    template <class FUNC, class ... ARGS>
    void
    enqueueSingle(void* opaque, int queueId, bool isHighPriority, const SequenceKey& sequenceKey, FUNC&& func, ARGS&&... args);

    template <class FUNC, class ... ARGS>
    void
    enqueueMultiple(void* opaque,
                    int queueId,
                    bool isHighPriority,
                    const std::vector<SequenceKey>& sequenceKeys,
                    FUNC&& func,
                    ARGS&&... args);

    template <class FUNC, class ... ARGS>
    void
    enqueueAllImpl(void* opaque, int queueId, bool isHighPriority, FUNC&& func, ARGS&&... args);

    template<class FUNC, class ...ARGS>
    static std::function<int(VoidContextPtr)> wrap(FUNC&& func, ARGS&&... args);

    Dispatcher&                  _dispatcher;
    std::atomic_bool             _drain;
    SequencerLiteKeyData<SequenceKey> _universalTaskQueue;
    PendingTaskQueueMap          _pendingTaskQueueMap;
    ExceptionCallback            _exceptionCallback;
    std::mutex                   _mutex;
    std::shared_ptr<SequenceKeyStatisticsWriter> _taskStats;
};

}}

#include <quantum/util/impl/quantum_sequencer_lite_impl.h>

#endif // BLOOMBERG_QUANTUM_SEQUENCER_LITE_SUPPORT

#endif //BLOOMBERG_QUANTUM_SEQUENCER_LITE_H
