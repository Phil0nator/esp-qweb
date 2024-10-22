#ifndef STATIC_CONTAINERS_H
#define STATIC_CONTAINERS_H

#include <stdlib.h>

#define __STC_EXPAND_ITEMS  (32)

/**
 * @brief Static vector capacity
 */
#define STC_VEC_CAP(vect)     vect##_cap

/**
 * @brief Static vector item count
 */
#define STC_VEC_CNT(vect)     vect##_cnt

#define __STC_VEC_EXPAND(vect)\
    STC_VEC_CAP(vect) += __STC_EXPAND_ITEMS;\
    vect = realloc( vect, sizeof(*vect) * STC_VEC_CAP(vect)  )

#define __STC_VEC_ENSURE_SIZE(vect)\
    if ((STC_VEC_CNT(vect) == STC_VEC_CAP(vect))) {  __STC_VEC_EXPAND(vect); }


/**
 * @brief Declare a static vector
 * @param type data type to store
 * @param name name of the static vector
 */
#define STC_VEC_DECL(type, name)\
    type* name = NULL;\
    size_t STC_VEC_CNT(name) = 0;\
    size_t STC_VEC_CAP(name) = 0


/**
 * @brief Push to a static vector
 * @param value value to push
 */
#define STC_VEC_PUSH(vect, value)\
    __STC_VEC_ENSURE_SIZE(vect);\
    vect[STC_VEC_CNT(vect)] = (value);\
    STC_VEC_CNT(vect)++;


/**
 * @brief Free resources held by unused capacity
 *  of a static vector. This will cause the capcity
 *  to be truncated to the item count.
 */
#define STC_VEC_CLEANUP(vect)\
    vect = realloc( vect, sizeof(*vect) * STC_VEC_CNT(vect) );\
    STC_VEC_CAP(vect) = STC_VEC_CNT(vect)


/**
 * @brief Pop from a static vector
 * @returns Either the popped item, or if the vector 
 * is empty, the uninitialized 0th item (Unsafe).
 */
#define STC_VEC_POP(vect)\
    (STC_VEC_CNT(vect)) ? (vect[--STC_VEC_CNT(vect)]) : (vect[0])
    
/**
 * @brief Remove an item from a static vector
 * @param index the index of the item to be removed
 */
#define STC_VEC_REMOVE(vect, index) do {\
        for (size_t __stc_vec_rmv_i = (index); (__stc_vec_rmv_i + 1) < STC_VEC_CNT(vect);__stc_vec_rmv_i++ )\
            vect[__stc_vec_rmv_i] = vect[__stc_vec_rmv_i+1];\
        STC_VEC_CNT(vect) --;\
    } while(0);


/**
 * @brief For loop header for a 'for-each' structure 
 *  over a static vector
 * @param iname name for index iterator
 * @example
 * 
 *  STC_VEC_FOREACH(i, my_vec) {
 *      printf("%d\n", my_vec[i]);
 *  }
 * 
 */
#define STC_VEC_FOREACH(iname, vect)    for(size_t iname = 0; iname < STC_VEC_CNT(vect); iname++)


#endif