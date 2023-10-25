
/* The code is 
 * Copyright(c) 2018-2019 Yiqing Huang, <yqhuang@uwaterloo.ca>.
 *
 * This software may be freely redistributed under the terms of the X11 License.
 */
/**
 * @brief  stack to push/pop integer(s), API header  
 * @author yqhuang@uwaterloo.ca
 */

struct int_stack;
struct RECV_BUF;

int sizeof_shm_stack(int size);
int init_shm_stack(struct int_stack *p, int stack_size);
struct int_stack *create_stack(int size);
void destroy_stack(struct int_stack *p);
int is_full(struct int_stack *p);
int is_empty(struct int_stack *p);
int push(struct int_stack *p, struct RECV_BUF item);
int pop(struct int_stack *p, struct RECV_BUF *p_item);
