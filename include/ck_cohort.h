/*
 * Copyright 2013 Samy Al Bahra.
 * Copyright 2013 Brendon Scheinman.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CK_COHORT_H
#define _CK_COHORT_H

/* This is an implementation of lock cohorts as described in:
 *     Dice, D.; Marathe, V.; and Shavit, N. 2012.
 *     Lock Cohorting: A General Technique for Designing NUMA Locks
 */

#include <ck_cc.h>
#include <ck_pr.h>
#include <stddef.h>

#define CK_COHORT_RELEASE_STATE_GLOBAL		0
#define CK_COHORT_RELEASE_STATE_LOCAL		1

#define CK_COHORT_DEFAULT_LOCAL_PASS_LIMIT	10

#define CK_COHORT_NAME(N) ck_cohort_##N
#define CK_COHORT_INSTANCE(N) struct CK_COHORT_NAME(N)
#define CK_COHORT_INIT(N, C, GL, LL, P) ck_cohort_##N##_init(C, GL, LL, P)
#define CK_COHORT_LOCK(N, C, GC, LC) ck_cohort_##N##_lock(C, GC, LC)
#define CK_COHORT_UNLOCK(N, C, GC, LC) ck_cohort_##N##_unlock(C, GC, LC)

#define CK_COHORT_PROTOTYPE(N, GT, GL, GU, LT, LL, LU)				\
 										\
	CK_COHORT_INSTANCE(N) {							\
		GT *global_lock;						\
		LT *local_lock;							\
		unsigned int release_state;					\
		unsigned int waiting_threads;					\
		unsigned int acquire_count;					\
		unsigned int local_pass_limit;					\
	};									\
										\
	CK_CC_INLINE static void						\
	ck_cohort_##N##_init(struct ck_cohort_##N *cohort,			\
	    GT *global_lock, LT *local_lock, unsigned int pass_limit)		\
	{									\
		ck_pr_store_ptr(&cohort->global_lock, global_lock);		\
		ck_pr_store_ptr(&cohort->local_lock, local_lock);		\
		ck_pr_store_uint(&cohort->release_state,			\
		    CK_COHORT_RELEASE_STATE_GLOBAL);				\
		ck_pr_store_uint(&cohort->waiting_threads, 0);			\
		ck_pr_store_uint(&cohort->acquire_count, 0);			\
		ck_pr_store_uint(&cohort->local_pass_limit, pass_limit);	\
		return;								\
	}									\
										\
	CK_CC_INLINE static void						\
	ck_cohort_##N##_lock(CK_COHORT_INSTANCE(N) *cohort,			\
	    void *global_context, void *local_context)				\
	{									\
		ck_pr_inc_uint(&cohort->waiting_threads);			\
		LL(cohort->local_lock, local_context);				\
		ck_pr_dec_uint(&cohort->waiting_threads);			\
										\
		if (cohort->release_state == CK_COHORT_RELEASE_STATE_GLOBAL) {	\
			GL(cohort->global_lock, global_context);		\
			cohort->release_state = CK_COHORT_RELEASE_STATE_LOCAL;	\
		}								\
										\
		++cohort->acquire_count;					\
		ck_pr_fence_memory();						\
		return;								\
	}									\
										\
	CK_CC_INLINE static void						\
	ck_cohort_##N##_unlock(CK_COHORT_INSTANCE(N) *cohort,			\
	    void *global_context, void *local_context)				\
	{									\
		if (ck_pr_load_uint(&cohort->waiting_threads) > 0		\
		    && cohort->acquire_count < cohort->local_pass_limit) {	\
			cohort->release_state = CK_COHORT_RELEASE_STATE_LOCAL;	\
		} else {							\
			GU(cohort->global_lock, global_context);		\
			cohort->release_state = CK_COHORT_RELEASE_STATE_GLOBAL;	\
			cohort->acquire_count = 0;				\
		}								\
										\
		ck_pr_fence_memory();						\
		LU(cohort->local_lock, local_context);				\
										\
		return;								\
	}


#define CK_COHORT_INITIALIZER							\
	{ .global_lock = NULL, .local_lock = NULL,				\
	    .release_state = CK_COHORT_RELEASE_STATE_GLOBAL,			\
	    .waiting_threads = 0, .acquire_count = 0,				\
	    .local_pass_limit = CK_COHORT_DEFAULT_LOCAL_PASS_LIMIT }


#endif /* _CK_COHORT_H */