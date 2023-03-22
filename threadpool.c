#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
// Lior Zadah - 318162930
/*
 * Here were going to create the thread pool by the arg were getting from the user.
 * Init the value of the p_thread. and the mutex.
 */
threadpool* create_threadpool(int num_threads_in_pool){
    if ((num_threads_in_pool > MAXT_IN_POOL) || (num_threads_in_pool <= 0)){ //check if the num is out of bounds
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        return NULL;
    }
    threadpool* threadP;
    threadP=(threadpool*)malloc(1*sizeof(threadpool));//create pool
    if(threadP == NULL){
        perror("malloc FAIL\n");
        return NULL;
    }
    threadP -> num_threads = num_threads_in_pool;
    threadP-> qsize = 0;
    threadP->qhead = NULL;
    threadP ->qtail = NULL;
    threadP-> shutdown = 0;
    threadP->dont_accept = 0;
    threadP ->threads = (pthread_t *)malloc(sizeof(pthread_t) *  threadP -> num_threads);
    if (threadP ->threads == NULL){
        perror("malloc FAIL");
        free(threadP);
        return NULL;
    }
    pthread_mutex_init(&(threadP-> qlock),NULL);
    pthread_cond_init(&(threadP ->q_not_empty),NULL);
    pthread_cond_init(&(threadP-> q_empty),NULL);

    for(int i=0 ; i < threadP->num_threads ; i++){
        if((pthread_create(&threadP->threads[i],NULL,do_work,threadP ) != 0)){
            perror("pthread creation FAIL");
            destroy_threadpool(threadP);
            return NULL;
        }
    }
    return threadP;
}
// this func is just like the requerment, lock and hendel all threads
void destroy_threadpool(threadpool* destroyme){
    pthread_mutex_lock(&(destroyme->qlock));
    destroyme->dont_accept = 1;
    if(destroyme->qsize != 0){ // wait
        pthread_cond_wait(&(destroyme->q_empty),&(destroyme->qlock));
    }
    destroyme->shutdown = 1;
    pthread_cond_broadcast(&(destroyme->q_not_empty));
    pthread_mutex_unlock(&(destroyme->qlock));

    for(int i=0 ; i<destroyme->num_threads ; i++){
        pthread_join(destroyme->threads[i],NULL);
    }
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_empty));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    free(destroyme->threads);
    free(destroyme);
}
//the dispatch connect between the threads and the routine. the do work func

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    pthread_mutex_lock(&(from_me->qlock));
    if(from_me->dont_accept ==1){ //after locking check if dont accept is on
        pthread_mutex_unlock(&(from_me->qlock));
        return;
    }
    work_t *work = (work_t *)malloc(sizeof(work_t)); //malloc the work
    if (work == NULL){
        perror("malloc FAiL");
        destroy_threadpool(from_me);
        return;
    }
    work -> routine = dispatch_to_here; // attach func to riutine
    work-> arg = arg; // argof func
    work ->next = NULL;
    if (from_me->qsize == 0){ //in case the queue is empty
        from_me->qhead = work;
        from_me->qtail = work;
    }else{
        from_me->qtail->next = work;
        from_me->qtail = work;
    }
    from_me->qsize++;
    pthread_mutex_unlock(&(from_me->qlock)); // unlock mutex
    pthread_cond_signal(&(from_me->q_not_empty)); // alert all not empty
}

// setting the routine just like the dimmand in the ex page.

void* do_work(void* p){
    if(p==NULL){
        return NULL;
    }
    threadpool* pool=(threadpool*)p; // casting from void* to threadpool
    while(1){
        pthread_mutex_lock(&(pool->qlock));//locking
        if(pool->shutdown == 1){
            pthread_mutex_unlock(&(pool->qlock));//unlock when need to destroy
            return NULL;
        }

        while(pool -> qsize == 0){
            pthread_cond_wait(&(pool->q_not_empty),&(pool->qlock)); //sent the threads to sleep
            if(pool->shutdown==1){
                pthread_mutex_unlock(&(pool->qlock));
                return NULL;
            }
        }
        work_t *WORK_Q_head;
        if(pool->qsize == 0)
            return NULL;
        WORK_Q_head = pool->qhead; // pic the first work
        pool->qhead = pool->qhead->next;//
        pool->qsize--;
        if(pool->qsize == 0)
            pool->qtail = NULL;
        if(pool->qsize == 0 && pool->dont_accept == 1)
            pthread_cond_signal(&(pool->q_empty)); // alert the queue is empty
        pthread_mutex_unlock(&(pool->qlock));

        WORK_Q_head->routine(WORK_Q_head->arg);
        free(WORK_Q_head);}
}
