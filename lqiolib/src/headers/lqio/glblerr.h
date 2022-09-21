/* -*- c++ -*-
 * $Id: glblerr.h 15869 2022-09-20 09:05:46Z greg $
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * December 1995
 *
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 *----------------------------------------------------------------------
 */

#if     !defined(LQIO_GLBLERR_H)
#define LQIO_GLBLERR_H

#include "error.h"

namespace LQIO {

    extern struct error_message_type global_error_messages[];

    enum {
        FTL_INTERNAL_ERROR=1,
        FTL_NO_MEMORY,
	ERR_ARRIVAL_RATE,
        ERR_ASYNC_REQUEST_TO_WAIT,
        ERR_BAD_PATH_TO_JOIN,
        ERR_CANT_OPEN_DIRECTORY,
        ERR_CANT_OPEN_FILE,
        ERR_CYCLE_IN_ACTIVITY_GRAPH,
        ERR_CYCLE_IN_CALL_GRAPH,
        ERR_DUPLICATE_ACTIVITY_LVALUE,
        ERR_DUPLICATE_ACTIVITY_RVALUE,
        ERR_DUPLICATE_SEMAPHORE_ENTRY_TYPES,
        ERR_DUPLICATE_START_ACTIVITY,
        ERR_DUPLICATE_SYMBOL,
        ERR_FORK_JOIN_MISMATCH,
        ERR_HISTOGRAM_INVALID_MAX,
        ERR_HISTOGRAM_INVALID_MIN,
        ERR_INFINITE_SERVER,
        ERR_INVALID_ARGUMENT,
        ERR_INVALID_DECISION_TYPE,
        ERR_INVALID_FORWARDING_PROBABILITY,
        ERR_INVALID_GROUP_SHARE,
        ERR_INVALID_OR_BRANCH_PROBABILITY,
        ERR_INVALID_PARAMETER,
        ERR_INVALID_REPLY_DUPLICATE,
        ERR_INVALID_REPLY_FOR_SNR_ENTRY,
        ERR_INVALID_REPLY_FROM_BRANCH,
        ERR_INVALID_SHARE,
        ERR_IS_START_ACTIVITY,
        ERR_LQX_COMPILATION,
        ERR_LQX_EXECUTION,
        ERR_LQX_SPEX,
        ERR_LQX_VARIABLE_RESOLUTION,
        ERR_MISSING_ATTRIBUTE,
        ERR_MIXED_ENTRY_TYPES,
        ERR_MIXED_RWLOCK_ENTRY_TYPES,
        ERR_MIXED_SEMAPHORE_ENTRY_TYPES,
        ERR_NON_REF_THINK_TIME,
        ERR_NON_UNITY_REPLIES,
        ERR_NOT_A_CONSTANT,
        ERR_NOT_DEFINED,
        ERR_NOT_REACHABLE,
        ERR_NOT_RWLOCK_TASK,
        ERR_NOT_SEMAPHORE_TASK,
        ERR_NOT_SPECIFIED,
        ERR_NO_GROUP_SPECIFIED,
        ERR_NO_OBJECT,
        ERR_NO_QUANTUM_SCHEDULING,
        ERR_NO_REFERENCE_TASKS,
        ERR_NO_RWLOCK,
        ERR_NO_SEMAPHORE,
        ERR_NO_START_ACTIVITIES,
        ERR_OPEN_AND_CLOSED_CLASSES,
        ERR_OR_BRANCH_PROBABILITIES,
        ERR_REFERENCE_TASK_FORWARDING,
        ERR_REFERENCE_TASK_IS_INFINITE,
        ERR_REFERENCE_TASK_IS_RECEIVER,
        ERR_REFERENCE_TASK_OPEN_ARRIVALS,
        ERR_REFERENCE_TASK_REPLIES,
        ERR_REPLICATION,
        ERR_REPLICATION_PROCESSOR,
        ERR_REPLY_NOT_GENERATED,
        ERR_SRC_EQUALS_DST,
        ERR_TASK_ENTRY_COUNT,
        ERR_TASK_HAS_NO_ENTRIES,
        ERR_TOO_MANY_X,
        ERR_UNEXPECTED_ATTRIBUTE,
        ERR_WRONG_TASK_FOR_ENTRY,
        ADV_SPEX_UNUSED_RESULT_VARIABLE,
        ADV_TOO_MANY_GNUPLOT_VARIABLES,
        ADV_LQX_IMPLICIT_SOLVE,
        ADV_MESSAGES_DROPPED,
        WRN_ENTRY_HAS_NO_REQUESTS,
        WRN_ENTRY_TYPE_MISMATCH,
        WRN_INFINITE_MULTI_SERVER,
        WRN_INFINITE_SERVER_OPEN_ARRIVALS,
        WRN_INVALID_SPEX_RESULT_PHASE,
        WRN_MULTIPLE_SPECIFICATION,
        WRN_NON_CFS_PROCESSOR,
        WRN_NOT_USED,
        WRN_NO_SENDS_FROM_REF_TASK,
        WRN_NO_SPEX_OBSERVATIONS,
        WRN_PRAGMA_ARGUMENT_INVALID,
        WRN_PRAGMA_UNKNOWN,
        WRN_PRIO_TASK_ON_FIFO_PROC,
        WRN_PROCESSOR_HAS_NO_TASKS,
        WRN_QUANTUM_SCHEDULING,
        WRN_SCHEDULING_NOT_SUPPORTED,
        WRN_TASK_QUEUE_LENGTH,
        WRN_XXXX_TIME_DEFINED_BUT_ZERO,
        ERR_NOT_SUPPORTED,
        LSTGBLERRMSG=ERR_NOT_SUPPORTED                     /* Define LAST! */
    };
}
#endif
